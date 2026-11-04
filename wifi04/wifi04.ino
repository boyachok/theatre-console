#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include "wifi_transport.h"
#include <esp_timer.h>
#include <atomic>
#include <algorithm>
#include "protocol.h"
#include "dashboard.h"
#include "ui_common.h"
#include "diagnostics.h"
#include "uart_page.h"
#include "actors.h"

// Local WLAN control; pins and actor policy are unchanged.
constexpr uint8_t BROADCAST[6] = {255,255,255,255,255,255};
WifiTransport network;
Actors actors;
uint32_t lastControlId=0;
Result lastControlResult=APPLIED;
uint32_t probeReceivedAt=0;
WebServer web(80);
Preferences prefs;
std::atomic<uint32_t> &rxDrops=network.rxDrops;
uint32_t &txErrors=network.txErrors;
uint32_t bootSession=0,testStart=0,restartAt=0,networkEpochSeen=0;
String csrfToken;
bool otaActive=false,otaOK=false,radioReady=false;
uint32_t identifyUntil=0,debugUntil=0,nextDebugAt=0;
bool measurementsRunning=true;
uint32_t measurementStoppedAt=0;
Health slaveHealth=WARMUP;
uint8_t masterMAC[6]{};
bool masterKnown=false;
uint32_t masterSession=0, masterSeen=0, lastProbe=0, helloAt=0;
uint8_t assignedId=0;
uint32_t lastProbeSeq=0;

struct Rx { uint8_t mac[6]; Packet p; UartStats uart; int8_t rssi; uint32_t atUs; };
struct Sample {
  uint32_t seq=0, atUs=0, rttUs=0;
  int8_t rssiMaster=0, rssiSlave=0;
  bool used=false, received=false, expired=false, late=false;
};
struct Node {
  bool used=false; uint8_t mac[6]{}; String name;
  Sample samples[WINDOW];
  uint32_t seq=0,measureSeq=0,sent=0,received=0,deadlineReceived=0,timeouts=0,late=0,duplicates=0;
  uint32_t lastReply=0, lastSeen=0, nextPoll=0, streak=0, maxStreak=0;
  int8_t lastMasterRssi=0, lastSlaveRssi=0;
  bool hasReply=false, maintenance=false;
  ControlState reported{};uint32_t reportAt=0,reportUptime=0,deviceBoot=0,nextCommand=0;
  UartStats uart{};uint32_t uartAt=0,uartUptime=0;
  Packet pendingPacket{};bool pending=false;uint8_t attempts=0;
  uint32_t commandAt=0,retryAt=0;String commandStatus="none";
};
Node nodes[MAX_NODES];
struct Metrics {
  int n=0, replies=0, deadline=0, worstRssi=0;
  float p50=0, p95=0, maxRtt=0;
  Health health=WARMUP;
};
bool heartbeatFresh() {return network.ready()&&lastProbe&&millis()-probeReceivedAt<OFFLINE_MS;}
uint32_t freshSession(){uint32_t n=esp_random();return n?n:1;}
void networkSessionChanged() {
  networkEpochSeen=network.generation;bootSession=freshSession();
  lastProbe=lastProbeSeq=probeReceivedAt=helloAt=debugUntil=0;
  lastControlId=0;lastControlResult=APPLIED;masterKnown=false;assignedId=0;
  if(!RADIO_MASTER)actors.off(1);
  else for(auto &n:nodes)if(n.used){
    n.pending=false;n.commandStatus="network_changed";n.deviceBoot=0;n.reportAt=0;n.reportUptime=0;
    n.hasReply=false;n.uartAt=0;n.uartUptime=0;
  }
}
void expireSlaveLease() {
  if(RADIO_MASTER||!lastProbe||millis()-probeReceivedAt<OFFLINE_MS)return;
  // UDP may contain frames queued during a stalled HTTP handler. Rotate the target
  // identity BEFORE draining them so a delayed ARM cannot undo the local failsafe.
  actors.off(1);bootSession=freshSession();lastControlId=0;lastControlResult=APPLIED;
  lastProbe=lastProbeSeq=probeReceivedAt=debugUntil=0;
  if(masterKnown)helloAt=millis()+20+esp_random()%100;
}
const char *healthName(Health h) {
  const char *names[]={"warmup","excellent","good","marginal","bad","offline"};
  return names[h <= OFFLINE ? h : BAD];
}
String macString(const uint8_t *mac) {
  char s[18]; snprintf(s,sizeof(s),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]); return s;
}
String jsonString(String s) {
  String out="\"";
  for (unsigned int i=0;i<s.length();i++) {
    uint8_t c=s[i];
    if(c=='"'||c=='\\') {out+='\\';out+=(char)c;}
    else if(c<32) {char code[7];snprintf(code,sizeof(code),"\\u%04x",c);out+=code;}
    else out+=(char)c;
  }
  return out+'"';
}
bool decodeMessage(const LanMessage &message,Rx &r) {
  const uint8_t *data=message.payload;int len=message.length;
  if(len==(int)sizeof(Packet)) {
    memcpy(&r.p,data,sizeof(r.p));
    if(r.p.magic!=MAGIC||r.p.protocol!=PROTOCOL||r.p.size!=sizeof(Packet)||r.p.type==DEBUG_REPORT)return false;
  } else if(len==(int)sizeof(DebugPacket)) {
    DebugPacket p;memcpy(&p,data,sizeof(p));
    if(p.magic!=MAGIC||p.protocol!=PROTOCOL||p.size!=sizeof(p)||p.type!=DEBUG_REPORT)return false;
    r.p.type=p.type;r.p.session=p.session;r.p.deviceBoot=p.deviceBoot;r.p.uptime=p.uptime;r.p.nodeId=p.nodeId;
    r.uart=p.uart;
  } else return false;
  memcpy(r.mac,message.mac,6);r.rssi=message.rssi;r.atUs=message.atUs;return true;
}
bool sendPacket(const uint8_t *mac,const Packet &p) {
  return network.send(mac,(const uint8_t*)&p,sizeof(p));
}
void sendUartReport() {
  if(!masterKnown||!heartbeatFresh())return;
  DebugPacket p{};p.magic=MAGIC;p.protocol=PROTOCOL;p.type=DEBUG_REPORT;p.size=sizeof(p);
  p.session=masterSession;p.deviceBoot=bootSession;p.uptime=millis();p.nodeId=assignedId;p.uart=actors.player.diagnostics();
  network.send(masterMAC,(const uint8_t*)&p,sizeof(p));
}
void led(Health h) {
  uint32_t now=millis(); uint8_t r=0,g=0,b=0;
  if(identifyUntil && (int32_t)(identifyUntil-now)>0) {if((now/150)%2)r=g=b=20;}
  else switch(h) {
    case EXCELLENT:b=20;break;
    case GOOD:g=20;break;
    case MARGINAL:r=18;g=10;break;
    case BAD:r=20;break;
    case OFFLINE:if((now/300)%2)r=24;break;
    default:r=g=b=((now/700)%2)?4:0;
  }
  static uint32_t previous=0xffffffff; uint32_t color=(r<<16)|(g<<8)|b;
  if(previous!=color) {rgbLedWrite(RGB_PIN,r,g,b);previous=color;}
}
Metrics metrics(Node &n) {
  Metrics m;float times[WINDOW];int nt=0;
  uint32_t nowUs=(uint32_t)esp_timer_get_time();
  for(auto &s:n.samples) {
    if(!s.used)continue;
    bool timely=s.received && s.rttUs<=TIMEOUT_MS*1000;
    if(!timely && !s.expired && (uint32_t)(nowUs-s.atUs)<TIMEOUT_MS*1000)continue;
    m.n++;
    if(timely) {
      m.replies++; if(s.rttUs<=100000)m.deadline++;
      times[nt++]=s.rttUs/1000.0f;
      m.worstRssi=std::min(m.worstRssi,(int)s.rssiMaster);
      m.worstRssi=std::min(m.worstRssi,(int)s.rssiSlave);
    }
  }
  if(nt) {std::sort(times,times+nt);m.p50=times[(nt-1)/2];m.p95=times[(95*nt+99)/100-1];m.maxRtt=times[nt-1];}
  uint32_t age=n.hasReply?millis()-n.lastReply:millis()-testStart;
  m.health=evaluateHealth(m.n,m.replies,m.deadline,m.p95,m.worstRssi,age,n.streak);
  return m;
}
void saveNode(int i) {
  String mk="mac"+String(i),nk="name"+String(i);
  prefs.putBytes(mk.c_str(),nodes[i].mac,6);prefs.putString(nk.c_str(),nodes[i].name);
}
int findNode(const uint8_t *mac,bool create) {
  for(int i=0;i<MAX_NODES;i++)if(nodes[i].used&&!memcmp(nodes[i].mac,mac,6))return i;
  if(!create)return -1;
  for(int i=0;i<MAX_NODES;i++)if(!nodes[i].used) {
    Node &n=nodes[i];n.used=true;memcpy(n.mac,mac,6);n.name="Ведомый "+String(i+1);n.lastSeen=millis();n.nextPoll=millis()+50*i;
    saveNode(i);Serial.printf("Found node %d %s\n",i+1,macString(mac).c_str());return i;
  }
  return -1;
}
void resetStats() {
  testStart=millis();measurementStoppedAt=measurementsRunning?0:testStart;txErrors=0;rxDrops=0;network.rxInvalid=0;
  for(int i=0;i<MAX_NODES;i++) {
    Node &n=nodes[i]; for(auto &s:n.samples)s=Sample{};
    // Sequence numbers are NOT reused: outstanding old replies must not match new tests.
    n.sent=n.received=n.deadlineReceived=n.timeouts=n.late=n.duplicates=n.streak=n.maxStreak=0;
    n.hasReply=false;n.lastReply=0;n.measureSeq=n.seq;n.nextPoll=millis()+i*50;
  }
}
void setMeasurements(bool running) {
  if(measurementsRunning==running)return;
  measurementsRunning=running;
  if(running)resetStats();
  else {
    measurementStoppedAt=millis();
    // Do not turn requests in flight into artificial losses when pausing the test.
    for(auto &n:nodes)for(auto &s:n.samples)if(s.used&&!s.received&&!s.expired)s.used=false;
  }
}
const char *resultName(Result r) {
  const char *names[]={"accepted","not_armed/configure_while_disarmed","invalid","io_or_queue_error","audio_disabled","player_offline","stale","heartbeat_expired"};
  return names[r<=LEASE_EXPIRED?r:INVALID];
}
bool queueCommand(int i,Op op,uint8_t channel=0,uint16_t value=0) {
  Node &n=nodes[i];
  if(!network.ready()||!n.used||!n.deviceBoot||(uint32_t)network.peerIP(n.mac)==0||!validControl(op,channel,value))return false;
  if(n.pending&&op!=ALL_OFF&&op!=DISARM&&op!=AUDIO_STOP)return false;
  Packet p=packet(COMMAND,bootSession);p.nodeId=i+1;p.deviceBoot=n.deviceBoot;
  p.commandId=++n.nextCommand;if(!p.commandId)p.commandId=++n.nextCommand;
  p.op=op;p.channel=channel;p.value=value;
  n.pendingPacket=p;n.pending=true;n.attempts=1;n.commandAt=millis();n.retryAt=millis()+150;n.commandStatus="waiting_ack";
  sendPacket(n.mac,p);return true;
}
void commandTick() {
  for(auto &n:nodes)if(n.used&&n.pending) {
    uint32_t now=millis();
    if(now-n.commandAt>=850){n.pending=false;n.commandStatus="UNKNOWN: no ACK; may have executed";}
    else if(n.attempts<4&&(int32_t)(now-n.retryAt)>=0){sendPacket(n.mac,n.pendingPacket);n.attempts++;n.retryAt=now+150;}
  }
}
void handleMasterRx(const Rx &r) {
  if(r.p.session!=bootSession||!r.p.deviceBoot)return;
  if(r.p.type==DEBUG_REPORT) {
    int i=findNode(r.mac,false);if(i<0)return;Node &n=nodes[i];
    if(n.deviceBoot!=r.p.deviceBoot)return;
    if(!n.uartAt||(int32_t)(r.p.uptime-n.uartUptime)>=0){n.uart=r.uart;n.uartAt=millis();n.uartUptime=r.p.uptime;}
    return;
  }
  if(r.p.type!=HELLO&&r.p.type!=ECHO&&r.p.type!=COMMAND_ACK)return;
  int i=findNode(r.mac,r.p.type==HELLO);if(i<0)return;
  Node &n=nodes[i];n.lastSeen=millis();
  bool newBoot=n.deviceBoot!=r.p.deviceBoot;
  if(n.deviceBoot&&newBoot) {
    n.pending=false;n.commandStatus="session_changed";n.uartAt=0;n.uartUptime=0;
  }
  n.deviceBoot=r.p.deviceBoot;
  if(newBoot||!n.reportAt||(int32_t)(r.p.uptime-n.reportUptime)>=0) {
    n.reported=r.p.state;n.reportAt=millis();n.reportUptime=r.p.uptime;
  }
  if(n.pending&&r.p.commandId==n.pendingPacket.commandId) {
    n.pending=false;n.commandStatus=resultName((Result)r.p.result);
  }
  if(r.p.type!=ECHO)return;
  // Live state/heartbeat continues even when radio measurement is paused.
  n.hasReply=true;n.lastReply=millis();n.lastMasterRssi=r.rssi;n.lastSlaveRssi=r.p.rssi;n.maintenance=r.p.reserved;
  if(!measurementsRunning)return;
  Sample &s=n.samples[r.p.seq%WINDOW];
  if(!s.used||s.seq!=r.p.seq)return;
  if(s.received){n.duplicates++;return;}
  s.received=true;s.rttUs=r.atUs-s.atUs;s.rssiMaster=r.rssi;s.rssiSlave=r.p.rssi;
  if(s.rttUs<=TIMEOUT_MS*1000) {
    // A callback may have queued the response before the main task noticed it.
    if(s.expired && n.timeouts)n.timeouts--;
    n.received++;if(s.rttUs<=100000)n.deadlineReceived++;n.streak=0;
  }
  else {
    s.late=true;n.late++;
    if(!s.expired){s.expired=true;n.timeouts++;n.streak++;n.maxStreak=std::max(n.maxStreak,n.streak);}
  }
}
void handleSlaveRx(const Rx &r) {
  uint32_t now=millis();
  if(r.p.type==DISCOVER) {
    if(masterKnown&&memcmp(r.mac,masterMAC,6)&&now-masterSeen<15000)return;
    if(!masterKnown||memcmp(r.mac,masterMAC,6)||masterSession!=r.p.session) {
      bootSession=freshSession();
      memcpy(masterMAC,r.mac,6);masterKnown=true;masterSession=r.p.session;lastProbeSeq=0;
      actors.off(2);lastControlId=0;lastControlResult=APPLIED;
      assignedId=0;slaveHealth=WARMUP;lastProbe=0;probeReceivedAt=0;debugUntil=0;
    }
    masterSeen=now;
    helloAt=now+20+esp_random()%350;
    return;
  }
  if(!masterKnown||memcmp(r.mac,masterMAC,6)||r.p.session!=masterSession)return;
  if(r.p.type==PROBE) {
    // Sequence freshness prevents old packets restoring a stale LED grade.
    if(lastProbeSeq && (int32_t)(r.p.seq-lastProbeSeq)<=0)return;
    lastProbeSeq=r.p.seq;masterSeen=now;lastProbe=now;
    probeReceivedAt=now-(uint32_t)((uint32_t)esp_timer_get_time()-r.atUs)/1000;
    assignedId=r.p.nodeId;
    slaveHealth=r.p.health<=OFFLINE?(Health)r.p.health:BAD;
    Packet p=packet(ECHO,masterSession,r.p.seq);p.rssi=r.rssi;p.nodeId=assignedId;
    p.deviceBoot=bootSession;p.commandId=lastControlId;p.result=lastControlResult;p.state=actors.snapshot();
    sendPacket(masterMAC,p);
  } else if(r.p.type==COMMAND) {
    // Bind commands to BOTH boots. Delayed PLAY packets cannot replay after a slave reboot.
    if(r.p.deviceBoot!=bootSession||!r.p.commandId)return;
    Result result=STALE;
    if(r.p.commandId==lastControlId)result=lastControlResult;
    else if(newSequence(r.p.commandId,lastControlId)) {
      bool fresh=heartbeatFresh()&&(uint32_t)((uint32_t)esp_timer_get_time()-r.atUs)<500000;
      result=actors.apply((Op)r.p.op,r.p.channel,r.p.value,fresh);
      lastControlId=r.p.commandId;lastControlResult=result;
    }
    Packet reply=packet(COMMAND_ACK,masterSession);reply.deviceBoot=bootSession;
    reply.commandId=r.p.commandId;reply.result=result;reply.state=actors.snapshot();sendPacket(masterMAC,reply);
  } else if(r.p.type==DEBUG_REQUEST) {
    if(r.p.deviceBoot==bootSession){debugUntil=now+15000;nextDebugAt=now;}
  } else if(r.p.type==IDENTIFY) identifyUntil=now+5000;
  else if(r.p.type==MAINTENANCE)actors.off(3);
}
void linkTick() {
  expireSlaveLease();
  LanMessage message;int budget=32;
  while(budget--&&network.receive(message)) {
    Rx r{};
    if(!decodeMessage(message,r)){network.rxInvalid++;continue;}
    if(RADIO_MASTER)handleMasterRx(r);else handleSlaveRx(r);
  }
  uint32_t now=millis();
  if(RADIO_MASTER) {
    commandTick();
    static uint32_t discoverAt=0,sendAt=0;
    if(network.ready()&&(int32_t)(now-discoverAt)>=0) {sendPacket(BROADCAST,packet(DISCOVER,bootSession));discoverAt=now+2000;}
    uint32_t nowUs=(uint32_t)esp_timer_get_time();
    for(auto &n:nodes)if(n.used&&measurementsRunning)for(auto &s:n.samples) {
      if(s.used&&!s.received&&!s.expired&&(uint32_t)(nowUs-s.atUs)>=TIMEOUT_MS*1000) {
        s.expired=true;n.timeouts++;n.streak++;n.maxStreak=std::max(n.maxStreak,n.streak);
      }
    }
    // No wait-for-response loop. At most one new poll per 20 ms for fair airtime.
    if(network.ready()&&(int32_t)(now-sendAt)>=0) {
      static int cursor=0;
      for(int c=0;c<MAX_NODES;c++) {
        int i=cursor;cursor=(cursor+1)%MAX_NODES;Node &n=nodes[i];
        if(!n.used||(uint32_t)network.peerIP(n.mac)==0||(int32_t)(now-n.nextPoll)<0)continue;
        n.nextPoll=now+POLL_MS;Metrics m=metrics(n);
        Packet p=packet(PROBE,bootSession,++n.seq);p.nodeId=i+1;p.health=m.health;
        if(measurementsRunning) {
          Sample &s=n.samples[n.seq%WINDOW];s=Sample{};s.used=true;s.seq=n.seq;s.atUs=(uint32_t)esp_timer_get_time();n.measureSeq=n.seq;n.sent++;
        }
        sendPacket(n.mac,p);sendAt=now+20;break;
      }
    }
  } else {
    if(network.ready()&&helloAt&&(int32_t)(now-helloAt)>=0) {
      helloAt=0;Packet p=packet(HELLO,masterSession);p.deviceBoot=bootSession;p.state=actors.snapshot();
      p.commandId=lastControlId;p.result=lastControlResult;sendPacket(masterMAC,p);
    }
    if(debugUntil&&(int32_t)(debugUntil-now)>0&&(int32_t)(now-nextDebugAt)>=0){sendUartReport();nextDebugAt=now+1000;}
  }
}
String controlJSON(const ControlState &s) {
  String j="{\"armed\":"+String(s.armed?"true":"false")+",\"pwm\":[";
  for(int i=0;i<4;i++){if(i)j+=',';j+=s.pwm[i];}
  j+="],\"relay\":"+String(s.relayOn?"true":"false")+",\"relayMode\":"+String(s.relayMode)+",\"fault\":"+String(s.fault);
  j+=",\"audioEnabled\":"+String(s.audioEnabled?"true":"false")+",\"audioState\":"+String(s.audioState)+",\"audioVolume\":"+String(s.audioVolume)+",\"audioMode\":"+String(s.audioMode)+",\"audioEq\":"+String(s.audioEq)+",\"audioDrive\":"+String(s.audioDrive);
  j+=",\"audioTrack\":"+String(s.audioTrack)+",\"audioTracks\":"+String(s.audioTracks)+",\"audioAgeMs\":"+(s.audioAge==65535?String("null"):String(s.audioAge))+"}";
  return j;
}
String hexString(const uint8_t *bytes,uint8_t length) {
  String s;for(uint8_t i=0;i<length;i++){char b[4];snprintf(b,sizeof(b),"%02X",bytes[i]);if(i)s+=' ';s+=b;}return s;
}
String uartJSON(const UartStats &s) {
  String j="{\"txFrames\":"+String(s.txFrames)+",\"txBytes\":"+String(s.txBytes)+",\"rxBytes\":"+String(s.rxBytes)+",\"validFrames\":"+String(s.validFrames)+",\"replies\":"+String(s.replies);
  j+=",\"checksumErrors\":"+String(s.checksumErrors)+",\"lengthErrors\":"+String(s.lengthErrors)+",\"frameTimeouts\":"+String(s.frameTimeouts)+",\"loopbacks\":"+String(s.loopbacks)+",\"txFailures\":"+String(s.txFailures);
  j+=",\"txPin\":"+String(s.txPin)+",\"rxPin\":"+String(s.rxPin)+",\"rxLevel\":"+String(s.rxLevel)+",\"polling\":"+String(s.polling?"true":"false");
  j+=",\"txAgeMs\":"+(s.txAge==65535?String("null"):String(s.txAge))+",\"rxAgeMs\":"+(s.rxAge==65535?String("null"):String(s.rxAge))+",\"replyAgeMs\":"+(s.replyAge==65535?String("null"):String(s.replyAge));
  j+=",\"lastTx\":"+jsonString(hexString(s.lastTx,std::min(s.txLen,(uint8_t)8)))+",\"lastRx\":"+jsonString(hexString(s.lastRx,std::min(s.rxLen,(uint8_t)16)))+"}";return j;
}
String statusJSON(bool history) {
  String j;j.reserve(history?90000:10000);
  j="{\"version\":"+jsonString(RADIO_VERSION)+",\"role\":"+jsonString(RADIO_MASTER?"master":"slave")+",\"transport\":\"wifi\",\"channel\":"+String(WiFi.channel())+",\"ap\":"+jsonString(STAGE_WIFI_SSID)+",\"ssid\":"+jsonString(STAGE_WIFI_SSID)+",\"ip\":"+jsonString(WiFi.localIP().toString())+",\"hostname\":"+jsonString(network.hostname)+",\"mac\":"+jsonString(macString(network.mac))+",\"connected\":"+String(network.ready()?"true":"false")+",\"rxInvalid\":"+String(network.rxInvalid)+",\"token\":"+jsonString(csrfToken)+",\"rxDrops\":"+String(rxDrops.load())+",\"txErrors\":"+String(txErrors)+",\"testSeconds\":"+String(((measurementStoppedAt?measurementStoppedAt:millis())-testStart)/1000.0f,1);
  j+=",\"measuring\":"+String(measurementsRunning?"true":"false")+",\"nodes\":[";
  bool comma=false;
  if(RADIO_MASTER)for(int i=0;i<MAX_NODES;i++) {
    Node &n=nodes[i];if(!n.used)continue;Metrics m=metrics(n);if(comma)j+=',';comma=true;
    IPAddress ip=network.peerIP(n.mac);
    j+="{\"id\":"+String(i+1)+",\"name\":"+jsonString(n.name)+",\"mac\":"+jsonString(macString(n.mac))+",\"health\":"+jsonString(healthName(m.health));
    j+=",\"ip\":"+((uint32_t)ip?jsonString(ip.toString()):String("null"));
    j+=",\"windowN\":"+String(m.n)+",\"deliveryPct\":"+(m.n?String(100.0*m.replies/m.n,3):String("null"))+",\"deadlinePct\":"+(m.n?String(100.0*m.deadline/m.n,3):String("null"));
    j+=",\"p50\":"+(m.replies?String(m.p50,2):String("null"))+",\"p95\":"+(m.replies?String(m.p95,2):String("null"))+",\"maxRtt\":"+(m.replies?String(m.maxRtt,2):String("null"))+",\"spread\":"+(m.replies?String(m.p95-m.p50,2):String("null"));
    j+=",\"rssiMaster\":"+(n.hasReply?String(n.lastMasterRssi):String("null"))+",\"rssiSlave\":"+(n.hasReply?String(n.lastSlaveRssi):String("null"))+",\"worstRssi\":"+(m.replies?String(m.worstRssi):String("null"));
    j+=",\"ageMs\":"+(n.hasReply?String(millis()-n.lastReply):String("null"))+",\"streak\":"+String(n.streak)+",\"maxStreak\":"+String(n.maxStreak)+",\"sent\":"+String(n.sent)+",\"received\":"+String(n.received)+",\"timeouts\":"+String(n.timeouts)+",\"late\":"+String(n.late)+",\"duplicates\":"+String(n.duplicates)+",\"maintenance\":"+(n.maintenance?"true":"false");
    j+=",\"control\":"+controlJSON(n.reported)+",\"reportAgeMs\":"+(n.reportAt?String(millis()-n.reportAt):String("null"));
    j+=",\"deviceBoot\":"+String(n.deviceBoot)+",\"commandStatus\":"+jsonString(n.commandStatus)+",\"commandPending\":"+(n.pending?"true":"false")+",\"commandAttempts\":"+String(n.attempts);
    j+=",\"uart\":"+(n.uartAt?uartJSON(n.uart):String("null"))+",\"uartReportAgeMs\":"+(n.uartAt?String(millis()-n.uartAt):String("null"));
    uint32_t finalized=n.received+n.timeouts;
    j+=",\"totalDeliveryPct\":"+(finalized?String(100.0*n.received/finalized,4):String("null"))+",\"totalDeadlinePct\":"+(finalized?String(100.0*n.deadlineReceived/finalized,4):String("null"));
    if(history) {
      j+=",\"samples\":[";bool sc=false;
      for(uint32_t k=(n.measureSeq>=WINDOW?n.measureSeq-WINDOW+1:1);k<=n.measureSeq;k++) {
        Sample &s=n.samples[k%WINDOW];if(!s.used||s.seq!=k)continue;if(sc)j+=',';sc=true;
        j+="{\"seq\":"+String(s.seq)+",\"rttMs\":"+(s.received?String(s.rttUs/1000.0f,3):String("null"))+",\"expired\":"+(s.expired?"true":"false")+",\"late\":"+(s.late?"true":"false")+",\"rssiMaster\":"+(s.received?String(s.rssiMaster):String("null"))+",\"rssiSlave\":"+(s.received?String(s.rssiSlave):String("null"))+"}";
      }
      j+=']';
    }
    j+='}';
  }
  j+="]";
  if(!RADIO_MASTER)j+=",\"control\":"+controlJSON(actors.snapshot())+",\"uart\":"+uartJSON(actors.player.diagnostics());
  j+="}";return j;
}
bool validForm() {
  if(web.arg("token")==csrfToken)return true;
  web.send(403,"text/plain","Invalid form token");return false;
}
bool unsignedArg(const char *name,uint32_t maximum,uint32_t &value) {
  String s=web.arg(name);if(s.isEmpty())return false;value=0;
  for(unsigned int i=0;i<s.length();i++) {
    if(s[i]<'0'||s[i]>'9')return false;
    value=value*10+(s[i]-'0');if(value>maximum)return false;
  }
  return true;
}
void routes() {
  const char *headers[]={"Origin"};web.collectHeaders(headers,1);
  web.on("/",HTTP_GET,[] {
    web.sendHeader("Cache-Control","no-store");
    if(RADIO_MASTER)web.send_P(200,"text/html; charset=utf-8",DASHBOARD);
    else web.send(200,"text/html; charset=utf-8","<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><link rel='stylesheet' href='/ui.css'><header>Stage Slave 0.4.1</header><main><div class='panel'><a class='btn' href='/update'>OTA</a> <a class='btn' href='/api/status'>Статус</a></div></main>");
  });
  web.on("/ui.css",HTTP_GET,[] {web.sendHeader("Cache-Control","no-store");web.send_P(200,"text/css",UI_CSS);});
  web.on("/ui.js",HTTP_GET,[] {web.sendHeader("Cache-Control","no-store");web.send_P(200,"text/javascript; charset=utf-8",UI_JS);});
  web.on("/diagnostics",HTTP_GET,[] {web.sendHeader("Cache-Control","no-store");web.send_P(200,"text/html; charset=utf-8",DIAGNOSTICS);});
  web.on("/uart",HTTP_GET,[] {web.sendHeader("Cache-Control","no-store");web.send_P(200,"text/html; charset=utf-8",UART_PAGE);});
  web.on("/measure",HTTP_POST,[] {
    if(!validForm())return;uint32_t running;
    if(!RADIO_MASTER||!unsignedArg("running",1,running)){web.send(400);return;}
    setMeasurements(running);web.send(200,"text/plain",running?"Measurement started":"Measurement paused; safety heartbeat remains active");
  });
  web.on("/uart/request",HTTP_POST,[] {
    if(!validForm())return;uint32_t id;
    if(!RADIO_MASTER||!unsignedArg("id",MAX_NODES,id)||!id||!nodes[id-1].deviceBoot){web.send(400);return;}
    Packet p=packet(DEBUG_REQUEST,bootSession);p.deviceBoot=nodes[id-1].deviceBoot;
    bool ok=sendPacket(nodes[id-1].mac,p);web.send(ok?202:503,"text/plain",ok?"Requested":"Send failed");
  });
  web.on("/command",HTTP_POST,[] {
    if(!validForm())return;
    uint32_t id,op,channel,value;
    if(!RADIO_MASTER||!unsignedArg("id",MAX_NODES,id)||!id||!unsignedArg("op",UART_POLL,op)||op<ARM||!unsignedArg("channel",4,channel)||!unsignedArg("value",65535,value)||!validControl((Op)op,channel,value)){web.send(400,"text/plain","Invalid command");return;}
    bool ok=queueCommand(id-1,(Op)op,channel,value);web.send(ok?202:409,"text/plain",ok?"Queued; check ACK status":"Node not ready or command pending");
  });
  web.on("/stop-all",HTTP_POST,[] {
    if(!validForm())return;if(!RADIO_MASTER){web.send(400,"text/plain","Master only");return;}
    int count=0;for(int i=0;i<MAX_NODES;i++)if(queueCommand(i,ALL_OFF))count++;
    web.send(202,"text/plain","ALL OFF: "+String(count));
  });
  web.on("/api/status",HTTP_GET,[] {web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",statusJSON(web.hasArg("history")));});
  web.on("/reset",HTTP_POST,[] {if(!validForm())return;resetStats();web.send(200,"text/plain","OK");});
  web.on("/name",HTTP_POST,[] {
    if(!validForm())return;int i=web.arg("id").toInt()-1;String name=web.arg("name");
    if(!RADIO_MASTER||i<0||i>=MAX_NODES||!nodes[i].used||name.length()>64){web.send(400);return;}
    nodes[i].name=name;saveNode(i);web.send(200,"text/plain","OK");
  });
  web.on("/identify",HTTP_POST,[] {
    if(!validForm())return;int i=web.arg("id").toInt()-1;
    if(!RADIO_MASTER||i<0||i>=MAX_NODES||!nodes[i].used){web.send(400);return;}
    bool ok=sendPacket(nodes[i].mac,packet(IDENTIFY,bootSession));web.send(ok?200:503,"text/plain",ok?"Queued":"Send failed");
  });
  web.on("/maintenance",HTTP_POST,[] {
    if(!validForm())return;int i=web.arg("id").toInt()-1;
    if(!RADIO_MASTER||i<0||i>=MAX_NODES||!nodes[i].used){web.send(400);return;}
    IPAddress ip=network.peerIP(nodes[i].mac);
    if((uint32_t)ip==0||!nodes[i].reportAt||millis()-nodes[i].reportAt>1500||!queueCommand(i,DISARM)){web.send(409,"text/plain","Node offline");return;}
    web.send(200,"application/json","{\"url\":"+jsonString("http://"+ip.toString()+"/update?target="+macString(nodes[i].mac))+"}");
  });
  web.on("/update",HTTP_GET,[] {
    if(web.hasArg("target")&&!web.arg("target").equalsIgnoreCase(macString(network.mac))){web.send(409,"text/plain","Device mismatch");return;}
    String h="<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><link rel='stylesheet' href='/ui.css'><header>OTA "+String(RADIO_MASTER?"master":"slave")+" 0.4.1 · "+network.hostname+"</header><main><form class='panel' method='post' action='/update?token="+csrfToken+"&target="+macString(network.mac)+"' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><button>Обновить</button></form><a class='btn' href='/'>Назад</a></main>";
    web.send(200,"text/html; charset=utf-8",h);
  });
  web.on("/update",HTTP_POST,[] {
    if(!validForm())return;
    if(web.hasArg("target")&&!web.arg("target").equalsIgnoreCase(macString(network.mac))){web.send(409,"text/plain","Device mismatch");return;}
    web.send(otaOK?200:400,"text/plain",otaOK?"Update OK. Rebooting.":"Update failed");if(otaOK)restartAt=millis()+700;
  },[] {
    HTTPUpload &u=web.upload();
    if(u.status==UPLOAD_FILE_START) {
      otaActive=false;otaOK=false;
      if(web.arg("token")!=csrfToken)return;
      if(web.hasArg("target")&&!web.arg("target").equalsIgnoreCase(macString(network.mac)))return;
      String origin=web.header("Origin");if(origin.length()&&origin!="http://"+web.hostHeader())return;
      if(!RADIO_MASTER)actors.off(3);
      otaActive=Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);
    } else if(u.status==UPLOAD_FILE_WRITE&&otaActive) {
      if(Update.write(u.buf,u.currentSize)!=u.currentSize){Update.abort();otaActive=false;}
    } else if(u.status==UPLOAD_FILE_END&&otaActive) {otaOK=Update.end(true);otaActive=false;}
    else if(u.status==UPLOAD_FILE_ABORTED){Update.abort();otaActive=false;otaOK=false;}
  });
  web.onNotFound([] {web.send(404,"text/plain","Not found");});web.begin();
}
void setup() {
  if(!RADIO_MASTER)actors.begin();
  Serial.begin(115200);delay(500);
  bootSession=freshSession();
  char token[17];snprintf(token,sizeof(token),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());csrfToken=token;
  prefs.begin(RADIO_MASTER?"radio-master":"radio-slave",false);
  network.begin(RADIO_MASTER);
  if(RADIO_MASTER) {
    for(int i=0;i<MAX_NODES;i++) {
      String mk="mac"+String(i),nk="name"+String(i);
      if(prefs.getBytesLength(mk.c_str())==6) {
        nodes[i].used=true;prefs.getBytes(mk.c_str(),nodes[i].mac,6);
        nodes[i].name=prefs.getString(nk.c_str(),"Ведомый "+String(i+1));
      }
    }
  }
  resetStats();routes();radioReady=true;
  Serial.printf("\nStage WiFi %s role=%s SSID=%s MAC=%s host=%s.local\nUSB: i=info, j=status, u=UART, q=query, !=off\n",RADIO_VERSION,RADIO_MASTER?"master":"slave",STAGE_WIFI_SSID,macString(network.mac).c_str(),network.hostname.c_str());
}
void loop() {
  network.tick();
  if(networkEpochSeen!=network.generation)networkSessionChanged();
  if(!RADIO_MASTER)actors.tick(radioReady&&heartbeatFresh());
  if(!radioReady){led(BAD);delay(100);return;}
  linkTick();
  if(!RADIO_MASTER)actors.tick(heartbeatFresh());
  if(network.ready())web.handleClient();
  linkTick();
  if(!RADIO_MASTER)actors.tick(heartbeatFresh());
  if(Serial.available()) {
    char c=Serial.read();
    if(c=='i')Serial.printf("WiFi %s role=%s SSID=%s IP=%s connected=%d channel=%d RSSI=%d host=%s.local uptime=%lus id=%u master=%s RXdrops=%lu TXerrors=%lu flash=%u PSRAM=%u\n",RADIO_VERSION,RADIO_MASTER?"master":"slave",STAGE_WIFI_SSID,WiFi.localIP().toString().c_str(),network.ready(),WiFi.channel(),WiFi.RSSI(),network.hostname.c_str(),(unsigned long)(millis()/1000),assignedId,masterKnown?macString(masterMAC).c_str():"none",(unsigned long)rxDrops.load(),(unsigned long)txErrors,ESP.getFlashChipSize(),ESP.getPsramSize());
    if(c=='r')resetStats();
    if(c=='j')Serial.println(statusJSON(false));
    if(c=='u'&&!RADIO_MASTER)Serial.println(uartJSON(actors.player.diagnostics()));
    if(c=='q'&&!RADIO_MASTER){actors.player.poll();Serial.println("UART status query queued");}
    if(c=='!'){if(!RADIO_MASTER)actors.off();else for(int i=0;i<MAX_NODES;i++)queueCommand(i,ALL_OFF);}
  }
  Health h=WARMUP;
  if(RADIO_MASTER) {
    bool any=false;h=EXCELLENT;
    for(auto &n:nodes)if(n.used){any=true;Health nh=metrics(n).health;if(nh==WARMUP&&h==EXCELLENT)h=WARMUP;else if(nh>h)h=nh;}
    if(!any)h=WARMUP;
  } else h=lastProbe?(millis()-lastProbe>OFFLINE_MS?OFFLINE:slaveHealth):WARMUP;
  if(!network.ready())h=OFFLINE;
  led(h);
  if(restartAt&&(int32_t)(millis()-restartAt)>=0)ESP.restart();
  delay(1);
}

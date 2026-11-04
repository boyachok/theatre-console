#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <atomic>
#include <algorithm>
#include "protocol.h"
#include "dashboard.h"

// NO load outputs. Unencrypted, trusted bench test only.
constexpr uint8_t BROADCAST[6] = {255,255,255,255,255,255};
constexpr uint8_t MAINTENANCE = 6;
WebServer web(80);
Preferences prefs;
QueueHandle_t rxQueue;
std::atomic<uint32_t> rxDrops{0};
uint32_t txErrors=0, bootSession=0, testStart=0, restartAt=0;
String apName, csrfToken;
bool apActive=false, otaActive=false, otaOK=false, radioReady=false;
uint32_t apUntil=0, identifyUntil=0;
Health slaveHealth=WARMUP;
uint8_t masterMAC[6]{};
bool masterKnown=false;
uint32_t masterSession=0, masterSeen=0, lastProbe=0, helloAt=0;
uint8_t assignedId=0;
uint32_t lastProbeSeq=0;

struct Rx { uint8_t mac[6]; Packet p; int8_t rssi; uint32_t atUs; };
struct Sample {
  uint32_t seq=0, atUs=0, rttUs=0;
  int8_t rssiMaster=0, rssiSlave=0;
  bool used=false, received=false, expired=false, late=false;
};
struct Node {
  bool used=false; uint8_t mac[6]{}; String name;
  Sample samples[WINDOW];
  uint32_t seq=0, sent=0, received=0, deadlineReceived=0, timeouts=0, late=0, duplicates=0;
  uint32_t lastReply=0, lastSeen=0, nextPoll=0, streak=0, maxStreak=0;
  int8_t lastMasterRssi=0, lastSlaveRssi=0;
  bool hasReply=false, maintenance=false;
};
Node nodes[MAX_NODES];
struct Metrics {
  int n=0, replies=0, deadline=0, worstRssi=0;
  float p50=0, p95=0, maxRtt=0;
  Health health=WARMUP;
};
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
void onReceive(const esp_now_recv_info_t *info,const uint8_t *data,int len) {
  if(len!=(int)sizeof(Packet)) return;
  Rx r{}; memcpy(&r.p,data,sizeof(r.p));
  if(r.p.magic!=MAGIC||r.p.protocol!=PROTOCOL||r.p.size!=sizeof(Packet)) return;
  memcpy(r.mac,info->src_addr,6); r.rssi=info->rx_ctrl->rssi;
  r.atUs=(uint32_t)esp_timer_get_time();
  if(xQueueSend(rxQueue,&r,0)!=pdTRUE) rxDrops++;
}
bool addPeer(const uint8_t *mac) {
  if(esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t p{}; memcpy(p.peer_addr,mac,6);
  p.channel=RADIO_CHANNEL; p.ifidx=WIFI_IF_STA; p.encrypt=false;
  esp_err_t result=esp_now_add_peer(&p);
  if(result!=ESP_OK) {txErrors++;Serial.printf("add_peer failed %d\n",result);return false;}
  return true;
}
bool sendPacket(const uint8_t *mac,const Packet &p) {
  if(!addPeer(mac)) return false;
  if(esp_now_send(mac,(const uint8_t*)&p,sizeof(p))!=ESP_OK) {txErrors++;return false;}
  // This only means queued. An ECHO with matching session/sequence proves delivery.
  return true;
}
void startAP() {
  if(!apActive) {
    apActive=WiFi.softAP(apName.c_str(),"debug123",RADIO_CHANNEL,0,4);
    Serial.printf("AP %s password=debug123 URL=http://192.168.4.1 active=%d\n",apName.c_str(),apActive);
  }
  if(!RADIO_MASTER) apUntil=millis()+600000;
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
  if(previous!=color) {rgbLedWrite(48,r,g,b);previous=color;}
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
  testStart=millis(); txErrors=0;rxDrops=0;
  for(int i=0;i<MAX_NODES;i++) {
    Node &n=nodes[i]; for(auto &s:n.samples)s=Sample{};
    // Sequence numbers are NOT reused: outstanding old replies must not match new tests.
    n.sent=n.received=n.deadlineReceived=n.timeouts=n.late=n.duplicates=n.streak=n.maxStreak=0;
    n.hasReply=false;n.lastReply=0;n.nextPoll=millis()+i*50;
  }
}
void handleMasterRx(const Rx &r) {
  if(r.p.session!=bootSession)return;
  if(r.p.type!=HELLO&&r.p.type!=ECHO)return;
  int i=findNode(r.mac,r.p.type==HELLO);if(i<0)return;
  Node &n=nodes[i];n.lastSeen=millis();
  if(r.p.type!=ECHO)return;
  Sample &s=n.samples[r.p.seq%WINDOW];
  if(!s.used||s.seq!=r.p.seq)return;
  if(s.received){n.duplicates++;return;}
  s.received=true;s.rttUs=r.atUs-s.atUs;s.rssiMaster=r.rssi;s.rssiSlave=r.p.rssi;
  n.hasReply=true;n.lastReply=millis();n.lastMasterRssi=r.rssi;n.lastSlaveRssi=r.p.rssi;n.maintenance=r.p.reserved;
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
      if(masterKnown)esp_now_del_peer(masterMAC);
      memcpy(masterMAC,r.mac,6);masterKnown=true;masterSession=r.p.session;lastProbeSeq=0;
      assignedId=0;slaveHealth=WARMUP;lastProbe=0;
    }
    masterSeen=now;
    helloAt=now+20+esp_random()%350;
    return;
  }
  if(!masterKnown||memcmp(r.mac,masterMAC,6)||r.p.session!=masterSession)return;
  if(r.p.type==PROBE) {
    // Sequence freshness prevents old packets restoring a stale LED grade.
    if(lastProbeSeq && (int32_t)(r.p.seq-lastProbeSeq)<=0)return;
    lastProbeSeq=r.p.seq;masterSeen=now;lastProbe=now;assignedId=r.p.nodeId;
    slaveHealth=r.p.health<=OFFLINE?(Health)r.p.health:BAD;
    Packet p=packet(ECHO,masterSession,r.p.seq);p.rssi=r.rssi;p.nodeId=assignedId;p.reserved=apActive;
    sendPacket(masterMAC,p);
  } else if(r.p.type==IDENTIFY) identifyUntil=now+5000;
  else if(r.p.type==MAINTENANCE)startAP();
}
void radioTick() {
  Rx r;int budget=32;
  while(budget--&&xQueueReceive(rxQueue,&r,0)==pdTRUE) {
    if(RADIO_MASTER)handleMasterRx(r);else handleSlaveRx(r);
  }
  uint32_t now=millis();
  if(RADIO_MASTER) {
    static uint32_t discoverAt=0,sendAt=0;
    if((int32_t)(now-discoverAt)>=0) {sendPacket(BROADCAST,packet(DISCOVER,bootSession));discoverAt=now+2000;}
    uint32_t nowUs=(uint32_t)esp_timer_get_time();
    for(auto &n:nodes)if(n.used)for(auto &s:n.samples) {
      if(s.used&&!s.received&&!s.expired&&(uint32_t)(nowUs-s.atUs)>=TIMEOUT_MS*1000) {
        s.expired=true;n.timeouts++;n.streak++;n.maxStreak=std::max(n.maxStreak,n.streak);
      }
    }
    // No wait-for-response loop. At most one new poll per 20 ms for fair airtime.
    if((int32_t)(now-sendAt)>=0) {
      static int cursor=0;
      for(int c=0;c<MAX_NODES;c++) {
        int i=cursor;cursor=(cursor+1)%MAX_NODES;Node &n=nodes[i];
        if(!n.used||(int32_t)(now-n.nextPoll)<0)continue;
        n.nextPoll=now+POLL_MS;Metrics m=metrics(n);
        Packet p=packet(PROBE,bootSession,++n.seq);p.nodeId=i+1;p.health=m.health;
        Sample &s=n.samples[n.seq%WINDOW];s=Sample{};s.used=true;s.seq=n.seq;s.atUs=(uint32_t)esp_timer_get_time();
        n.sent++;sendPacket(n.mac,p);sendAt=now+20;break;
      }
    }
  } else {
    if(helloAt&&(int32_t)(now-helloAt)>=0) {helloAt=0;sendPacket(masterMAC,packet(HELLO,masterSession));}
    if(apActive&&apUntil&&(int32_t)(now-apUntil)>=0&&!otaActive) {WiFi.softAPdisconnect(true);apActive=false;apUntil=0;}
  }
}
String statusJSON(bool history) {
  String j;j.reserve(history?90000:10000);
  j="{\"version\":"+jsonString(RADIO_VERSION)+",\"role\":"+jsonString(RADIO_MASTER?"master":"slave")+",\"channel\":"+String(RADIO_CHANNEL)+",\"ap\":"+jsonString(apName)+",\"token\":"+jsonString(csrfToken)+",\"rxDrops\":"+String(rxDrops.load())+",\"txErrors\":"+String(txErrors)+",\"testSeconds\":"+String((millis()-testStart)/1000.0f,1)+",\"nodes\":[";
  bool comma=false;
  if(RADIO_MASTER)for(int i=0;i<MAX_NODES;i++) {
    Node &n=nodes[i];if(!n.used)continue;Metrics m=metrics(n);if(comma)j+=',';comma=true;
    j+="{\"id\":"+String(i+1)+",\"name\":"+jsonString(n.name)+",\"mac\":"+jsonString(macString(n.mac))+",\"health\":"+jsonString(healthName(m.health));
    j+=",\"windowN\":"+String(m.n)+",\"deliveryPct\":"+(m.n?String(100.0*m.replies/m.n,3):String("null"))+",\"deadlinePct\":"+(m.n?String(100.0*m.deadline/m.n,3):String("null"));
    j+=",\"p50\":"+(m.replies?String(m.p50,2):String("null"))+",\"p95\":"+(m.replies?String(m.p95,2):String("null"))+",\"maxRtt\":"+(m.replies?String(m.maxRtt,2):String("null"))+",\"spread\":"+(m.replies?String(m.p95-m.p50,2):String("null"));
    j+=",\"rssiMaster\":"+(n.hasReply?String(n.lastMasterRssi):String("null"))+",\"rssiSlave\":"+(n.hasReply?String(n.lastSlaveRssi):String("null"))+",\"worstRssi\":"+(m.replies?String(m.worstRssi):String("null"));
    j+=",\"ageMs\":"+(n.hasReply?String(millis()-n.lastReply):String("null"))+",\"streak\":"+String(n.streak)+",\"maxStreak\":"+String(n.maxStreak)+",\"sent\":"+String(n.sent)+",\"received\":"+String(n.received)+",\"timeouts\":"+String(n.timeouts)+",\"late\":"+String(n.late)+",\"duplicates\":"+String(n.duplicates)+",\"maintenance\":"+(n.maintenance?"true":"false");
    uint32_t finalized=n.received+n.timeouts;
    j+=",\"totalDeliveryPct\":"+(finalized?String(100.0*n.received/finalized,4):String("null"))+",\"totalDeadlinePct\":"+(finalized?String(100.0*n.deadlineReceived/finalized,4):String("null"));
    if(history) {
      j+=",\"samples\":[";bool sc=false;
      for(uint32_t k=(n.seq>=WINDOW?n.seq-WINDOW+1:1);k<=n.seq;k++) {
        Sample &s=n.samples[k%WINDOW];if(!s.used||s.seq!=k)continue;if(sc)j+=',';sc=true;
        j+="{\"seq\":"+String(s.seq)+",\"rttMs\":"+(s.received?String(s.rttUs/1000.0f,3):String("null"))+",\"expired\":"+(s.expired?"true":"false")+",\"late\":"+(s.late?"true":"false")+",\"rssiMaster\":"+(s.received?String(s.rssiMaster):String("null"))+",\"rssiSlave\":"+(s.received?String(s.rssiSlave):String("null"))+"}";
      }
      j+=']';
    }
    j+='}';
  }
  j+="]}";return j;
}
bool validForm() {
  if(web.arg("token")==csrfToken)return true;
  web.send(403,"text/plain","Invalid form token");return false;
}
void routes() {
  const char *headers[]={"Origin"};web.collectHeaders(headers,1);
  web.on("/",HTTP_GET,[] {
    web.sendHeader("Cache-Control","no-store");
    if(RADIO_MASTER)web.send_P(200,"text/html; charset=utf-8",DASHBOARD);
    else web.send(200,"text/html; charset=utf-8","<meta charset='utf-8'><h1>Stage Radio Slave 0.2.0</h1><p>ESP-NOW работает. Точка обслуживания закроется через 10 минут.</p><a href='/update'>OTA ведомого</a>");
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
    bool ok=sendPacket(nodes[i].mac,packet(IDENTIFY,bootSession));web.send(ok?200:503,"text/plain",ok?"Queued (not confirmed)":"Send failed");
  });
  web.on("/maintenance",HTTP_POST,[] {
    if(!validForm())return;int i=web.arg("id").toInt()-1;
    if(!RADIO_MASTER||i<0||i>=MAX_NODES||!nodes[i].used){web.send(400);return;}
    bool ok=sendPacket(nodes[i].mac,packet((Type)MAINTENANCE,bootSession));web.send(ok?200:503,"text/plain",ok?"Queued. Slave setup AP opens for 10 minutes if delivered.":"Send failed");
  });
  web.on("/update",HTTP_GET,[] {
    String h="<meta charset='utf-8'><h1>OTA "+String(RADIO_MASTER?"master":"slave")+" 0.2.0</h1><p>Только application .bin для этой роли и платы! Не merged.bin. Во время обновления тест прерывается.</p><form method='post' action='/update?token="+csrfToken+"' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><button>Обновить</button></form>";
    web.send(200,"text/html; charset=utf-8",h);
  });
  web.on("/update",HTTP_POST,[] {
    if(!validForm())return;
    web.send(otaOK?200:400,"text/plain",otaOK?"Update OK. Rebooting.":"Update failed");if(otaOK)restartAt=millis()+700;
  },[] {
    HTTPUpload &u=web.upload();
    if(u.status==UPLOAD_FILE_START) {
      otaActive=false;otaOK=false;
      if(web.arg("token")!=csrfToken)return;
      String origin=web.header("Origin");if(origin.length()&&origin!="http://"+web.hostHeader())return;
      otaActive=Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);
    } else if(u.status==UPLOAD_FILE_WRITE&&otaActive) {
      if(Update.write(u.buf,u.currentSize)!=u.currentSize){Update.abort();otaActive=false;}
    } else if(u.status==UPLOAD_FILE_END&&otaActive) {otaOK=Update.end(true);otaActive=false;}
    else if(u.status==UPLOAD_FILE_ABORTED){Update.abort();otaActive=false;otaOK=false;}
  });
  web.onNotFound([] {web.send(404,"text/plain","Not found");});web.begin();
}
void setup() {
  Serial.begin(115200);delay(500);
  bootSession=esp_random();if(!bootSession)bootSession=1;
  char token[17];snprintf(token,sizeof(token),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());csrfToken=token;
  prefs.begin(RADIO_MASTER?"radio-master":"radio-slave",false);
  // Slave must start in STA-only mode: AP_STA could revive an old setup SSID.
  WiFi.mode(RADIO_MASTER?WIFI_AP_STA:WIFI_STA);WiFi.setAutoReconnect(false);WiFi.setSleep(false);
  // Do not inherit a stored router association: it could change the radio channel.
  WiFi.disconnect(false,false);delay(20);
  esp_wifi_set_ps(WIFI_PS_NONE);
  if(esp_wifi_set_channel(RADIO_CHANNEL,WIFI_SECOND_CHAN_NONE)!=ESP_OK){Serial.println("Channel setup failed");led(BAD);return;}
  uint8_t mac[6];WiFi.macAddress(mac);char suffix[7];snprintf(suffix,sizeof(suffix),"%02X%02X%02X",mac[3],mac[4],mac[5]);
  apName=String(RADIO_MASTER?"Stage-Master-":"Stage-Slave-")+suffix;
  rxQueue=xQueueCreate(64,sizeof(Rx));
  if(!rxQueue||esp_now_init()!=ESP_OK){Serial.println("ESP-NOW init failed");led(BAD);return;}
  esp_now_register_recv_cb(onReceive);addPeer(BROADCAST);
  if(RADIO_MASTER) {
    for(int i=0;i<MAX_NODES;i++) {
      String mk="mac"+String(i),nk="name"+String(i);
      if(prefs.getBytesLength(mk.c_str())==6) {
        nodes[i].used=true;prefs.getBytes(mk.c_str(),nodes[i].mac,6);
        nodes[i].name=prefs.getString(nk.c_str(),"Ведомый "+String(i+1));addPeer(nodes[i].mac);
      }
    }
    startAP();
  }
  resetStats();routes();radioReady=true;
  Serial.printf("\nStage Radio %s role=%s channel=%d MAC=%s\nUSB: i=info, a=maintenance AP, r=reset stats\n",RADIO_VERSION,RADIO_MASTER?"master":"slave",RADIO_CHANNEL,macString(mac).c_str());
}
void loop() {
  if(!radioReady){led(BAD);delay(100);return;}
  radioTick();
  if(apActive)web.handleClient();
  radioTick();
  if(Serial.available()) {
    char c=Serial.read();
    if(c=='i')Serial.printf("Radio %s role=%s channel=%d uptime=%lus AP=%s active=%d pass=debug123 web=no-login id=%d master=%s lastProbeAge=%lu RXdrops=%lu TXerrors=%lu flash=%u PSRAM=%u\n",RADIO_VERSION,RADIO_MASTER?"master":"slave",RADIO_CHANNEL,(unsigned long)(millis()/1000),apName.c_str(),apActive,assignedId,masterKnown?macString(masterMAC).c_str():"none",(unsigned long)(lastProbe?millis()-lastProbe:0),(unsigned long)rxDrops.load(),(unsigned long)txErrors,ESP.getFlashChipSize(),ESP.getPsramSize());
    if(c=='a')startAP();if(c=='r')resetStats();
  }
  Health h=WARMUP;
  if(RADIO_MASTER) {
    bool any=false;h=EXCELLENT;
    for(auto &n:nodes)if(n.used){any=true;Health nh=metrics(n).health;if(nh==WARMUP&&h==EXCELLENT)h=WARMUP;else if(nh>h)h=nh;}
    if(!any)h=WARMUP;
  } else h=lastProbe?(millis()-lastProbe>OFFLINE_MS?OFFLINE:slaveHealth):WARMUP;
  led(h);
  if(restartAt&&(int32_t)(millis()-restartAt)>=0)ESP.restart();
  delay(1);
}

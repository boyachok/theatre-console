#pragma once
#include "remote_link.h"
#if RADIO_MASTER
// Separate role: these peers never occupy an actuator slot or receive a score.
struct RemotePeer {bool used=false;uint8_t mac[6]{};Node monitor;Remote::Gate gate;uint32_t seen=0,nextPing=0;};
RemotePeer remotePeers[2];
int findRemote(const uint8_t *mac){for(int i=0;i<2;i++)if(remotePeers[i].used&&!memcmp(remotePeers[i].mac,mac,6))return i;return -1;}
void resetRemoteNetwork(){for(auto &peer:remotePeers)peer=RemotePeer{};}
void resetRemoteStats(){for(auto &p:remotePeers)if(p.used){auto &n=p.monitor;for(auto &s:n.samples)s=Sample{};n.sent=n.received=n.deadlineReceived=n.timeouts=n.late=n.duplicates=n.streak=n.maxStreak=0;n.hasReply=false;p.nextPing=0;}}
void pauseRemoteMeasurements(){for(auto &p:remotePeers)for(auto &s:p.monitor.samples)if(s.used&&!s.received&&!s.expired)s.used=false;}
bool navigateCurrent(uint8_t action){
 if(!RADIO_MASTER||networkResetActive||otaSafetyLock||!Remote::validAction(action)||!showMaster.navigationCount())return false;
 if(!showMaster.livePreview&&(showMaster.draftUnsaved()||!publishedReady()))return false;
 uint16_t target=Remote::target((Remote::Action)action,showMaster.navigationCount(),showMaster.step,showMaster.cursorValid);
 if(showMaster.active&&showMaster.isPreview==showMaster.livePreview&&target==showMaster.step){showMaster.bumpNavigation();return true;}
 if(showMaster.livePreview)return goDraft(target,action==Remote::BACK||action==Remote::FIRST?1:0);
 if(!goPublished(target,action==Remote::BACK||action==Remote::FIRST?1:0))return false;
 return true;
}
void remoteMasterTick(){
 if(!RADIO_MASTER||networkResetActive||!network.ready())return;
 uint32_t now=millis(),us=(uint32_t)esp_timer_get_time();
 for(auto &p:remotePeers)if(p.used&&measurementsRunning)for(auto &s:p.monitor.samples)if(s.used&&!s.received&&!s.expired&&us-s.atUs>=TIMEOUT_MS*1000){s.expired=true;p.monitor.timeouts++;p.monitor.streak++;p.monitor.maxStreak=std::max(p.monitor.maxStreak,p.monitor.streak);}
 static uint32_t lastSend=0;static bool sentBefore=false;if(sentBefore&&now-lastSend<20)return;
 for(auto &p:remotePeers){auto &n=p.monitor;if(!p.used||!n.deviceBoot||(p.nextPing&&(int32_t)(now-p.nextPing)<0)||(uint32_t)network.peerIP(p.mac)==0)continue;
  p.nextPing=now+500;lastSend=now;sentBefore=true;Packet ping=packet(REMOTE_PING,bootSession,++n.seq);if(!ping.seq)ping.seq=++n.seq;
  ping.deviceBoot=n.deviceBoot;ping.commandId=freshSession();p.gate.issue(ping.commandId,now);ping.padding[0]='R';ping.padding[1]=1;ping.health=metrics(n).health;ping.reserved=measurementsRunning?1:0;ping.value=showMaster.cursorValid?showMaster.step+1:0;
  if(measurementsRunning){auto &s=n.samples[ping.seq%WINDOW];s=Sample{};s.used=true;s.seq=ping.seq;s.atUs=(uint32_t)esp_timer_get_time();n.sent++;}
  sendPacket(p.mac,ping);break;
 }
}
bool handleRemoteRx(const Rx &r){
 if(!RADIO_MASTER)return false;
 if(r.p.type==FIRMWARE_REPORT&&r.firmwareRole==FW_REMOTE){int i=findRemote(r.mac);if(i>=0&&r.p.session==bootSession&&remotePeers[i].monitor.deviceBoot==r.p.deviceBoot)remotePeers[i].monitor.firmwareVersion=String(r.firmwareVersion);return true;}
 if(!remoteType(r.p.type))return findRemote(r.mac)>=0&&(r.p.type==HELLO||r.p.type==ECHO);
 if(r.p.protocol!=PROTOCOL||!remoteTag(r.p)||r.p.session!=bootSession||!r.p.deviceBoot)return true;
 int index=findRemote(r.mac);uint32_t now=millis();
 if(r.p.type==REMOTE_HELLO){
  if(networkResetActive)return true;
  if(index<0){for(int i=0;i<2;i++)if(!remotePeers[i].used||now-remotePeers[i].seen>15000){index=i;remotePeers[i]=RemotePeer{};remotePeers[i].used=true;memcpy(remotePeers[i].mac,r.mac,6);break;}}
  if(index<0)return true;
  auto &p=remotePeers[index];if(p.monitor.deviceBoot!=r.p.deviceBoot){p.monitor=Node{};p.monitor.deviceBoot=r.p.deviceBoot;p.monitor.name=index?"remote 2":"remote";p.gate=Remote::Gate{};p.nextPing=0;}
  p.seen=now;
  int old=findNode(r.mac,false);if(old>=0){prefs.remove((String("mac")+old).c_str());prefs.remove((String("name")+old).c_str());nodes[old]=Node{};}
  return true;
 }
 if(index<0)return true;auto &peer=remotePeers[index];auto &n=peer.monitor;if(n.deviceBoot!=r.p.deviceBoot)return true;
 if(r.p.type==REMOTE_PONG){
  bool known=false;for(const auto &t:peer.gate.tokens)if(t.id==r.p.commandId&&now-t.at<=Remote::TOKEN_TTL_MS)known=true;if(!known)return true;
  peer.seen=now;n.hasReply=true;n.lastReply=now;n.lastMasterRssi=r.rssi;n.lastSlaveRssi=r.p.rssi;
  if(!measurementsRunning)return true;auto &s=n.samples[r.p.seq%WINDOW];if(!s.used||s.seq!=r.p.seq)return true;if(s.received){n.duplicates++;return true;}
  s.received=true;s.rttUs=r.atUs-s.atUs;s.rssiMaster=r.rssi;s.rssiSlave=r.p.rssi;
  if(s.rttUs<=TIMEOUT_MS*1000){if(s.expired&&n.timeouts)n.timeouts--;n.received++;if(s.rttUs<=100000)n.deadlineReceived++;n.streak=0;}
  else{s.late=true;n.late++;if(!s.expired){s.expired=true;n.timeouts++;n.streak++;n.maxStreak=std::max(n.maxStreak,n.streak);}}return true;
 }
 if(r.p.type==REMOTE_EVENT){
  auto decision=peer.gate.inspect(r.p.commandId,r.p.op,r.p.seq,r.p.uptime,now);Result result=STALE;
  if(decision==Remote::Gate::DUPLICATE)result=(Result)peer.gate.lastResult;
  else if(decision==Remote::Gate::NEW){result=navigateCurrent(r.p.op)?APPLIED:NOT_READY;peer.gate.remember(r.p.commandId,r.p.op,result);peer.seen=now;}
  Packet ack=packet(REMOTE_ACK,bootSession,r.p.commandId);ack.deviceBoot=n.deviceBoot;ack.commandId=r.p.commandId;ack.result=result;ack.value=showMaster.cursorValid?showMaster.step+1:0;ack.health=metrics(n).health;ack.padding[0]='R';ack.padding[1]=1;sendPacket(peer.mac,ack);return true;
 }return true;
}
bool identifyRemote(const uint8_t *mac){int i=findRemote(mac);if(i<0)return false;auto &p=remotePeers[i];Packet command=packet(IDENTIFY,bootSession);command.deviceBoot=p.monitor.deviceBoot;return sendPacket(mac,command);}
String remoteStatusJSON(){
 String j="[";bool comma=false;for(auto &p:remotePeers)if(p.used){if(comma)j+=',';comma=true;auto &n=p.monitor;auto m=metrics(n);
  j+="{\"role\":\"remote\",\"name\":"+jsonString(n.name)+",\"mac\":"+jsonString(macString(p.mac))+",\"ip\":"+((uint32_t)network.peerIP(p.mac)?jsonString(network.peerIP(p.mac).toString()):String("null"))+",\"ageMs\":"+(n.hasReply?String(millis()-n.lastReply):String("null"));
  j+=",\"firmwareVersion\":"+(n.firmwareVersion.length()?jsonString(n.firmwareVersion):String("null"));
  j+=",\"sent\":"+String(n.sent)+",\"received\":"+String(n.received)+",\"timeouts\":"+String(n.timeouts)+",\"p50\":"+(m.replies?String(m.p50,2):String("null"))+",\"p95\":"+(m.replies?String(m.p95,2):String("null"));
  j+=",\"rssiSlave\":"+(n.hasReply?String(n.lastSlaveRssi):String("null"))+",\"worstRssi\":"+(m.replies?String(m.worstRssi):String("null"))+",\"health\":"+jsonString(healthName(m.health))+"}";
 }return j+"]";
}
#else
inline void resetRemoteNetwork(){} inline void resetRemoteStats(){} inline void pauseRemoteMeasurements(){} inline void remoteMasterTick(){}
inline bool handleRemoteRx(const Rx &){return false;} inline bool identifyRemote(const uint8_t*){return false;} inline bool navigateCurrent(uint8_t){return false;} inline String remoteStatusJSON(){return "[]";}
#endif

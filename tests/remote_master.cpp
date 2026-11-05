// Actual master remote handlers, with transport/publication hardware boundaries mocked.
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
class String:public std::string {
public:
 using std::string::string;String()=default;String(const std::string &s):std::string(s){}
 String(int v):std::string(std::to_string(v)){}String(unsigned v):std::string(std::to_string(v)){}
 String(float v,int):std::string(std::to_string(v)){}
 friend String operator+(String a,const String &b){a.append(b);return a;}friend String operator+(const char *a,const String &b){return String(a)+b;}
 friend String operator+(String a,const char *b){a.append(b);return a;}
 friend String operator+(const String &a,int b){return a+String(b);}
};
#define RADIO_MASTER 1
#include "../console/protocol.h"
#include "../console/remote_link.h"
struct Sample{uint32_t seq=0,atUs=0,rttUs=0;int8_t rssiMaster=0,rssiSlave=0;bool used=false,received=false,expired=false,late=false;};
struct Node{bool used=false,hasReply=false;uint8_t mac[6]{};String name;Sample samples[WINDOW];uint32_t deviceBoot=0,seq=0,sent=0,received=0,deadlineReceived=0,timeouts=0,late=0,duplicates=0,streak=0,maxStreak=0,lastReply=0,nextScore=0;int8_t lastMasterRssi=0,lastSlaveRssi=0;};
Node nodes[MAX_NODES];struct Rx{uint8_t mac[6]{};Packet p{};int8_t rssi=-50;uint32_t atUs=0;};
struct FakeIP{operator uint32_t()const{return 1;}String toString()const{return "192.168.50.88";}};
struct Network{bool connected=true;bool ready()const{return connected;}FakeIP peerIP(const uint8_t*)const{return {};}}network;
struct Preferences{void remove(const char*){}}prefs;
struct Publication{std::vector<String> stepIds{"s0","s1","s2"};bool active=false,isPreview=false;uint16_t step=0;int calls=0;bool go(uint16_t to,uint8_t){if(to>=stepIds.size())return false;step=to;active=true;isPreview=false;calls++;return true;}}showMaster;
bool networkResetActive=false,otaSafetyLock=false,measurementsRunning=true,plansReady=true;uint32_t bootSession=77;
bool goPublished(uint16_t target,uint8_t reason){if(!plansReady)return false;if(!showMaster.go(target,reason))return false;for(auto &n:nodes)n.nextScore=0;return true;}
uint32_t freshSession(){static uint32_t n=900;return ++n;}uint64_t esp_timer_get_time(){return (uint64_t)mockNow*1000;}
struct Sent{uint8_t mac[6];Packet p;};std::vector<Sent> sent;
bool sendPacket(const uint8_t *mac,const Packet &p){Sent item{};std::copy(mac,mac+6,item.mac);item.p=p;sent.push_back(item);return true;}
int findNode(const uint8_t *mac,bool){for(int i=0;i<MAX_NODES;i++)if(nodes[i].used&&!memcmp(nodes[i].mac,mac,6))return i;return -1;}
struct Metrics{int replies=0,worstRssi=-60;float p50=2,p95=2;Health health=GOOD;};Metrics metrics(Node &n){Metrics m;m.replies=n.received;return m;}
String jsonString(const String &s){return String("\"")+s+String("\"");}String macString(const uint8_t*){return "AA:BB:CC:00:00:22";}const char *healthName(Health){return "good";}
#include "../console/remote_master.h"
int main(){
 for(int i=0;i<MAX_NODES;i++){nodes[i].used=true;nodes[i].mac[0]=0x20;nodes[i].mac[5]=i;}
 Rx r;r.mac[0]=0xaa;r.mac[5]=0x22;r.p=packet(REMOTE_HELLO,bootSession);r.p.deviceBoot=9;r.p.padding[0]='R';r.p.padding[1]=1;mockNow=100;assert(handleRemoteRx(r));assert(findRemote(r.mac)==0);for(auto &n:nodes)assert(n.used); // no actuator slots consumed
 remoteMasterTick();assert(sent.back().p.type==REMOTE_PING);Packet ping=sent.back().p;assert(ping.deviceBoot==9);assert(remoteStatusJSON().find("remote")!=std::string::npos);
 r.p=packet(REMOTE_PONG,bootSession,ping.seq);r.p.deviceBoot=9;r.p.commandId=ping.commandId;r.p.padding[0]='R';r.p.padding[1]=1;r.p.rssi=-60;mockNow=102;r.atUs=102000;handleRemoteRx(r);assert(remotePeers[0].monitor.received==1);
 auto event=[&](uint32_t id,uint8_t action){r.p=packet(REMOTE_EVENT,bootSession,ping.commandId);r.p.deviceBoot=9;r.p.commandId=id;r.p.op=action;r.p.uptime=mockNow+700;r.p.padding[0]='R';r.p.padding[1]=1;};
 event(1,Remote::FORWARD);handleRemoteRx(r);assert(showMaster.step==0&&showMaster.calls==1&&sent.back().p.result==APPLIED);handleRemoteRx(r);assert(showMaster.calls==1);event(2,Remote::FORWARD);handleRemoteRx(r);assert(showMaster.step==1&&showMaster.calls==2);
 event(3,Remote::BACK);r.p.uptime=mockNow-1;handleRemoteRx(r);assert(showMaster.calls==2&&sent.back().p.result==STALE);
 event(3,Remote::LAST);r.p.deviceBoot=10;handleRemoteRx(r);assert(showMaster.calls==2);r.p.deviceBoot=9;r.p.session=88;handleRemoteRx(r);assert(showMaster.calls==2);
 event(3,Remote::LAST);networkResetActive=true;handleRemoteRx(r);assert(sent.back().p.result==NOT_READY&&showMaster.calls==2);networkResetActive=false;handleRemoteRx(r);assert(showMaster.calls==2); // rejected command cannot execute after reset/publish
 event(4,Remote::BACK);otaSafetyLock=true;handleRemoteRx(r);assert(sent.back().p.result==NOT_READY);otaSafetyLock=false;handleRemoteRx(r);assert(showMaster.calls==2);
 event(5,Remote::LAST);plansReady=false;handleRemoteRx(r);assert(sent.back().p.result==NOT_READY&&showMaster.calls==2);plansReady=true;handleRemoteRx(r);assert(showMaster.calls==2); // No delayed GO after preload completes.
 event(6,Remote::LAST);handleRemoteRx(r);assert(showMaster.step==2&&showMaster.calls==3);assert(identifyRemote(r.mac)&&sent.back().p.type==IDENTIFY&&sent.back().p.deviceBoot==9);
 pauseRemoteMeasurements();measurementsRunning=false;auto count=remotePeers[0].monitor.sent;mockNow=600;remoteMasterTick();assert(sent.back().p.type==REMOTE_PING&&sent.back().p.reserved==0&&remotePeers[0].monitor.sent==count);
 mockNow=0x80000100U;resetRemoteStats();auto before=sent.size();remoteMasterTick();assert(sent.size()==before+1&&sent.back().p.type==REMOTE_PING);
 resetRemoteNetwork();assert(findRemote(r.mac)<0);puts("Actual master/remote handlers: separate role and 10 full actuator slots, handshake, real ping RTT, once-only published navigation, expired/wrong boot/session rejection, no delayed reset/OTA replay, identification and paused measurement heartbeat passed");
}

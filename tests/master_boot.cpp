#define RADIO_MASTER 1
#include <cassert>
#include <vector>
#include <cstdio>
#include <cstring>
#include "../console/protocol.h"
#include "../console/remote_link.h"
#include "../console/helmet_sound_wire.h"
#include "../console/startup_policy.h"
StartupPolicy startupShow;bool radioReady=true,networkResetActive=false,otaSafetyLock=false;uint32_t bootSession=99;
struct Node {ControlState reported{};uint8_t mac[6]{};bool helmet=false,soundSupported=false,hasReply=false,maintenance=false;uint32_t deviceBoot=11,soundRevision=0,soundHash=0,lastReply=0,nextSound=0;}nodes[2];
struct Plan {uint8_t mac[6]{};};
struct Master {std::vector<Plan> plans,draftPlans;bool active=false,cursorValid=false,workspaceFault=false;unsigned step=9;Plan *find(const uint8_t *mac){for(auto &p:plans)if(!memcmp(p.mac,mac,6))return &p;return nullptr;}}showMaster;
struct Store {struct Slot {uint8_t mac[6]{};HelmetSound::Profile profile{};uint32_t checksum=123;};std::vector<Slot> slots;bool fault=false;uint32_t revision=1;void tick(uint32_t){}Slot *find(const uint8_t *mac){for(auto &s:slots)if(!memcmp(s.mac,mac,6))return &s;return nullptr;}}helmetSounds;
struct Network {bool connected=true;int sent=0;bool ready(){return connected;}uint32_t peerIP(const uint8_t*){return 1;}bool send(const uint8_t*,const uint8_t*,size_t){sent++;return true;}}network;
struct Rx {uint8_t mac[6]{};Packet p{};};
int findNode(const uint8_t *mac,bool){for(int i=0;i<2;i++)if(!memcmp(nodes[i].mac,mac,6))return i;return -1;}
bool plansReady=false;bool publishedReady(){return plansReady;}int goCalls=0;
bool goPublished(uint16_t step,uint8_t){startupShow.cancel();showMaster.step=step;showMaster.active=showMaster.cursorValid=true;goCalls++;return true;}
#include "../console/helmet_sound_master.h"
int main(){
 for(int i=0;i<2;i++){nodes[i].mac[5]=i+1;nodes[i].reported.ready=true;Plan p;memcpy(p.mac,nodes[i].mac,6);showMaster.plans.push_back(p);}nodes[1].helmet=nodes[1].soundSupported=true;Store::Slot slot;memcpy(slot.mac,nodes[1].mac,6);helmetSounds.slots.push_back(slot);
 StartupPolicy faultReset;faultReset.begin(false);assert(!faultReset.waiting()&&!faultReset.take(true));
 mockNow=1000;startupShow.begin(true);bootShowTick();assert(!goCalls&&startupShow.waiting());plansReady=true;nodes[0].hasReply=nodes[1].hasReply=true;nodes[0].lastReply=nodes[1].lastReply=1000;bootShowTick();assert(!goCalls); // Global preset ACK required too.
 Rx r;memcpy(r.mac,nodes[1].mac,6);r.p.deviceBoot=12;r.p.result=APPLIED;r.p.seq=1;r.p.commandId=123;handleGlobalVoiceAck(r);assert(!globalVoiceReady());r.p.deviceBoot=11;handleGlobalVoiceAck(r);assert(globalVoiceReady());
 bootShowTick();assert(goCalls==1&&showMaster.active&&showMaster.cursorValid&&showMaster.step==0&&!startupShow.waiting());assert(Remote::target(Remote::FORWARD,2,showMaster.step,showMaster.cursorValid)==1);
 network.connected=false;bootShowTick();network.connected=true;bootShowTick();startupShow.begin(true);bootShowTick();assert(goCalls==1); // No reconnect/browser/publication re-arm.
 startupShow.cancel();showMaster.active=false;bootShowTick();assert(goCalls==1); // STOP authoritative.
 startupShow=StartupPolicy{};showMaster.cursorValid=false;startupShow.begin(true);startupShow.cancel();bootShowTick();assert(goCalls==1); // STOP before readiness.
 startupShow=StartupPolicy{};startupShow.begin(true);nodes[0].lastReply=0;mockNow=5000;bootShowTick();assert(goCalls==1&&startupShow.waiting());nodes[0].lastReply=nodes[1].lastReply=5000;bootShowTick();assert(goCalls==2&&showMaster.step==0);
 puts("Actual boot coordinator: first-scene GO once only after all plans/fresh peers/global voice ACK; remote forward then targets second; no reconnect/browser/publication re-arm, STOP-before-ready and STOP-after-start held; no hardware outputs tested");
}

// Actual delivery handlers/scheduler; only transport, web-independent registry and plan metadata are mocked.
#include <cassert>
#include <cstdio>
#include <vector>
#include <array>
#include <cstring>
bool asMaster=false;
#define RADIO_MASTER asMaster
#define THEATRE_HOST_TEST
#include "../console/protocol.h"
#include "../console/score_runtime.h"
#include "../console/lan_wire.h"
struct LanMessage {uint8_t mac[6]{},payload[LAN_MAX_PAYLOAD]{};uint16_t length=0;int8_t rssi=-50;};
struct Node {
 bool used=false,localScoreSupported=false,hasReply=false;uint8_t mac[6]{};
 uint32_t deviceBoot=0,scoreLoaded=0,scoreChecksum=0,scoreStoredUptime=0,scoreNext=0,scoreAt=0,scoreSeq=0,scoreFeedbackAt=0,scoreFeedbackUptime=0,scoreSent=0,scoreReceived=0,nextScore=0,nextStore=0,lastReply=0,reportAt=0,reportUptime=0;
 uint16_t scoreStep=0;uint8_t scoreResult=NOT_READY;int32_t scoreRtt=-1;int8_t lastMasterRssi=0,lastSlaveRssi=0;ControlState reported{};
}nodes[MAX_NODES];
struct Rx {uint8_t mac[6]{};Packet p{};int8_t rssi=-50;};
struct Plan {uint8_t mac[6]{};std::vector<Show::Cue> cues;uint32_t checksum=0;};
struct Publication {
 std::vector<Plan> plans,preview;std::vector<int> stepIds{0,1};uint32_t version=12,run=300,sequence=0;uint16_t step=0,previewTotal=2;uint8_t reason=0;bool active=false,isPreview=false,syncPending=false,programPreview=false,livePreview=false,dirty=false;
 bool draftUnsaved()const{return dirty;}
 uint16_t previewIndex()const{return programPreview?step:0;}
 bool goDraft(uint16_t target,uint8_t why){if(target>=previewTotal)return false;step=target;reason=why;active=isPreview=programPreview=true;sequence++;return true;}
 Plan *find(const uint8_t *mac,bool draft=false){for(auto &p:(draft?preview:plans))if(!memcmp(mac,p.mac,6))return &p;return nullptr;}
 bool go(uint16_t next,uint8_t why){if(next>=stepIds.size())return false;step=next;reason=why;isPreview=false;active=true;if(!++sequence)++sequence;return true;}
}showMaster;
struct Frame {uint8_t mac[6]{};uint32_t at=0;std::vector<uint8_t> bytes;};
struct Network {
 bool connected=true;uint32_t rxInvalid=0;std::vector<Frame> sent;
 bool ready()const{return connected;}uint32_t peerIP(const uint8_t*)const{return 1;}
 bool send(const uint8_t *mac,const uint8_t *data,size_t n){Frame f;memcpy(f.mac,mac,6);f.at=millis();f.bytes.assign(data,data+n);sent.push_back(f);return true;}
}network;
Actors actors;Show::Runner showRunner(actors);Show::SlaveStore showStore;
bool networkResetActive=false,otaSafetyLock=false,masterKnown=true,showRecovery=false,showFSReady=true;
uint32_t bootSession=700,masterSession=77,masterSeen=0,lastProbe=0,probeReceivedAt=0;
uint8_t masterMAC[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};Health slaveHealth=WARMUP;
std::vector<Packet> replies;
bool sendPacket(const uint8_t*,const Packet &p){replies.push_back(p);return true;}
int findNode(const uint8_t *mac,bool){for(int i=0;i<MAX_NODES;i++)if(nodes[i].used&&!memcmp(mac,nodes[i].mac,6))return i;return -1;}
struct Metrics {Health health=GOOD;};Metrics metrics(const Node&){return {};}
#include "../console/scene_delivery.h"
template<class T>bool receive(const T &p){LanMessage m;memcpy(m.mac,masterMAC,6);m.length=sizeof(p);memcpy(m.payload,&p,sizeof(p));return handleShowMessage(m);}
Show::Packet base(uint8_t type){Show::Packet p{};p.magic=Show::MAGIC;p.protocol=Show::PROTOCOL;p.type=type;p.size=sizeof(p);p.session=77;p.deviceBoot=700;p.run=100;p.transition=1;p.reserved=Show::ACTIVE;p.version=12;p.total=2;return p;}
int main(){
 assert(Show::REPEAT_MS==200&&sizeof(Show::RunPacket)==44&&Show::PROTOCOL==8);
 actors.begin();showRunner.begin();mockNow=100;
 Show::Cue cues[2]{};for(int i=0;i<2;i++){cues[i].dimmerMask=1;cues[i].dimmers[0]={(uint8_t)(60+i*120),1000,0};}
 auto crc=Show::crc32(cues,sizeof(cues));auto ref=Show::reference(base(Show::RUN));ref.checksum=crc;ref.step=1;
 assert(receive(ref)&&replies.back().result==NOT_READY&&(replies.back().reserved&Show::MISSING_PLAN));assert(!showRunner.active&&actors.dimmerValue(0)==0);
 auto part=base(Show::STORE);part.checksum=crc;part.step=1;part.cue=cues[1];receive(part);assert(showStore.version==0);
 part.step=0;part.cue=cues[0];receive(part);assert(showStore.version==12&&replies.back().seq==crc&&(replies.back().reserved&Show::LOADED));
 Show::SlaveStore reboot;reboot.begin();assert(reboot.version==12&&!showRunner.active);
 mockNow=1000;receive(ref);assert(replies.back().result==APPLIED&&showRunner.sequence==1);
 showRunner.tick(1500);assert(actors.dimmerValue(0)==90);mockNow=1500;receive(ref);showRunner.tick(2000);assert(actors.dimmerValue(0)==180); // Duplicates do not restart the ramp.
 auto bad=ref;bad.transition=2;bad.version++;receive(bad);assert(replies.back().result==NOT_READY&&(replies.back().reserved&Show::MISSING_PLAN)&&showRunner.sequence==1);
 bad=ref;bad.checksum++;receive(bad);assert(replies.back().reserved&Show::MISSING_PLAN);
 bad=ref;bad.total=3;receive(bad);assert(replies.back().reserved&Show::MISSING_PLAN);
 bad=ref;bad.step=2;receive(bad);assert(replies.back().result==NOT_READY&&!(replies.back().reserved&Show::MISSING_PLAN));
 auto full=base(Show::RUN);full.cue.dimmerMask=1;full.cue.dimmers[0].value=255;auto invalids=network.rxInvalid;receive(full);assert(network.rxInvalid==invalids+1&&showRunner.sequence==1); // Full-cue RUN is NOT a fallback.
 bad=ref;bad.protocol=6;receive(bad);assert(network.rxInvalid==invalids+2);
 bad=ref;bad.deviceBoot++;receive(bad);assert(network.rxInvalid==invalids+3);
 full.type=Show::PREVIEW;full.transition=2;full.cue.dimmers[0]={42,0,0};mockNow=2100;receive(full);assert(replies.back().result==APPLIED&&actors.dimmerValue(0)==42&&showStore.checksum==crc);
 ref.transition=3;ref.step=0;mockNow=2200;receive(ref);showRunner.tick(3200);assert(actors.dimmerValue(0)==60); // Cached cue, not the previous preview target.
 ref.transition=1;receive(ref);assert(replies.back().result==NOT_READY&&showRunner.sequence==3);
 showRunner.stop();ref.transition=3;receive(ref);assert(replies.back().result==NOT_READY&&!showRunner.active); // STOP is not undone by a repeat.
 ref.transition=4;ref.step=0;receive(ref);assert(showRunner.active);auto idle=ref;idle.reserved=0;idle.transition=999;receive(idle);assert(replies.back().result==APPLIED&&!(replies.back().reserved&Show::ACTIVE)&&showRunner.sequence==4); // Keepalive cannot confirm an untargeted old scene.
 // Actual master scheduler with 10 controllers, readiness and CRC-bound storage ACKs.
 asMaster=true;bootSession=77;mockNow=10000;
 for(int i=0;i<10;i++){
  auto &n=nodes[i];n.used=n.localScoreSupported=true;n.deviceBoot=700+i;n.mac[0]=0xc0;n.mac[5]=i+1;
  Plan p;memcpy(p.mac,n.mac,6);p.cues.assign(cues,cues+2);p.cues[0].dimmers[0].value+=i;p.checksum=Show::crc32(p.cues.data(),p.cues.size()*sizeof(Show::Cue));showMaster.plans.push_back(p);
 }
 assert(!publishedReady()&&!goPublished(0,0)&&showMaster.sequence==0);
 auto stored=[&](int i,bool good){Rx r;memcpy(r.mac,nodes[i].mac,6);r.p=packet(SCORE_ACK,77);r.p.deviceBoot=nodes[i].deviceBoot;r.p.channel=0;r.p.reserved=Show::LOADED;r.p.result=APPLIED;r.p.commandId=12;r.p.seq=showMaster.plans[i].checksum+(good?0:1);r.p.value=2;handleShowAck(r);};
 for(int i=0;i<9;i++)stored(i,true);stored(9,false);assert(storedPlans()==9&&!goPublished(0,0));stored(9,true);showMaster.dirty=true;assert(publishedReady()&&!goPublished(0,0)&&showMaster.sequence==0);showMaster.dirty=false;assert(publishedReady()&&goPublished(0,0)&&showMaster.sequence==1);
 for(mockNow=10000;mockNow<10400;mockNow++)showMasterTick();
 std::array<int,10> counts{};std::array<uint32_t,10> first{};
 for(auto &frame:network.sent){assert(frame.bytes.size()==44);Show::RunPacket r;memcpy(&r,frame.bytes.data(),sizeof(r));assert(r.type==Show::RUN&&r.version==12&&r.total==2&&r.reserved==Show::ACTIVE);int i=frame.mac[5]-1;assert(r.checksum==showMaster.plans[i].checksum&&r.deviceBoot==nodes[i].deviceBoot);if(counts[i]++==0)first[i]=frame.at;else assert(frame.at-first[i]==200);}
 for(auto n:counts)assert(n==2);assert(first[9]-first[0]==90); // Dispatch span only, NOT measured physical latency.
 Rx ack;memcpy(ack.mac,nodes[0].mac,6);ack.p=packet(SCORE_ACK,77);ack.p.deviceBoot=700;ack.p.channel=1;ack.p.commandId=showMaster.run;ack.p.seq=1;ack.p.value=0;ack.p.reserved=Show::ACTIVE;ack.p.result=APPLIED;handleShowAck(ack);assert(currentSceneConfirmed(nodes[0])&&!strcmp(sceneStatus(nodes[0]),"applied"));
 assert(goPublished(1,0)&&!currentSceneConfirmed(nodes[0])&&!strcmp(sceneStatus(nodes[0]),"waiting_ack"));
 ack.p.result=NOT_READY;ack.p.reserved|=Show::MISSING_PLAN;handleShowAck(ack);assert(publishedReady()); // Stale negative ACK cannot invalidate a newer GO.
 ack.p.seq=2;handleShowAck(ack);assert(!publishedReady()&&nodes[0].scoreLoaded==0&&!strcmp(sceneStatus(nodes[0]),"loading"));
 nodes[0].scoreNext=1;nodes[0].nextStore=mockNow+100;ack.p.uptime++;handleShowAck(ack);assert(nodes[0].scoreNext==1&&nodes[0].nextStore==mockNow+100); // Repeated misses cannot starve recovery.
 network.sent.clear();for(;mockNow<10600;mockNow++)showMasterTick();bool advanced=false;for(const auto &f:network.sent)if(f.bytes.size()==sizeof(Show::Packet)){Show::Packet s;memcpy(&s,f.bytes.data(),sizeof(s));if(s.type==Show::STORE&&f.mac[5]==1){assert(s.step==1);advanced=true;}}assert(advanced);
 stored(0,true);assert(publishedReady());ack.p.result=APPLIED;ack.p.reserved=Show::ACTIVE;ack.p.value=1;handleShowAck(ack);assert(currentSceneConfirmed(nodes[0]));
 auto late=ack;late.p.result=NOT_READY;late.p.reserved|=Show::MISSING_PLAN;late.p.uptime--;handleShowAck(late);assert(publishedReady()&&currentSceneConfirmed(nodes[0])); // Older same-transition failure cannot undo a success.
 mockNow+=OFFLINE_MS;assert(!currentSceneConfirmed(nodes[0]));
 // Explicit preview is still full-size and cannot alter the stored publication.
 showMaster.preview=showMaster.plans;showMaster.isPreview=true;showMaster.previewTotal=2;showMaster.sequence++;for(auto &n:nodes)n.nextScore=0;
 mockNow+=10;network.sent.clear();showMasterTick();assert(network.sent.size()==1&&network.sent[0].bytes.size()==sizeof(Show::Packet));Show::Packet draft;memcpy(&draft,network.sent[0].bytes.data(),sizeof(draft));assert(draft.type==Show::PREVIEW&&draft.version==0);
 showMaster.programPreview=true;showMaster.step=1;showMaster.sequence++;for(auto &n:nodes)n.nextScore=0;mockNow+=10;network.sent.clear();showMasterTick();assert(network.sent.size()==1);memcpy(&draft,network.sent[0].bytes.data(),sizeof(draft));assert(draft.type==Show::PREVIEW&&draft.step==1&&draft.cue.dimmers[0].value==180&&showMaster.version==12); // Full RAM bank follows the master's selected index, not cue0.
 puts("Actual scene delivery: 44B indexed RUN from CRC-bound flash plans, no legacy full-cue fallback; explicit draft PREVIEW, preload barrier, 200ms/10-node scheduler, stale/negative ACK isolation, current-scene confirmation and STOP latch passed");
}

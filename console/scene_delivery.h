#pragma once
// Executed by the controller task. One master owns the published counter.
// New GO uses a local plan reference; only explicit draft PREVIEW carries outputs.
size_t storedPlans(){
 size_t ready=0;for(const auto &plan:showMaster.plans){int i=findNode(plan.mac,false);if(i<0)continue;const auto &n=nodes[i];
  if(n.deviceBoot&&n.localScoreSupported&&n.scoreLoaded==showMaster.version&&n.scoreChecksum==plan.checksum)ready++;
 }return ready;
}
bool publishedReady(){return showMaster.version&&!showMaster.plans.empty()&&storedPlans()==showMaster.plans.size();}
bool goPublished(uint16_t target,uint8_t reason){
 if(!RADIO_MASTER||networkResetActive||otaSafetyLock||(!showMaster.livePreview&&showMaster.draftUnsaved())||!publishedReady()||(!showMaster.cursorValid&&!globalVoiceReady()))return false;
 if(!showMaster.go(target,reason))return false;startupShow.cancel();
 for(auto &n:nodes)n.nextScore=0;return true;
}
bool goDraft(uint16_t target,uint8_t reason){
 if(!RADIO_MASTER||networkResetActive||otaSafetyLock||(!showMaster.cursorValid&&!globalVoiceReady())||!showMaster.goDraft(target,reason))return false;startupShow.cancel();
 for(auto &n:nodes)n.nextScore=0;return true;
}
bool currentSceneConfirmed(const Node &n){return showMaster.active&&showMaster.find(n.mac,showMaster.isPreview)&&n.scoreAt&&n.scoreStep==showMaster.step&&n.scoreSeq==showMaster.sequence&&n.scoreResult==APPLIED&&millis()-n.scoreAt<OFFLINE_MS;}
const char *sceneStatus(const Node &n){
 auto plan=showMaster.find(n.mac,showMaster.isPreview);if(!plan)return "idle";
 if(!n.localScoreSupported)return "upgrade";
 if(!showMaster.isPreview&&(n.scoreLoaded!=showMaster.version||n.scoreChecksum!=plan->checksum))return "loading";
 if(!showMaster.active)return "ready";
 if(currentSceneConfirmed(n))return "applied";
 if(n.scoreSeq==showMaster.sequence&&n.scoreResult!=APPLIED)return "rejected";
 return "waiting_ack";
}
void handleShowAck(const Rx &r){
 int i=findNode(r.mac,false);if(i<0)return;Node &n=nodes[i];if(n.deviceBoot!=r.p.deviceBoot||!n.localScoreSupported)return;
 n.hasReply=true;n.lastReply=millis();n.lastMasterRssi=r.rssi;n.lastSlaveRssi=r.p.rssi;
 if(r.p.channel==0){
  auto plan=showMaster.find(n.mac);if(!plan||r.p.seq!=plan->checksum||r.p.value>plan->cues.size())return;
  n.scoreNext=r.p.value;
  if((r.p.reserved&Show::LOADED)&&r.p.result==APPLIED&&r.p.commandId==showMaster.version){n.scoreLoaded=r.p.commandId;n.scoreChecksum=r.p.seq;n.scoreStoredUptime=r.p.uptime;}
  return;
 }
 if(r.p.channel!=1)return;
 n.scoreReceived++;uint32_t sentAt=r.p.op|((uint32_t)r.p.padding[0]<<8)|((uint32_t)r.p.padding[1]<<16)|((uint32_t)r.p.padding[2]<<24);
 n.scoreRtt=(uint32_t)(millis()-sentAt)<60000?(uint32_t)(millis()-sentAt):-1;
 // A late ACK for an older step must never confirm/invalidate the current one.
 if(r.p.commandId==showMaster.run&&r.p.seq==showMaster.sequence){
  if(n.scoreFeedbackAt&&(int32_t)(r.p.uptime-n.scoreFeedbackUptime)<0)return;
  if(n.scoreFeedbackAt&&r.p.uptime==n.scoreFeedbackUptime&&n.scoreSeq==r.p.seq&&n.scoreAt&&n.scoreResult==APPLIED&&r.p.result!=APPLIED)return;
  n.scoreFeedbackAt=millis();n.scoreFeedbackUptime=r.p.uptime;
  n.scoreSeq=r.p.seq;n.scoreResult=r.p.result;
  if((r.p.reserved&Show::ACTIVE)&&r.p.result==APPLIED){n.scoreAt=millis();n.scoreStep=r.p.value;}else n.scoreAt=0;
  if(!showMaster.isPreview&&(r.p.reserved&Show::MISSING_PLAN)&&(!n.scoreLoaded||(int32_t)(r.p.uptime-n.scoreStoredUptime)>0)){
   // Reset only when losing an acknowledged plan. Repeated RUN misses must NOT
   // reset the STORE cursor, or 200ms RUN can starve multi-cue recovery forever.
   if(n.scoreLoaded||n.scoreChecksum){n.scoreNext=0;n.nextStore=0;}
   n.scoreLoaded=n.scoreChecksum=0;
  }
 }
 if(!n.reportAt||(int32_t)(r.p.uptime-n.reportUptime)>=0){n.reported=r.p.state;n.reportAt=millis();n.reportUptime=r.p.uptime;}
}
bool handleShowMessage(const LanMessage &message){
 if(message.length!=sizeof(Show::RunPacket)&&message.length!=sizeof(Show::Packet))return false;
 Show::Packet p{};memcpy(&p,message.payload,message.length);
 if(RADIO_MASTER||p.magic!=Show::MAGIC||p.protocol!=Show::PROTOCOL||p.size!=message.length||!masterKnown||memcmp(message.mac,masterMAC,6)||p.session!=masterSession||p.deviceBoot!=bootSession){network.rxInvalid++;return true;}
 bool indexed=message.length==sizeof(Show::RunPacket);
 if((indexed&&p.type!=Show::RUN)||(!indexed&&p.type!=Show::STORE&&p.type!=Show::PREVIEW)){network.rxInvalid++;return true;}
 masterSeen=lastProbe=millis();probeReceivedAt=millis();slaveHealth=p.health<=OFFLINE?(Health)p.health:BAD;
 bool accepted=false,missing=false;
 if(!otaSafetyLock){
  if(p.type==Show::STORE)accepted=showFSReady&&showStore.accept(p);
  else if(p.reserved==Show::ACTIVE&&p.run&&p.transition){
   if(indexed){auto lookup=showStore.resolve(Show::reference(p),p.cue);missing=lookup==Show::SlaveStore::Lookup::MISSING;
    if(lookup==Show::SlaveStore::Lookup::READY)accepted=showRunner.apply(p,millis(),showRecovery&&showRunner.active);
   }else accepted=showRunner.apply(p,millis(),showRecovery&&showRunner.active);
   accepted=accepted&&showRunner.active&&showRunner.run==p.run&&showRunner.sequence==p.transition&&showRunner.step==p.step;
   if(accepted)showRecovery=false;
  }else accepted=p.reserved==0;
 }
 Packet reply=packet(SCORE_ACK,masterSession);reply.deviceBoot=bootSession;reply.channel=p.type==Show::STORE?0:1;reply.result=accepted?APPLIED:NOT_READY;reply.rssi=message.rssi;reply.state=actors.snapshot();
 reply.op=p.sentAt&255;reply.padding[0]=(p.sentAt>>8)&255;reply.padding[1]=(p.sentAt>>16)&255;reply.padding[2]=(p.sentAt>>24)&255;
 if(p.type==Show::STORE){reply.commandId=showStore.version;reply.seq=p.checksum;reply.value=showStore.nextMissing();if(showStore.version==p.version&&showStore.checksum==p.checksum)reply.reserved|=Show::LOADED;}
 else{reply.commandId=p.run;reply.seq=p.transition;reply.value=showRunner.step;if(accepted&&p.reserved==Show::ACTIVE&&showRunner.active)reply.reserved|=Show::ACTIVE;if(missing)reply.reserved|=Show::MISSING_PLAN;}
 sendPacket(masterMAC,reply);return true;
}
void showMasterTick(){
 if(!RADIO_MASTER||!network.ready()||networkResetActive)return;
 if(showMaster.syncPending&&!showMaster.draftUnsaved()&&publishedReady())showMaster.syncPending=false;
 static uint32_t slot=0;uint32_t now=millis();if((int32_t)(now-slot)<0)return;
 static int cursor=0;
 for(int k=0;k<MAX_NODES;k++){
  int i=cursor;cursor=(cursor+1)%MAX_NODES;Node &n=nodes[i];if(!n.used||!n.deviceBoot||!n.localScoreSupported||(uint32_t)network.peerIP(n.mac)==0)continue;
  Show::Packet p{};p.magic=Show::MAGIC;p.protocol=Show::PROTOCOL;p.size=sizeof(p);p.session=bootSession;p.deviceBoot=n.deviceBoot;p.run=showMaster.run;p.transition=showMaster.sequence;p.sentAt=now;p.health=metrics(n).health;
  if((int32_t)(now-n.nextScore)>=0){
   n.nextScore=now+(showMaster.active?Show::REPEAT_MS:Show::IDLE_MS);
   p.type=showMaster.isPreview?Show::PREVIEW:Show::RUN;
   auto plan=showMaster.find(n.mac,showMaster.isPreview);
   if(showMaster.active&&plan){uint16_t index=showMaster.isPreview?showMaster.previewIndex():showMaster.step;if(index<plan->cues.size()){
    p.reserved=Show::ACTIVE;p.version=showMaster.isPreview?0:showMaster.version;p.checksum=plan->checksum;p.step=showMaster.step;p.total=showMaster.isPreview?showMaster.previewTotal:showMaster.stepIds.size();p.reason=showMaster.reason;
    if(showMaster.isPreview)p.cue=plan->cues[index];
   }}
   if(p.type==Show::RUN){auto ref=Show::reference(p);network.send(n.mac,(const uint8_t*)&ref,sizeof(ref));}
   else network.send(n.mac,(const uint8_t*)&p,sizeof(p));
   n.scoreSent++;slot=now+10;return;
  }
  auto plan=showMaster.find(n.mac);
  if(plan&&(n.scoreLoaded!=showMaster.version||n.scoreChecksum!=plan->checksum)&&(int32_t)(now-n.nextStore)>=0){
   n.nextStore=now+100;p.type=Show::STORE;p.version=showMaster.version;p.checksum=plan->checksum;p.total=plan->cues.size();p.step=n.scoreNext<p.total?n.scoreNext:0;p.cue=plan->cues[p.step];network.send(n.mac,(const uint8_t*)&p,sizeof(p));slot=now+10;return;
  }
 }
}
void invalidateShowDelivery(){for(auto &n:nodes){n.nextScore=n.nextStore=0;n.scoreLoaded=n.scoreChecksum=n.scoreNext=n.scoreAt=0;}}

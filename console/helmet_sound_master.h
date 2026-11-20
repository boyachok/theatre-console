#pragma once
bool globalVoiceReady(){
 if(helmetSounds.fault)return false;
 for(const auto &s:helmetSounds.slots){if(!showMaster.find(s.mac))continue;int i=findNode(s.mac,false);if(i<0)return false;const auto &n=nodes[i];if(!n.helmet||!n.soundSupported||n.soundRevision!=helmetSounds.revision||n.soundHash!=s.checksum)return false;}
 return true;
}
void handleGlobalVoiceAck(const Rx &r){
 int i=findNode(r.mac,false);if(i<0)return;auto &n=nodes[i];auto *s=helmetSounds.find(r.mac);
 if(!s||!n.helmet||!n.soundSupported||n.deviceBoot!=r.p.deviceBoot||r.p.result!=APPLIED||r.p.seq!=helmetSounds.revision||r.p.commandId!=s->checksum)return;
 n.soundRevision=r.p.seq;n.soundHash=r.p.commandId;
}
void globalVoiceTick(){
 if(!RADIO_MASTER)return;uint32_t now=millis();if(!otaSafetyLock)helmetSounds.tick(now);
 if(!network.ready()||networkResetActive||otaSafetyLock)return;
 for(auto &s:helmetSounds.slots){bool assigned=showMaster.find(s.mac)!=nullptr;if(!assigned)for(const auto &p:showMaster.draftPlans)if(!memcmp(p.mac,s.mac,6)){assigned=true;break;}if(!assigned)continue;int i=findNode(s.mac,false);if(i<0)continue;auto &n=nodes[i];
  if(!n.helmet||!n.soundSupported||!n.deviceBoot||(uint32_t)network.peerIP(n.mac)==0||now-n.lastReply>=OFFLINE_MS)continue;
  if(n.soundRevision==helmetSounds.revision&&n.soundHash==s.checksum)continue;
  if(n.nextSound&&(int32_t)(now-n.nextSound)<0)continue;n.nextSound=now+250;
  HelmetSound::Packet p;p.session=bootSession;p.deviceBoot=n.deviceBoot;p.revision=helmetSounds.revision;p.profile=s.profile;p.checksum=s.checksum;network.send(n.mac,(const uint8_t*)&p,sizeof(p));return;
 }
}
void bootShowTick(){
 if(!RADIO_MASTER||!startupShow.waiting())return;
 if(showMaster.active||showMaster.cursorValid){startupShow.cancel();return;}
 bool ready=radioReady&&network.ready()&&!networkResetActive&&!otaSafetyLock&&!showMaster.workspaceFault&&publishedReady()&&globalVoiceReady();
 if(ready)for(const auto &plan:showMaster.plans){int i=findNode(plan.mac,false);if(i<0||!nodes[i].hasReply||millis()-nodes[i].lastReply>=OFFLINE_MS||nodes[i].maintenance||!nodes[i].reported.ready||nodes[i].reported.fault){ready=false;break;}}
 if(startupShow.take(ready))goPublished(0,1);
}

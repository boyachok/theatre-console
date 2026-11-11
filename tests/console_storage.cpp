#define THEATRE_HOST_TEST
#include <string>
#include <cassert>
#include <cstdio>
using String=std::string;
#include "../console/score_store.h"
String payload(uint32_t version,bool animate=false,bool delay=false){
 JsonDocument doc,cue;deserializeJson(cue,R"({"dimMask":255,"relayMask":3,"d":[[0,200,400],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],"r":[[0,0,0,0],[0,0,0,0]],"a":[0,30,1,0,0],"stripAction":0})");
 auto dims=cue["d"].as<JsonArray>();while(dims.size()<12){auto extra=dims.add<JsonArray>();extra.add(0);extra.add(0);extra.add(0);}cue["dimMask"]=4095;cue["invertedMask"]=2049;cue["inversionSet"]=4095;
 if(animate)for(auto d:dims){auto a=d.as<JsonArray>();a.add(4);a.add(1000);a.add(0xffffffffu);a.add(25);a.add(50);a.add(100);}
 if(delay){cue["a"].as<JsonArray>().add(1250);cue["a"].as<JsonArray>().add(1);for(auto d:dims)d.as<JsonArray>().add(5678);cue["ac"]["enabled"]=1;auto ac=cue["ac"]["d"].to<JsonArray>();for(unsigned v:{127u,100u,200u,1u,1000u,0u,0u,50u,0u,3456u})ac.add(v);}
 doc["version"]=version;String fp="p"+std::to_string(version);doc["fingerprint"]=fp;auto steps=doc["steps"].to<JsonArray>();for(int i=0;i<100;i++)steps.add("s"+std::to_string(i));
 auto devices=doc["devices"].to<JsonArray>();for(int d=0;d<10;d++){auto device=devices.add<JsonObject>();char mac[18];snprintf(mac,sizeof(mac),"A0:00:00:00:00:%02X",d+1);device["id"]="device"+std::to_string(d);device["mac"]=mac;auto cues=device["cues"].to<JsonArray>();for(int s=0;s<100;s++)cues.add(cue.as<JsonObject>());}
 String workspace="{\"format\":\"theatre-score\",\"schemaVersion\":1,\"published\":{\"version\":"+std::to_string(version)+",\"fingerprint\":\""+fp+"\"},\"padding\":\""+String(1789000,'x')+"\"}";doc["workspaceJson"]=workspace;String text;serializeJson(doc,text);return text;
}
int main(){
 uint64_t floor=0,number=0;assert(Show::backupFloor("0",floor)&&floor==0&&Show::backupFloor("1000000000000",floor)&&floor==1000000000000ULL);assert(!Show::backupFloor("-1",floor)&&!Show::backupFloor("1.5",floor)&&!Show::backupFloor("9007199254740991",floor));
 assert(Show::reserveBackupSequence("project-a",1,0,number)&&number==1);assert(Show::reserveBackupSequence("project-a",1,0,number)&&number==2);assert(Show::reserveBackupSequence("project-a",1,50,number)&&number==51);assert(Show::reserveBackupSequence("project-a",2,0,number)&&number==1);assert(Show::reserveBackupSequence("project-b",1,0,number)&&number==1);
 auto ledger=*LittleFS.files["/backup-sequences.json"];LittleFS.failBackupRename=true;assert(!Show::reserveBackupSequence("project-a",1,0,number)&&*LittleFS.files["/backup-sequences.json"]==ledger);LittleFS.failBackupRename=false;assert(Show::reserveBackupSequence("project-a",1,0,number)&&number==52);assert(!Show::reserveBackupSequence("project-a",0,0,number));ledger=*LittleFS.files["/backup-sequences.json"];LittleFS.files["/backup-sequences.json"]->back()^=1;assert(!Show::reserveBackupSequence("project-a",1,0,number));*LittleFS.files["/backup-sequences.json"]=ledger;
 JsonDocument encoded;deserializeJson(encoded,payload(1,true));auto raw=encoded["devices"][0]["cues"][0];Show::Cue decoded{};assert(Show::decodeCue(raw,decoded)&&decoded.animations[0].mode==Show::RANDOM&&decoded.animations[0].phaseSeed==0xffffffffu&&decoded.invertedMask==2049&&decoded.inversionSet==4095);
 raw["inversionSet"]=0;assert(!Show::decodeCue(raw,decoded));raw["inversionSet"]=4095;raw["invertedMask"]=4096;assert(!Show::decodeCue(raw,decoded));raw["invertedMask"]=2049;
 raw["d"][0][4]=19;assert(!Show::decodeCue(raw,decoded));raw["d"][0][4]=1000;raw["d"][0][3]=1;assert(!Show::decodeCue(raw,decoded));raw["d"][0][5]=1800;assert(Show::decodeCue(raw,decoded));raw["d"][0][6]=101;assert(!Show::decodeCue(raw,decoded));
 raw["d"][0][6]=25;raw["a"].as<JsonArray>().add(1250);assert(Show::decodeCue(raw,decoded)&&Show::audioDelay(decoded)==1250);raw["a"][5]=3600001;assert(!Show::decodeCue(raw,decoded));raw["a"][5]=-1;assert(!Show::decodeCue(raw,decoded));raw["a"][5]=1.5;assert(!Show::decodeCue(raw,decoded));
 raw["a"][5]=1250;raw["a"].as<JsonArray>().add(1);raw["d"][0].as<JsonArray>().add(1234);raw["ac"]["enabled"]=1;auto ac=raw["ac"]["d"].to<JsonArray>();for(unsigned v:{255u,100u,200u,500u})ac.add(v);assert(Show::decodeCue(raw,decoded)&&decoded.audioLoop==1&&decoded.lightDelayMs[0]==1234&&decoded.ac.enabled&&decoded.ac.delayMs==500);raw["a"][6]=2;assert(!Show::decodeCue(raw,decoded));raw["a"][6]=1;raw["ac"]["enabled"]=2;assert(!Show::decodeCue(raw,decoded));raw["ac"]["enabled"]=1;raw["ac"]["d"][3]=3600001;assert(!Show::decodeCue(raw,decoded));raw["ac"]["d"][3]=500;raw["d"][0][9]=1.1;assert(!Show::decodeCue(raw,decoded));
 Show::MasterStore store;String error,one=payload(1);bool ok=store.loadPayload(one,true,error);if(!ok)fprintf(stderr,"Publish failed: %s\n",error.c_str());assert(ok);assert(store.version==1&&store.plans.size()==10&&store.plans[0].cues.size()==100);
 String two=payload(2);assert(store.loadPayload(two,true,error));assert(store.version==2&&LittleFS.peakBytes<0x3f0000);
 Show::MasterStore restored;restored.begin();assert(restored.version==2&&restored.stepIds.size()==100);
 auto saved=*LittleFS.files["/workspace.json"];assert(saved.size()<100000);LittleFS.files["/workspace.json"]->back()^=0x80;Show::MasterStore damaged;damaged.begin();assert(damaged.workspaceFault);*LittleFS.files["/workspace.json"]=saved;
 LittleFS.failPlanRename=true;String three=payload(3);assert(store.loadPayload(three,true,error));assert(store.version==3);Show::MasterStore interrupted;interrupted.begin();assert(interrupted.version==3);LittleFS.failPlanRename=false;Show::MasterStore recovered;recovered.begin();assert(recovered.version==3&&LittleFS.files.count("/published.json.tmp")==0);
 String next=payload(4);JsonDocument uncommitted;deserializeJson(uncommitted,next);uncommitted.remove("workspaceJson");String stage;serializeJson(uncommitted,stage);assert(Show::stageText("/published.json.tmp",stage.c_str()));Show::MasterStore beforeCommit;beforeCommit.begin();assert(beforeCommit.version==3);
 String invalid=payload(4);JsonDocument bad;deserializeJson(bad,invalid);bad["devices"][1]["mac"]=bad["devices"][0]["mac"];invalid.clear();serializeJson(bad,invalid);assert(!store.loadPayload(invalid,true,error)&&store.version==3);
 // Actual master draft bank: patches are atomic/revision-bound and remote cursor survives edits/reorder.
 JsonDocument draft;deserializeJson(draft,payload(4));draft.remove("workspaceJson");draft["base"]="";draft["revision"]=0;draft["fingerprint"]="draft-a";
 auto text=[&](JsonDocument &d){String out;serializeJson(d,out);return out;};
 assert(store.draftPayload(text(draft),error)&&!store.livePreview&&store.version==3&&store.draftRevision==1);
 assert(store.goDraft(49,0)&&store.cursorValid&&store.cursorId()=="s49"&&store.previewIndex()==49);auto navigation=store.navigation,sequence=store.sequence;assert(Show::reserveBackupSequence("project-a",1,0,number)&&number==53&&store.navigation==navigation&&store.sequence==sequence&&store.livePreview&&store.active);
 auto patch=[&](int step,int value,const char *fp){JsonDocument delta;delta["base"]=store.draftFingerprint;delta["revision"]=store.draftRevision;delta["fingerprint"]=fp;auto p=delta["patches"].to<JsonArray>().add<JsonObject>();p["id"]="device0";p["step"]=step;p["cue"]=draft["devices"][0]["cues"][step];p["cue"]["d"][0][0]=value;return text(delta);};
 auto update=patch(49,123,"draft-b");assert(store.draftPayload(update,error)&&store.sequence==sequence+1&&store.navigation==navigation&&store.draftPlans[0].cues[49].dimmers[0].value==123);
 assert(!store.draftPayload(update,error)&&store.sequence==sequence+1); // Replay cannot reapply even if data is valid.
 sequence=store.sequence;assert(store.draftPayload(patch(2,33,"draft-c"),error)&&store.sequence==sequence&&store.step==49);
 assert(store.draftPayload(patch(49,123,"draft-a"),error));assert(!store.draftPayload(update,error)); // ABA fingerprint reuse still rejects the stale revision.
 auto brokenPatch=patch(49,999,"bad");assert(!store.draftPayload(brokenPatch,error)&&store.draftFingerprint=="draft-a"&&store.draftPlans[0].cues[49].dimmers[0].value==123);
 draft["base"]=store.draftFingerprint;draft["revision"]=store.draftRevision;draft["fingerprint"]="draft-d";draft["steps"][7]="s49";draft["steps"][49]="s7";
 assert(store.draftPayload(text(draft),error)&&store.cursorId()=="s49"&&store.step==7&&store.navigation==navigation+1);
 assert(store.queueShow()&&!store.livePreview&&!store.active&&store.syncPending&&store.cursorId()=="s49"&&store.step==49);
 assert(store.loadPayload(payload(4),true,error)&&store.cursorValid&&store.step==49&&!store.active&&store.syncPending); // Synchronization cannot queue a delayed GO.
 assert(store.goDraft(7,0));assert(store.queueShow("invalid-editor")&&!store.livePreview&&!store.active&&store.draftUnsaved()); // OFF works before validation/publish.
 assert(store.goDraft(7,0));assert(store.loadPayload(payload(5),true,error)&&store.cursorId()=="s49"&&store.step==49&&!store.active); // Direct publish from LIVE also preserves the actual master's ID.
 JsonDocument expanded;deserializeJson(expanded,payload(6));expanded["steps"].as<JsonArray>().add("new-step");for(auto d:expanded["devices"].as<JsonArray>())d["cues"].as<JsonArray>().add(d["cues"][0]);
 String publishExpanded=text(expanded);expanded.remove("workspaceJson");expanded["base"]=store.draftFingerprint;expanded["revision"]=store.draftRevision;
 assert(store.draftPayload(text(expanded),error)&&store.goDraft(100,0)&&store.cursorId()=="new-step");
 assert(store.queueShow("p6")&&store.cursorId()=="new-step"&&store.step==100); // Newly added scene need not exist in the OLD publication.
 assert(store.loadPayload(publishExpanded,true,error)&&store.cursorId()=="new-step"&&store.step==100&&!store.active&&!store.draftUnsaved());
 assert(store.loadPayload(payload(7,true,true),true,error));Show::MasterStore animated;animated.begin();assert(animated.version==7&&Show::audioDelay(animated.plans[0].cues[0])==1250&&animated.plans.size()==10&&animated.plans[0].cues.size()==100&&animated.plans[9].cues[99].animations[11].phaseSeed==0xffffffffu&&animated.plans[9].cues[99].lightDelayMs[11]==5678&&animated.plans[9].cues[99].audioLoop==1&&animated.plans[9].cues[99].ac.delayMs==3456&&animated.plans[9].cues[99].invertedMask==2049&&LittleFS.peakBytes<0x3f0000);
 printf("Master programming: full RAM draft + atomic patches/revision/replay, immediate active-cue edits only in preview, no movement on nonselected edits, stable-ID cursor reorder, show sync without delayed execution passed\n");
 printf("Master persistence: repeated 100-step/10-device publications, flash peak %zu < 4128768; restart/journal recovery and uncommitted/invalid rejection passed\n",LittleFS.peakBytes);
}

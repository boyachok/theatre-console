#define THEATRE_HOST_TEST
#include <string>
#include <cassert>
#include <cstdio>
using String=std::string;
#include "../console/score_store.h"
String payload(uint32_t version){
 JsonDocument doc,cue;deserializeJson(cue,R"({"dimMask":255,"relayMask":3,"d":[[0,200,400],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],"r":[[0,0,0,0],[0,0,0,0]],"a":[0,30,1,0,0],"stripAction":0})");
 auto dims=cue["d"].as<JsonArray>();while(dims.size()<12){auto extra=dims.add<JsonArray>();extra.add(0);extra.add(0);extra.add(0);}cue["dimMask"]=4095;
 doc["version"]=version;String fp="p"+std::to_string(version);doc["fingerprint"]=fp;auto steps=doc["steps"].to<JsonArray>();for(int i=0;i<100;i++)steps.add("s"+std::to_string(i));
 auto devices=doc["devices"].to<JsonArray>();for(int d=0;d<10;d++){auto device=devices.add<JsonObject>();char mac[18];snprintf(mac,sizeof(mac),"A0:00:00:00:00:%02X",d+1);device["id"]="device"+std::to_string(d);device["mac"]=mac;auto cues=device["cues"].to<JsonArray>();for(int s=0;s<100;s++)cues.add(cue.as<JsonObject>());}
 String workspace="{\"format\":\"theatre-score\",\"schemaVersion\":1,\"published\":{\"version\":"+std::to_string(version)+",\"fingerprint\":\""+fp+"\"},\"padding\":\""+String(1789000,'x')+"\"}";doc["workspaceJson"]=workspace;String text;serializeJson(doc,text);return text;
}
int main(){
 Show::MasterStore store;String error,one=payload(1);bool ok=store.loadPayload(one,true,error);if(!ok)fprintf(stderr,"Publish failed: %s\n",error.c_str());assert(ok);assert(store.version==1&&store.plans.size()==10&&store.plans[0].cues.size()==100);
 String two=payload(2);assert(store.loadPayload(two,true,error));assert(store.version==2&&LittleFS.peakBytes<0x3f0000);
 Show::MasterStore restored;restored.begin();assert(restored.version==2&&restored.stepIds.size()==100);
 auto saved=*LittleFS.files["/workspace.json"];assert(saved.size()<100000);LittleFS.files["/workspace.json"]->back()^=0x80;Show::MasterStore damaged;damaged.begin();assert(damaged.workspaceFault);*LittleFS.files["/workspace.json"]=saved;
 LittleFS.failPlanRename=true;String three=payload(3);assert(store.loadPayload(three,true,error));assert(store.version==3);Show::MasterStore interrupted;interrupted.begin();assert(interrupted.version==3);LittleFS.failPlanRename=false;Show::MasterStore recovered;recovered.begin();assert(recovered.version==3&&LittleFS.files.count("/published.json.tmp")==0);
 String next=payload(4);JsonDocument uncommitted;deserializeJson(uncommitted,next);uncommitted.remove("workspaceJson");String stage;serializeJson(uncommitted,stage);assert(Show::stageText("/published.json.tmp",stage.c_str()));Show::MasterStore beforeCommit;beforeCommit.begin();assert(beforeCommit.version==3);
 String invalid=payload(4);JsonDocument bad;deserializeJson(bad,invalid);bad["devices"][1]["mac"]=bad["devices"][0]["mac"];invalid.clear();serializeJson(bad,invalid);assert(!store.loadPayload(invalid,true,error)&&store.version==3);
 printf("Master persistence: repeated 100-step/10-device publications, flash peak %zu < 4128768; restart/journal recovery and uncommitted/invalid rejection passed\n",LittleFS.peakBytes);
}

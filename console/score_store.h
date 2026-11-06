#pragma once
#define ARDUINOJSON_STRING_LENGTH_SIZE 4
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <vector>
#include "score_wire.h"
#include "workspace_codec.h"
namespace Show {
class JsonRam:public ArduinoJson::Allocator {
 void *allocate(size_t n)override{return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
 void deallocate(void *p)override{heap_caps_free(p);}
 void *reallocate(void *p,size_t n)override{return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
};
inline JsonRam jsonRam;
inline String readText(const char *path){if(!strncmp(path,"/workspace.json",15))return readWorkspaceFile(path);auto f=LittleFS.open(path,"r");return f?f.readString():String();}
inline bool stageText(const char *path,const char *body){auto f=LittleFS.open(path,"w");if(!f)return false;if(!strncmp(path,"/workspace.json",15)){bool ok=writeWorkspace(f,body);f.close();return ok;}size_t n=strlen(body);bool ok=f.write((const uint8_t*)body,n)==n;f.close();return ok;}
inline uint32_t workspaceVersion(const String &body,String &fingerprint){JsonDocument filter,doc(&jsonRam);filter["published"]["version"]=true;filter["published"]["fingerprint"]=true;if(deserializeJson(doc,body,DeserializationOption::Filter(filter)))return 0;fingerprint=doc["published"]["fingerprint"].as<String>();return doc["published"]["version"]|0;}
inline bool atomicText(const char *path,const String &body){String temp=String(path)+".tmp";return stageText(temp.c_str(),body.c_str())&&LittleFS.rename(temp,path);}
inline bool parseMAC(const char *s,uint8_t *out){unsigned n[6];if(!s||strlen(s)!=17||sscanf(s,"%x:%x:%x:%x:%x:%x",n,n+1,n+2,n+3,n+4,n+5)!=6)return false;for(int i=0;i<6;i++){if(n[i]>255)return false;out[i]=n[i];}return out[0]&&!(out[0]&1);}
inline bool bounded(JsonVariantConst v,uint32_t maximum){return v.is<uint32_t>()&&v.as<uint32_t>()<=maximum;}
inline bool decodeCue(JsonVariantConst source,Cue &cue){
 if(!source.is<JsonObjectConst>()||!bounded(source["dimMask"],4095)||!bounded(source["relayMask"],3))return false;
 cue={};cue.dimmerMask=source["dimMask"];cue.relayMask=source["relayMask"];
 auto dim=source["d"].as<JsonArrayConst>(),rel=source["r"].as<JsonArrayConst>(),audio=source["a"].as<JsonArrayConst>();
 if((dim.size()!=8&&dim.size()!=12)||rel.size()!=2||audio.size()!=5)return false;
 for(unsigned i=0;i<dim.size();i++){if(dim[i].size()!=3||!bounded(dim[i][0],255)||!bounded(dim[i][1],3600000)||!bounded(dim[i][2],3600000))return false;cue.dimmers[i]={dim[i][0],dim[i][1],dim[i][2]};}
 for(int i=0;i<2;i++){if(rel[i].size()!=4||!bounded(rel[i][0],1)||!bounded(rel[i][1],2)||!bounded(rel[i][2],3600000)||!bounded(rel[i][3],3600000))return false;cue.relays[i]={rel[i][0],rel[i][1],rel[i][2],rel[i][3]};}
 if(!bounded(audio[0],65535)||!bounded(audio[1],30)||!bounded(audio[2],1)||!bounded(audio[3],3600000)||!bounded(audio[4],3600000))return false;
 cue.audio={audio[0],audio[1],audio[2],audio[3],audio[4]};
 auto s=source["s"].as<JsonArrayConst>();
 if(s.size()){
  const uint32_t max[17]={1,2,2,1024,255,255,255,255,255,255,100,100,1,3600000,0xffffffffUL,1,1};
  if(s.size()!=17)return false;for(unsigned i=0;i<17;i++)if(!bounded(s[i],max[i]))return false;
  cue.strip={s[0],s[1],s[2],s[4],s[5],s[6],s[7],s[8],s[9],s[10],s[11],s[12],s[15],s[16],s[3],s[13],s[14]};
 }else{if(source["stripAction"].as<int>()==2)return false;cue.strip.action=source["stripAction"]|0;}
 return valid(cue);
}
struct Plan {uint8_t mac[6]{};String id;std::vector<Cue> cues;uint32_t checksum=0;};
class MasterStore {
public:
 std::vector<Plan> plans,preview,draftPlans;std::vector<String> stepIds,draftStepIds;
 String draftFingerprint,syncFingerprint,pendingCursor;bool livePreview=false,programPreview=false,cursorValid=false,syncPending=false;uint32_t navigation=0,draftRevision=0;
 String cursorId()const{if(cursorValid&&pendingCursor.length())return pendingCursor;const auto &ids=programPreview?draftStepIds:stepIds;return cursorValid&&step<ids.size()?ids[step]:String();}
 bool draftUnsaved()const{return (syncFingerprint.length()&&syncFingerprint!=fingerprint)||(draftFingerprint.length()&&draftFingerprint!=fingerprint);}
 size_t navigationCount()const{return livePreview?draftStepIds.size():stepIds.size();}
 uint16_t previewIndex()const{return programPreview?step:0;}
 void bumpSequence(){if(!++sequence)++sequence;}
 void bumpNavigation(){if(!++navigation)++navigation;}
 bool goDraft(uint16_t target,uint8_t why){if(target>=draftStepIds.size()||draftPlans.empty())return false;if(livePreview&&programPreview&&active&&target==step){bumpNavigation();return true;}pendingCursor=String();livePreview=programPreview=isPreview=active=cursorValid=true;syncPending=false;step=target;previewTotal=draftStepIds.size();reason=why;bumpSequence();bumpNavigation();return true;}
 bool queueShow(const String &expected=String()){if(expected.length())syncFingerprint=expected;String id=cursorId();pendingCursor=id;uint16_t next=step;for(size_t i=0;i<stepIds.size();i++)if(stepIds[i]==id){next=i;break;}step=next;livePreview=programPreview=isPreview=active=false;syncPending=true;bumpNavigation();return true;}
 uint32_t version=0,run=0,sequence=0;uint16_t step=0;uint8_t reason=0;uint16_t previewTotal=1;bool active=false,isPreview=false;
 String fingerprint,workspace;bool workspaceFault=false;
 bool decodePlans(JsonVariantConst compiled,std::vector<Plan> &out,uint16_t count,String &error){
  auto devices=compiled.as<JsonArrayConst>();if(!devices.size()||devices.size()>10){error="Нужно от 1 до 10 привязанных приборов";return false;}
  out.clear();for(auto d:devices){Plan p;p.id=d["id"].as<String>();if(!p.id.length()||!parseMAC(d["mac"],p.mac)){error="Не назначена MAC контроллера";return false;}
   for(auto &other:out)if(!memcmp(other.mac,p.mac,6)){error="Один контроллер назначен дважды";return false;}
   auto cues=d["cues"].as<JsonArrayConst>();if(cues.size()!=count){error="Неверное число шагов";return false;}p.cues.resize(count);
   for(int i=0;i<count;i++)if(!decodeCue(cues[i],p.cues[i])){error="Неверные параметры или неподдерживаемая лента";return false;}
   p.checksum=crc32(p.cues.data(),p.cues.size()*sizeof(Cue));out.push_back(std::move(p));
  }return true;
 }
 bool draftPayload(const String &body,String &error){
  JsonDocument doc(&jsonRam);if(deserializeJson(doc,body)){error="Неверный JSON черновика";return false;}
  if(!doc["revision"].is<uint32_t>()||doc["revision"].as<uint32_t>()!=draftRevision||doc["base"].as<String>()!=draftFingerprint){error="Черновик уже изменён другим запросом: обновите страницу";return false;}
  String fp=doc["fingerprint"].as<String>();if(!fp.length()||fp.length()>40){error="Нет отпечатка черновика";return false;}
  std::vector<Plan> decoded;std::vector<String> ids;
  if(doc["devices"].is<JsonArrayConst>()){
   auto steps=doc["steps"].as<JsonArrayConst>();if(!steps.size()||steps.size()>MAX_STEPS){error="Неверные шаги черновика";return false;}
   for(auto s:steps){String id=s.as<String>();if(!id.length()){error="Нет ID шага";return false;}for(const auto &old:ids)if(id==old){error="Повтор ID шага";return false;}ids.push_back(id);}
   if(!decodePlans(doc["devices"],decoded,ids.size(),error))return false;
  }else{
   if(draftPlans.empty()||!doc["patches"].is<JsonArrayConst>()){error="Сначала нужен полный черновик";return false;}
   decoded=draftPlans;ids=draftStepIds;std::vector<uint32_t> seen;bool affected[10]{};
   for(auto patch:doc["patches"].as<JsonArrayConst>()){
    String id=patch["id"].as<String>();size_t device=0;for(;device<decoded.size()&&decoded[device].id!=id;device++);
    if(device==decoded.size()||!bounded(patch["step"],ids.size()-1)){error="Неверный patch черновика";return false;}
    uint16_t index=patch["step"];uint32_t key=device*MAX_STEPS+index;for(auto old:seen)if(old==key){error="Повтор patch";return false;}seen.push_back(key);affected[device]=true;
    if(!decodeCue(patch["cue"],decoded[device].cues[index])){error="Неверные параметры patch";return false;}
   }
   for(size_t i=0;i<decoded.size();i++)if(affected[i])decoded[i].checksum=crc32(decoded[i].cues.data(),decoded[i].cues.size()*sizeof(Cue));
  }
  String current=cursorId();uint16_t next=0;bool matched=false;for(size_t i=0;i<ids.size();i++)if(ids[i]==current){next=i;matched=true;break;}
  if(!matched&&cursorValid)next=std::min<size_t>(step,ids.size()-1);
  bool changed=next!=step||ids.size()!=draftStepIds.size()||decoded.size()!=draftPlans.size();
  if(livePreview&&active)for(const auto &old:draftPlans){
   const Plan *replacement=nullptr;for(const auto &p:decoded)if(!memcmp(old.mac,p.mac,6)){replacement=&p;break;}
   if(!replacement){error="Сначала выключите предпросмотр перед отключением контроллера";return false;}
   if(step>=old.cues.size()||memcmp(&old.cues[step],&replacement->cues[next],sizeof(Cue)))changed=true;
  }
  draftPlans=std::move(decoded);draftStepIds=std::move(ids);draftFingerprint=fp;if(!livePreview)syncFingerprint=fp;if(!++draftRevision)++draftRevision;
  if(livePreview){bool moved=cursorValid&&(next!=step||current!=draftStepIds[next]);step=next;previewTotal=draftStepIds.size();if(moved)bumpNavigation();if(active&&changed){reason=0;bumpSequence();}}
  return true;
 }
 bool loadPayload(const String &body,bool persist,String &error){
  JsonDocument doc(&jsonRam);auto result=deserializeJson(doc,body);if(result){error="Неверный JSON";return false;}
  uint32_t next=doc["version"]|0;auto steps=doc["steps"].as<JsonArrayConst>();
  if(!next||!steps.size()||steps.size()>MAX_STEPS||(persist&&next!=version+1)){error="Версия изменилась или слишком много шагов";return false;}
  std::vector<Plan> decoded;if(!decodePlans(doc["devices"],decoded,steps.size(),error))return false;
  const char *nextWorkspace=doc["workspaceJson"]|"";
  if(persist){size_t length=strlen(nextWorkspace);if(length<10||length>2200000){error="Неверная партитура";return false;}
   JsonDocument filter,meta(&jsonRam);filter["format"]=true;filter["schemaVersion"]=true;filter["published"]["version"]=true;filter["published"]["fingerprint"]=true;
   if(deserializeJson(meta,nextWorkspace,DeserializationOption::Filter(filter))||meta["format"]!="theatre-score"||meta["schemaVersion"]!=1||meta["published"]["version"]!=next||meta["published"]["fingerprint"]!=doc["fingerprint"]){error="Не совпадает версия партитуры";return false;}
  }
  std::vector<String> ids;for(auto s:steps){String id=s.as<String>();if(!id.length()){error="Нет ID шага";return false;}for(auto &old:ids)if(id==old){error="Повтор ID шага";return false;}ids.push_back(id);}
  if(persist){
   // Finalize a previously committed journal before reusing its staging filename.
   String pending=readText("/published.json.tmp");if(pending.length()){JsonDocument head(&jsonRam);if(!deserializeJson(head,pending)&&(head["version"]|0U)==version&&!LittleFS.rename("/published.json.tmp","/published.json")){error="Ошибка журнала flash";return false;}}
   if(!stageText("/workspace.json.tmp",nextWorkspace)){error="Ошибка flash";return false;}
   doc.remove("workspaceJson");String planText;serializeJson(doc,planText);
   // Keep the workspace only once. This fits 100 steps x 10 devices plus both staged files.
   if(!stageText("/published.json.tmp",planText.c_str())||!LittleFS.rename("/workspace.json.tmp","/workspace.json")){error="Ошибка flash";return false;}
   // The workspace rename is the commit marker; begin() can finish this final rename after a power failure.
   LittleFS.rename("/published.json.tmp","/published.json");workspace=String();workspace=readText("/workspace.json");workspaceFault=!workspace.length();active=false;preview.clear();isPreview=false;
  }
  String current=cursorId();bool hadCursor=cursorValid,wasLive=livePreview;uint16_t oldStep=step;
  plans=std::move(decoded);stepIds=std::move(ids);version=next;fingerprint=doc["fingerprint"].as<String>();syncFingerprint=fingerprint;
  if(persist){uint16_t target=std::min<size_t>(step,stepIds.size()-1);for(size_t i=0;i<stepIds.size();i++)if(stepIds[i]==current){target=i;break;}step=target;cursorValid=hadCursor;pendingCursor=String();livePreview=programPreview=false;syncPending=true;if(wasLive||(hadCursor&&(target!=oldStep||current!=stepIds[target])))bumpNavigation();}
  return true;
 }
 void begin(){
  workspace=readText("/workspace.json");workspaceFault=LittleFS.exists("/workspace.json")&&!workspace.length();String fp,error;uint32_t committed=workspaceVersion(workspace,fp);
  String staged=readText("/published.json.tmp");bool recovered=false;
  if(committed&&staged.length()){JsonDocument head(&jsonRam);if(!deserializeJson(head,staged)&&(head["version"]|0U)==committed&&head["fingerprint"]==fp&&loadPayload(staged,false,error)){LittleFS.rename("/published.json.tmp","/published.json");recovered=true;}}
  if(!recovered){String body=readText("/published.json");if(body.length())loadPayload(body,false,error);}
 }
 Plan *find(const uint8_t *mac,bool forPreview=false){auto &list=forPreview?(programPreview?draftPlans:preview):plans;for(auto &p:list)if(!memcmp(mac,p.mac,6))return &p;return nullptr;}
 bool go(uint16_t target,uint8_t why){if(target>=stepIds.size())return false;step=target;reason=why;pendingCursor=String();livePreview=programPreview=isPreview=false;active=cursorValid=true;syncPending=false;bumpSequence();bumpNavigation();return true;}
 bool previewPayload(const String &body,String &error){
  JsonDocument doc(&jsonRam);if(deserializeJson(doc,body)){error="Неверный JSON";return false;}
  std::vector<Plan> decoded;if(!decodePlans(doc["devices"],decoded,1,error))return false;
  uint16_t total=doc["total"]|0,target=doc["step"]|0;if(!total||total>MAX_STEPS||target>=total){error="Неверный шаг";return false;}previewTotal=total;
  pendingCursor=String();preview=std::move(decoded);step=target;reason=doc["reason"]|0;livePreview=programPreview=false;isPreview=true;active=cursorValid=true;bumpSequence();bumpNavigation();return true;
 }
};
}

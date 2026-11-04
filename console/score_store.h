#pragma once
#define ARDUINOJSON_STRING_LENGTH_SIZE 4
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <vector>
#include "score_wire.h"
namespace Show {
class JsonRam:public ArduinoJson::Allocator {
 void *allocate(size_t n)override{return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
 void deallocate(void *p)override{heap_caps_free(p);}
 void *reallocate(void *p,size_t n)override{return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
};
inline JsonRam jsonRam;
inline String readText(const char *path){auto f=LittleFS.open(path,"r");return f?f.readString():String();}
inline bool stageText(const char *path,const char *body){auto f=LittleFS.open(path,"w");if(!f)return false;size_t n=strlen(body);bool ok=f.write((const uint8_t*)body,n)==n;f.close();return ok;}
inline uint32_t workspaceVersion(const String &body,String &fingerprint){JsonDocument filter,doc(&jsonRam);filter["published"]["version"]=true;filter["published"]["fingerprint"]=true;if(deserializeJson(doc,body,DeserializationOption::Filter(filter)))return 0;fingerprint=doc["published"]["fingerprint"].as<String>();return doc["published"]["version"]|0;}
inline bool atomicText(const char *path,const String &body){String temp=String(path)+".tmp";auto f=LittleFS.open(temp,"w");if(!f)return false;bool ok=f.print(body)==body.length();f.close();return ok&&LittleFS.rename(temp,path);}
inline bool parseMAC(const char *s,uint8_t *out){unsigned n[6];if(!s||strlen(s)!=17||sscanf(s,"%x:%x:%x:%x:%x:%x",n,n+1,n+2,n+3,n+4,n+5)!=6)return false;for(int i=0;i<6;i++){if(n[i]>255)return false;out[i]=n[i];}return out[0]&&!(out[0]&1);}
inline bool bounded(JsonVariantConst v,uint32_t maximum){return v.is<uint32_t>()&&v.as<uint32_t>()<=maximum;}
inline bool decodeCue(JsonVariantConst source,Cue &cue){
 if(!source.is<JsonObjectConst>()||!bounded(source["dimMask"],255)||!bounded(source["relayMask"],3))return false;
 cue={};cue.dimmerMask=source["dimMask"];cue.relayMask=source["relayMask"];
 auto dim=source["d"].as<JsonArrayConst>(),rel=source["r"].as<JsonArrayConst>(),audio=source["a"].as<JsonArrayConst>();
 if(dim.size()!=8||rel.size()!=2||audio.size()!=5)return false;
 for(int i=0;i<8;i++){if(dim[i].size()!=3||!bounded(dim[i][0],255)||!bounded(dim[i][1],3600000)||!bounded(dim[i][2],3600000))return false;cue.dimmers[i]={dim[i][0],dim[i][1],dim[i][2]};}
 for(int i=0;i<2;i++){if(rel[i].size()!=4||!bounded(rel[i][0],1)||!bounded(rel[i][1],2)||!bounded(rel[i][2],3600000)||!bounded(rel[i][3],3600000))return false;cue.relays[i]={rel[i][0],rel[i][1],rel[i][2],rel[i][3]};}
 if(!bounded(audio[0],65535)||!bounded(audio[1],30)||!bounded(audio[2],1)||!bounded(audio[3],3600000)||!bounded(audio[4],3600000))return false;
 cue.audio={audio[0],audio[1],audio[2],audio[3],audio[4]};
 // Strip output is intentionally unavailable until its electrical protocol is confirmed.
 if(source["stripAction"].as<int>()==2)return false;
 return valid(cue);
}
struct Plan {uint8_t mac[6]{};String id;std::vector<Cue> cues;uint32_t checksum=0;};
class MasterStore {
public:
 std::vector<Plan> plans,preview;std::vector<String> stepIds;
 uint32_t version=0,run=0,sequence=0;uint16_t step=0;uint8_t reason=0;uint16_t previewTotal=1;bool active=false,isPreview=false;
 String fingerprint,workspace;
 bool decodePlans(JsonVariantConst compiled,std::vector<Plan> &out,uint16_t count,String &error){
  auto devices=compiled.as<JsonArrayConst>();if(!devices.size()||devices.size()>10){error="Нужно от 1 до 10 привязанных приборов";return false;}
  out.clear();for(auto d:devices){Plan p;p.id=d["id"].as<String>();if(!p.id.length()||!parseMAC(d["mac"],p.mac)){error="Не назначена MAC контроллера";return false;}
   for(auto &other:out)if(!memcmp(other.mac,p.mac,6)){error="Один контроллер назначен дважды";return false;}
   auto cues=d["cues"].as<JsonArrayConst>();if(cues.size()!=count){error="Неверное число шагов";return false;}p.cues.resize(count);
   for(int i=0;i<count;i++)if(!decodeCue(cues[i],p.cues[i])){error="Неверные параметры или неподдерживаемая лента";return false;}
   p.checksum=crc32(p.cues.data(),p.cues.size()*sizeof(Cue));out.push_back(std::move(p));
  }return true;
 }
 bool loadPayload(const String &body,bool persist,String &error){
  JsonDocument doc(&jsonRam);auto result=deserializeJson(doc,body);if(result){error="Неверный JSON";return false;}
  uint32_t next=doc["version"]|0;auto steps=doc["steps"].as<JsonArrayConst>();
  if(!next||!steps.size()||steps.size()>MAX_STEPS||(persist&&next!=version+1)){error="Версия изменилась или слишком много шагов";return false;}
  std::vector<Plan> decoded;if(!decodePlans(doc["devices"],decoded,steps.size(),error))return false;
  const char *nextWorkspace=doc["workspaceJson"]|"";
  if(persist){size_t length=strlen(nextWorkspace);if(length<10||length>1500000){error="Неверная партитура";return false;}
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
   LittleFS.rename("/published.json.tmp","/published.json");workspace=readText("/workspace.json");active=false;preview.clear();isPreview=false;
  }
  plans=std::move(decoded);stepIds=std::move(ids);version=next;fingerprint=doc["fingerprint"].as<String>();return true;
 }
 void begin(){
  workspace=readText("/workspace.json");String fp,error;uint32_t committed=workspaceVersion(workspace,fp);
  String staged=readText("/published.json.tmp");bool recovered=false;
  if(committed&&staged.length()){JsonDocument head(&jsonRam);if(!deserializeJson(head,staged)&&(head["version"]|0U)==committed&&head["fingerprint"]==fp&&loadPayload(staged,false,error)){LittleFS.rename("/published.json.tmp","/published.json");recovered=true;}}
  if(!recovered){String body=readText("/published.json");if(body.length())loadPayload(body,false,error);}
 }
 Plan *find(const uint8_t *mac,bool forPreview=false){auto &list=forPreview?preview:plans;for(auto &p:list)if(!memcmp(mac,p.mac,6))return &p;return nullptr;}
 bool go(uint16_t target,uint8_t why){if(target>=stepIds.size())return false;step=target;reason=why;isPreview=false;active=true;if(!++sequence)++sequence;return true;}
 bool previewPayload(const String &body,String &error){
  JsonDocument doc(&jsonRam);if(deserializeJson(doc,body)){error="Неверный JSON";return false;}
  std::vector<Plan> decoded;if(!decodePlans(doc["devices"],decoded,1,error))return false;
  uint16_t total=doc["total"]|0,target=doc["step"]|0;if(!total||total>MAX_STEPS||target>=total){error="Неверный шаг";return false;}previewTotal=total;
  preview=std::move(decoded);step=target;reason=doc["reason"]|0;isPreview=true;active=true;if(!++sequence)++sequence;return true;
 }
};
}

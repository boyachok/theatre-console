#pragma once
#include "helmet_sound_json.h"
#include "raw_workspace_patch.h"
// Small independent file, so a voice slider never rewrites the multi-MB score or restarts a scene.
class HelmetSoundStore {
 JsonDocument data{&Show::jsonRam};bool dirty=false;uint32_t changedAt=0,retryAt=0;
public:
 struct Slot {uint8_t mac[6]{};HelmetSound::Profile profile{};uint32_t checksum=0;};std::vector<Slot> slots;
 bool fault=false;uint32_t revision=1;
 bool validateBanks(JsonVariantConst banks){if(!banks.is<JsonObjectConst>()||banks.size()>10)return false;uint8_t seen[10][6]{};int n=0;for(auto kv:banks.as<JsonObjectConst>()){uint8_t mac[6];HelmetSound::Profile p;if(!Show::parseMAC(kv.key().c_str(),mac)||!HelmetSound::bank(kv.value(),p))return false;for(int i=0;i<n;i++)if(!memcmp(seen[i],mac,6))return false;memcpy(seen[n++],mac,6);}return true;}
 static String key(const uint8_t *mac){char text[18];snprintf(text,sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);return String(text);}
 void rebuild(){slots.clear();for(auto kv:data["banks"].as<JsonObjectConst>()){Slot s;if(Show::parseMAC(kv.key().c_str(),s.mac)&&HelmetSound::bank(kv.value(),s.profile)){s.checksum=HelmetSound::hash(s.profile);slots.push_back(s);}}}
 void begin(const String &workspace){
  String file=Show::readText("/helmet-voices.json");
  if(file.length()){if(deserializeJson(data,file)||data["format"]!="helmet-voices-1"||!data["revision"].is<uint32_t>()||!data["revision"].as<uint32_t>()||!validateBanks(data["banks"])){fault=true;data.clear();return;}revision=data["revision"];rebuild();return;}
  if(LittleFS.exists("/helmet-voices.json")){fault=true;return;}
  data["format"]="helmet-voices-1";data["revision"]=revision;data["banks"].to<JsonObject>();
  JsonDocument filter,old(&Show::jsonRam);filter["helmetVoices"]=true;filter["draft"]["devices"][0]["profile"]=true;filter["draft"]["devices"][0]["mac"]=true;filter["draft"]["devices"][0]["voice"]=true;
  if(workspace.length()&&!deserializeJson(old,workspace,DeserializationOption::Filter(filter))){
   if(!old["helmetVoices"].isNull()){if(!validateBanks(old["helmetVoices"])){fault=true;return;}data["banks"].set(old["helmetVoices"]);}
   else for(auto d:old["draft"]["devices"].as<JsonArrayConst>())if(d["profile"]=="helmet"){HelmetSound::Profile p;String mac=d["mac"].as<String>();uint8_t bytes[6];if(Show::parseMAC(mac.c_str(),bytes)&&HelmetSound::bank(d["voice"],p))data["banks"][mac].set(d["voice"]);}
  }
  if(data.overflowed()){fault=true;return;}
  rebuild(); // Legacy fallback stays untouched on disk until an explicit global voice edit.
 }
 Slot *find(const uint8_t *mac){for(auto &s:slots)if(!memcmp(s.mac,mac,6))return &s;return nullptr;}
 bool set(const char *mac,JsonVariantConst bank,uint32_t expected,uint32_t now,String &error){
  uint8_t bytes[6];HelmetSound::Profile p;if(fault){error="Voice storage is damaged; preserve files and restore JSON explicitly";return false;}
  if(expected!=revision){error="Voice bank changed in another editor; reload";return false;}
  if(!Show::parseMAC(mac,bytes)||!HelmetSound::bank(bank,p)){error="Invalid voice bank";return false;}
  String canonical=key(bytes);for(auto kv:data["banks"].as<JsonObjectConst>()){uint8_t old[6];if(Show::parseMAC(kv.key().c_str(),old)&&!memcmp(old,bytes,6)){canonical=kv.key().c_str();break;}}
  if(data["banks"].size()>=10&&data["banks"][canonical].isNull()){error="Too many helmet banks";return false;}
  JsonDocument next(&Show::jsonRam);if(!next.set(data)||!next["banks"][canonical].set(bank)||next.overflowed()){error="No RAM for voice bank; previous bank preserved";return false;}
  uint32_t r=revision+1;if(!r)r=1;next["revision"]=r;data=std::move(next);revision=r;dirty=true;changedAt=now;rebuild();return true;
 }
 bool importBanks(JsonVariantConst banks,uint32_t expected,uint32_t now,String &error){if(expected!=revision||!validateBanks(banks)){error="Voice revision conflict or invalid bank import";return false;}JsonDocument next(&Show::jsonRam);next["format"]="helmet-voices-1";next["banks"].set(banks);uint32_t r=revision+1;if(!r)r=1;next["revision"]=r;if(next.overflowed()){error="No RAM for import; previous banks preserved";return false;}data=std::move(next);revision=r;fault=false;dirty=true;changedAt=now;rebuild();return true;}
 void tick(uint32_t now){if(fault||!dirty||now-changedAt<1500||(retryAt&&(int32_t)(now-retryAt)<0))return;String body;serializeJson(data,body);if(!data.overflowed()&&body.length()==measureJson(data)&&Show::atomicText("/helmet-voices.json",body)){dirty=false;retryAt=0;}else retryAt=now+30000;}
 bool saved()const{return !dirty&&!fault;}
 String json(){JsonDocument reply(&Show::jsonRam);reply["revision"]=revision;reply["banks"].set(data["banks"]);reply["saved"]=saved();reply["fault"]=fault;String s;serializeJson(reply,s);return s;}
 String decorate(const String &workspace){if(!workspace.length()||workspace=="null")return "null";String banks;serializeJson(data["banks"],banks);if(banks.length()!=measureJson(data["banks"]))return String();char rev[16];snprintf(rev,sizeof(rev),"%lu",(unsigned long)revision);const char *keys[]={"helmetVoices","helmetVoiceRevision"};String values[]={banks,String(rev)};return fault?RawWorkspace::patch(workspace,keys+1,values+1,1):RawWorkspace::patch(workspace,keys,values,2);}
};

#define THEATRE_HOST_TEST
#include <string>
#include <cassert>
#include <cstdio>
using String=std::string;
#include "../console/score_store.h"
#include "../console/helmet_sound_store.h"
#include "../console/raw_workspace_patch.h"
const char *settings=R"({"mode":"robot","algorithm":"autotune","gain":6.5,"volume":1,"frequency":110,"mix":0.5,"drive":4.1,"gateDb":-62,"notch":true,"reduceThumps":true,"breathReduction":0,"wave":0,"toneHz":4500,"formant":0,"clarity":1,"tuneKey":0,"tuneScale":0,"retuneMs":77,"tuneStrength":1,"transpose":-9})";
int main(){
 JsonDocument w;w["format"]="theatre-score";w["schemaVersion"]=1;auto d=w["draft"]["devices"].add<JsonObject>();d["mac"]="C0:4E:30:36:C6:88";d["profile"]="helmet";d["voice"]["selected"]="v1";auto preset=d["voice"]["presets"].add<JsonObject>();preset["id"]="v1";preset["name"]="Голос 1";deserializeJson(preset["settings"],settings);w["published"]["version"]=384;
 String workspace;serializeJson(w,workspace);HelmetSoundStore store;store.begin(workspace);assert(!store.fault&&store.slots.size()==1&&store.revision==1);assert(!LittleFS.exists("/helmet-voices.json"));
 JsonDocument bank;bank.set(d["voice"]);bank["presets"][0]["settings"]["presence"]["enabled"]=true;auto presence=bank["presets"][0]["settings"]["presence"];presence["frequency"]=2500;presence["width"]=2000;presence["drive"]=3;presence["amount"]=.2;
 String error;assert(!store.set("C0:4E:30:36:C6:88",bank.as<JsonVariantConst>(),2,100,error));assert(store.set("C0:4E:30:36:C6:88",bank.as<JsonVariantConst>(),1,100,error));assert(store.revision==2&&store.slots[0].profile.tone.presenceEnabled&&!store.saved());
 store.tick(1599);assert(!LittleFS.exists("/helmet-voices.json"));store.tick(1600);assert(store.saved()&&LittleFS.exists("/helmet-voices.json"));
 HelmetSoundStore restored;restored.begin(workspace);assert(restored.slots.size()==1&&restored.slots[0].profile.tone.presenceAmount>.19f&&restored.revision==2);
 String decorated=restored.decorate(workspace);JsonDocument exported;assert(!deserializeJson(exported,decorated));assert(exported["helmetVoices"]["C0:4E:30:36:C6:88"]["presets"][0]["settings"]["presence"]["enabled"]==true);
 exported["draft"]["devices"][0]["profile"]="standard";String changed;serializeJson(exported,changed);assert(changed!=decorated);assert(workspace.find("presence")==String::npos); // Original legacy bank untouched.
 auto original=*LittleFS.files["/helmet-voices.json"];LittleFS.files["/helmet-voices.json"]->assign({'!'});HelmetSoundStore broken;broken.begin(workspace);assert(broken.fault&&!broken.set("C0:4E:30:36:C6:88",bank.as<JsonVariantConst>(),1,200,error));assert(*LittleFS.files["/helmet-voices.json"]==std::vector<uint8_t>({'!'}));*LittleFS.files["/helmet-voices.json"]=original;
 presence["amount"]=2;assert(!restored.set("C0:4E:30:36:C6:88",bank.as<JsonVariantConst>(),2,200,error));
 const String precise=R"({"draft":{"period":21.029000000000003,"text":"braces ] { and escaped \\\" quote"},"published":null,"helmetVoiceRevision":1})";
 String preciseOut=restored.decorate(precise);assert(preciseOut.find("21.029000000000003")!=String::npos);assert(preciseOut.find("braces ] { and escaped")!=String::npos);JsonDocument precisionCheck;assert(!deserializeJson(precisionCheck,preciseOut));assert(precisionCheck["helmetVoiceRevision"]==2);assert(preciseOut.find("helmetVoiceRevision")==preciseOut.rfind("helmetVoiceRevision"));
 const char *keys[]={"interfaceVersion"};String values[]={"\"0.8.5\""};String metadata=RawWorkspace::patch(precise,keys,values,1);assert(metadata.find("21.029000000000003")!=String::npos&&metadata.find("\"helmetVoiceRevision\":1")!=String::npos);
 puts("Independent global voice banks: lossless legacy fallback, revision conflict/range validation, small debounced atomic file, reload, global banks included in JSON backup, raw JSON patch preserves exact score doubles/escaping and avoids duplicate metadata keys; corrupt file never overwritten");
}

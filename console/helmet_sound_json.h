#pragma once
#include "helmet_sound_wire.h"
namespace HelmetSound {
inline bool settings(JsonVariantConst s,Profile &p){
 if(!s.is<JsonObjectConst>())return false;
 const char *alg=s["algorithm"]|"",*mode=s["mode"]|"";int a=-1;const char *names[]={"ring","vocoder","autotune","monotone","chip"};for(int i=0;i<5;i++)if(!strcmp(alg,names[i]))a=i;
 if(a<0||(strcmp(mode,"dry")&&strcmp(mode,"robot"))||!s["notch"].is<bool>()||!s["reduceThumps"].is<bool>())return false;
 const char *fields[]={"gain","volume","frequency","mix","drive","gateDb","wave","toneHz","formant","clarity","retuneMs","tuneStrength","transpose","tuneKey","tuneScale","breathReduction"};for(auto f:fields)if(!s[f].is<float>())return false;
 if(!s["tuneKey"].is<int>()||!s["tuneScale"].is<int>()||!s["breathReduction"].is<int>())return false;
 p.voice={1,(uint8_t)!strcmp(mode,"robot"),(uint8_t)a,(uint8_t)s["tuneKey"].as<int>(),(uint8_t)s["tuneScale"].as<int>(),(uint8_t)((s["notch"].as<bool>()?1:0)|(s["reduceThumps"].as<bool>()?2:0)),(uint8_t)s["breathReduction"].as<int>(),s["gain"],s["volume"],s["frequency"],s["mix"],s["drive"],s["gateDb"],s["wave"],s["toneHz"],s["formant"],s["clarity"],s["retuneMs"],s["tuneStrength"],s["transpose"]};
 p.tone=Show::VoiceTone{};
 if(!s["eq"].isNull()){
  auto eq=s["eq"];if(!eq["enabled"].is<bool>()||!eq["highpassHz"].is<float>()||!eq["lowpassHz"].is<float>()||eq["bands"].size()!=3)return false;
  p.tone.eqEnabled=eq["enabled"];p.tone.highpassHz=eq["highpassHz"];p.tone.lowpassHz=eq["lowpassHz"];
  for(int i=0;i<3;i++){auto b=eq["bands"][i];if(!b["frequency"].is<float>()||!b["gainDb"].is<float>()||!b["q"].is<float>())return false;p.tone.bands[i]={b["frequency"],b["gainDb"],b["q"]};}
 }
 if(!s["presence"].isNull()){
  auto b=s["presence"];if(!b["enabled"].is<bool>())return false;
  for(auto f:{"frequency","width","drive","amount"})if(!b[f].is<float>())return false;
  p.tone.presenceEnabled=b["enabled"];p.tone.presenceFrequency=b["frequency"];p.tone.presenceWidth=b["width"];p.tone.presenceDrive=b["drive"];p.tone.presenceAmount=b["amount"];
 }
 return s["tuneKey"].as<int>()>=0&&s["tuneKey"].as<int>()<12&&s["tuneScale"].as<int>()>=0&&s["tuneScale"].as<int>()<3&&s["breathReduction"].as<int>()>=0&&s["breathReduction"].as<int>()<=100&&valid(p);
}
inline bool bank(JsonVariantConst b,Profile &p){
 auto presets=b["presets"].as<JsonArrayConst>();if(!presets.size()||presets.size()>8)return false;String selected=b["selected"].as<String>();bool found=false;String ids[8];int index=0;
 for(auto item:presets){String id=item["id"].as<String>(),name=item["name"].as<String>();if(!id.length()||id.length()>160||!name.length()||name.length()>112)return false;for(int k=0;k<index;k++)if(ids[k]==id)return false;ids[index++]=id;Profile decoded;if(!settings(item["settings"],decoded))return false;if(id==selected){p=decoded;found=true;}}
 if(!found)return false;if(!b["working"].isNull()&&!settings(b["working"],p))return false;return true;
}
}

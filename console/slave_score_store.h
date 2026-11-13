#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include "score_wire.h"
namespace Show {
// Shared local plan storage and indexed lookup for regular slaves and helmets.
class SlaveStore {
 uint32_t stagingVersion=0,expectedCRC=0;uint16_t expected=0;
 std::vector<Cue> pending;uint8_t present[MAX_STEPS/8]{};
public:
 enum class Lookup { READY,MISSING,INVALID };
 uint32_t version=0,checksum=0;std::vector<Cue> cues;
 Lookup resolve(const RunPacket &p,Cue &cue)const{
  if(p.type!=RUN||p.reserved!=ACTIVE||!p.run||!p.transition||!p.version||!p.total||p.total>MAX_STEPS||p.step>=p.total)return Lookup::INVALID;
  if(version!=p.version||checksum!=p.checksum||cues.size()!=p.total)return Lookup::MISSING;
  if(!valid(cues[p.step]))return Lookup::INVALID;
  cue=cues[p.step];return Lookup::READY;
 }
 void begin(){
  version=checksum=0;cues.clear();
  auto f=LittleFS.open("/slave-score.bin","r");if(!f)return;FileHeader h{};
  if(f.read((uint8_t*)&h,sizeof(h))!=sizeof(h)||h.magic!=MAGIC||!h.version||!h.total||h.total>MAX_STEPS)return;
  if(h.reserved!=0&&h.reserved!=8&&h.reserved!=9&&h.reserved!=10&&h.reserved!=11&&h.reserved!=PROTOCOL)return;
  size_t stride=!h.reserved?sizeof(LegacyCue):h.reserved==PROTOCOL?sizeof(Cue):h.reserved==11?sizeof(Score11Cue):h.reserved==10?sizeof(Score10Cue):sizeof(PriorCue);if(f.size()!=sizeof(h)+h.total*stride)return;
  if(!h.reserved){
   std::vector<LegacyCue> old(h.total);if(f.read((uint8_t*)old.data(),old.size()*sizeof(LegacyCue))!=old.size()*sizeof(LegacyCue)||crc32(old.data(),old.size()*sizeof(LegacyCue))!=h.checksum)return;
   cues.assign(h.total,Cue{});for(size_t i=0;i<old.size();i++)memcpy(&cues[i],&old[i],sizeof(LegacyCue));
  }else if(h.reserved==11){
   std::vector<Score11Cue> old(h.total);if(f.read((uint8_t*)old.data(),old.size()*sizeof(Score11Cue))!=old.size()*sizeof(Score11Cue)||crc32(old.data(),old.size()*sizeof(Score11Cue))!=h.checksum)return;
   cues.assign(h.total,Cue{});for(size_t i=0;i<old.size();i++)memcpy(&cues[i],&old[i],sizeof(Score11Cue));
  }else if(h.reserved==10){
   std::vector<Score10Cue> old(h.total);if(f.read((uint8_t*)old.data(),old.size()*sizeof(Score10Cue))!=old.size()*sizeof(Score10Cue)||crc32(old.data(),old.size()*sizeof(Score10Cue))!=h.checksum)return;
   cues.assign(h.total,Cue{});for(size_t i=0;i<old.size();i++)memcpy(&cues[i],&old[i],sizeof(Score10Cue));
  }else if(h.reserved!=PROTOCOL){
   std::vector<PriorCue> old(h.total);if(f.read((uint8_t*)old.data(),old.size()*sizeof(PriorCue))!=old.size()*sizeof(PriorCue)||crc32(old.data(),old.size()*sizeof(PriorCue))!=h.checksum)return;
   cues.assign(h.total,Cue{});for(size_t i=0;i<old.size();i++)memcpy(&cues[i],&old[i],sizeof(PriorCue));
  }else{cues.resize(h.total);if(f.read((uint8_t*)cues.data(),cues.size()*sizeof(Cue))!=cues.size()*sizeof(Cue)||crc32(cues.data(),cues.size()*sizeof(Cue))!=h.checksum){cues.clear();return;}}
  if(h.reserved==0||h.reserved==8)for(auto &cue:cues)setAudioDelay(cue,0); // score7/8 used these bytes as reserved, never as a timer.
  for(const auto &cue:cues)if(!valid(cue)){cues.clear();return;}
  version=h.version;checksum=crc32(cues.data(),cues.size()*sizeof(Cue)); // Legacy static cache converted in RAM; never erase it on boot.
 }
 uint16_t nextMissing()const{for(uint16_t i=0;i<expected;i++)if(!(present[i/8]&(1<<(i%8))))return i;return expected;}
 bool accept(const Packet &p){
  if(!p.version||!p.total||p.total>MAX_STEPS||p.step>=p.total||!valid(p.cue))return false;
  if(version==p.version&&checksum==p.checksum)return true;
  if(stagingVersion!=p.version||expectedCRC!=p.checksum){stagingVersion=p.version;expectedCRC=p.checksum;expected=p.total;pending.assign(expected,Cue{});memset(present,0,sizeof(present));}
  if(expected!=p.total)return false;
  pending[p.step]=p.cue;present[p.step/8]|=1<<(p.step%8);
  if(nextMissing()!=expected)return true;
  if(crc32(pending.data(),pending.size()*sizeof(Cue))!=expectedCRC){memset(present,0,sizeof(present));return false;}
  auto file=LittleFS.open("/slave-score.tmp","w");if(!file)return false;
  FileHeader h{MAGIC,stagingVersion,expectedCRC,expected,PROTOCOL};
  bool ok=file.write((uint8_t*)&h,sizeof(h))==sizeof(h)&&file.write((uint8_t*)pending.data(),pending.size()*sizeof(Cue))==pending.size()*sizeof(Cue);file.close();
  if(!ok||!LittleFS.rename("/slave-score.tmp","/slave-score.bin"))return false;
  cues=std::move(pending);version=stagingVersion;checksum=expectedCRC;return true;
 }
};
}

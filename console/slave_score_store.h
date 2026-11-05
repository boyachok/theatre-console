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
  cues.resize(h.total);if(f.read((uint8_t*)cues.data(),cues.size()*sizeof(Cue))!=cues.size()*sizeof(Cue)||crc32(cues.data(),cues.size()*sizeof(Cue))!=h.checksum){cues.clear();return;}
  for(const auto &cue:cues)if(!valid(cue)){cues.clear();return;}
  version=h.version;checksum=h.checksum;
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
  FileHeader h{MAGIC,stagingVersion,expectedCRC,expected,0};
  bool ok=file.write((uint8_t*)&h,sizeof(h))==sizeof(h)&&file.write((uint8_t*)pending.data(),pending.size()*sizeof(Cue))==pending.size()*sizeof(Cue);file.close();
  if(!ok||!LittleFS.rename("/slave-score.tmp","/slave-score.bin"))return false;
  cues=std::move(pending);version=stagingVersion;checksum=expectedCRC;return true;
 }
};
}

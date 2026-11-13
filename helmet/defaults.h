#pragma once
#include <Preferences.h>
#include "score_wire.h"
#include "voice/preset_bank.h"
namespace Helmet {
struct __attribute__((packed)) BootSettings {Show::VoiceProfile voice;Show::Strip strip;Show::StripOptions options;};
struct __attribute__((packed)) BootRecord {uint32_t magic,version;BootSettings settings;uint32_t checksum;};
static_assert(sizeof(BootRecord)==111,"Helmet boot settings record");
inline BootSettings factoryBoot(){BootSettings b{};b.voice={1,0,0,0,0,1,0,8,.15f,85,1,0,-70,0,4500,0,.5f,0,1,0};b.strip={1,2,3,0,0,255,0,0,0,10,15,0,0,0,60,4000,0x424f4f54};b.options={50,25,30,0,0,0,100,50,400,1};return b;}
inline bool validBoot(const BootSettings &b){Show::Cue c{};c.voice=b.voice;c.strip=b.strip;c.stripOptions=b.options;return b.voice.configured&&b.strip.enabled&&b.strip.action==2&&Show::valid(c);}
class Defaults {
 Preferences prefs;BootSettings current{};bool dirty=false,blocked=false;uint32_t due=0;
 bool legacy(){Preferences old;old.begin("robot-voice",true);RVPresetBank bank{};bool ok=false;for(const char *key:{"bank_v1","bank_backup"})if(old.getBytesLength(key)==sizeof(bank)&&old.getBytes(key,&bank,sizeof(bank))==sizeof(bank)&&rv_bank_valid(&bank)){ok=true;break;}if(!ok)return false;const auto &s=bank.slots[bank.active].sound;if(s.mode>1)return false;
  Show::VoiceProfile p{1,(uint8_t)s.mode,(uint8_t)s.algorithm,(uint8_t)s.key,(uint8_t)s.scale,(uint8_t)((s.flags&2?1:0)|(s.flags&RV_LOW_CUT_FLAG?2:0)),(uint8_t)((s.flags&RV_BREATH_MASK)>>RV_BREATH_SHIFT),s.gain,s.volume,s.frequency,s.mix,s.drive,s.gateDb,s.wave,s.toneHz,s.formant,s.clarity,s.retuneMs,s.strength,s.transpose};
  if(!Show::validVoice(p))return false;current.voice=p;return true;
 }
public:
 const char *source="factory";bool saved=false;uint32_t writes=0;
 void begin(){prefs.begin("helmet-start",false);BootRecord record{};size_t size=prefs.getBytesLength("defaults");current=factoryBoot();
  if(size){if(size==sizeof(record)&&prefs.getBytes("defaults",&record,sizeof(record))==sizeof(record)&&record.magic==0x48424f54&&record.version==1&&record.checksum==Show::crc32(&record,offsetof(BootRecord,checksum))&&validBoot(record.settings)){current=record.settings;source="helmet-start/defaults";saved=true;return;}blocked=true;source="invalid-defaults-preserved";return;}
  if(legacy())source="robot-voice/bank";dirty=true;save(); // Before audio starts: seed once without interrupting live I2S.
 }
 const BootSettings &settings()const{return current;}
 // Explicit USB service import only: never overwrite a master's saved sound implicitly on boot.
 bool importLegacy(){if(blocked)return false;auto before=current;const char *oldSource=source;if(!legacy())return false;dirty=true;saved=false;if(!save()){current=before;dirty=false;source=oldSource;return false;}source="robot-voice/imported";return true;}
 void remember(const Show::Cue &cue,uint32_t now){BootSettings next=current;if(cue.voice.configured)next.voice=cue.voice;if(cue.strip.enabled&&cue.strip.action==2){next.strip=cue.strip;next.options=cue.stripOptions;}if(!validBoot(next)||!memcmp(&next,&current,sizeof(next)))return;current=next;dirty=true;saved=false;due=now+2000;}
 bool save(){if(!dirty)return saved;if(blocked)return false;BootRecord r{};r.magic=0x48424f54;r.version=1;r.settings=current;r.checksum=Show::crc32(&r,offsetof(BootRecord,checksum));if(prefs.putBytes("defaults",&r,sizeof(r))!=sizeof(r)){saved=false;return false;}dirty=false;saved=true;writes++;return true;}
 void tick(uint32_t now){if(dirty&&(int32_t)(now-due)>=0){if(!save())due=now+30000;}}
};
}

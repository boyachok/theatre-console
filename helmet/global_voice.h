#pragma once
#include <Preferences.h>
#include "helmet_sound_wire.h"
namespace Helmet {
class GlobalVoice {
 struct __attribute__((packed)) Record {uint32_t magic=0x31564748;uint16_t version=1;HelmetSound::Profile profile{};uint32_t checksum=0;};
 Preferences prefs;bool dirty=false;uint32_t changedAt=0,retryAt=0;
public:
 bool active=false,saved=false;uint32_t writes=0;HelmetSound::Profile profile{};
 void begin(){prefs.begin("helmet-global",false);Record r;if(prefs.getBytesLength("sound-v1")==sizeof(r)&&prefs.getBytes("sound-v1",&r,sizeof(r))==sizeof(r)&&r.magic==0x31564748&&r.version==1&&HelmetSound::valid(r.profile)&&r.checksum==HelmetSound::hash(r.profile)){profile=r.profile;active=saved=true;}}
 bool set(const HelmetSound::Profile &p,uint32_t now){if(!HelmetSound::valid(p))return false;if(active&&!memcmp(&profile,&p,sizeof(p)))return true;profile=p;active=true;saved=false;dirty=true;changedAt=now;return true;}
 void tick(uint32_t now){if(!dirty||now-changedAt<2000||(retryAt&&(int32_t)(now-retryAt)<0))return;Record r;r.profile=profile;r.checksum=HelmetSound::hash(profile);if(prefs.putBytes("sound-v1",&r,sizeof(r))==sizeof(r)){dirty=false;saved=true;writes++;retryAt=0;}else retryAt=now+30000;}
};
}

#pragma once
#include <stdint.h>
#include <cstring>
#include <cmath>
namespace Show {
// A global helmet sound, repeated in compiled cues only to keep each indexed plan self-contained.
// The editor owns one selected bank per DEVICE, never a per-scene sound override.
struct __attribute__((packed)) VoiceProfile {
 uint8_t configured,mode,algorithm,key,scale,flags,breath;
 float gain,volume,frequency,mix,drive,gateDb,wave,toneHz,formant,clarity,retuneMs,strength,transpose;
};
static_assert(sizeof(VoiceProfile)==59,"Helmet profile wire");
inline bool voiceRange(float n,float lo,float hi){return std::isfinite(n)&&n>=lo&&n<=hi;}
inline bool validVoice(const VoiceProfile &p){
 if(!p.configured){VoiceProfile zero{};return !memcmp(&p,&zero,sizeof(p));}
 return p.configured==1&&p.mode<=1&&p.algorithm<=4&&p.key<12&&p.scale<3&&!(p.flags&~3)&&p.breath<=100&&
 voiceRange(p.gain,.25f,64)&&voiceRange(p.volume,0,1)&&voiceRange(p.frequency,p.algorithm==4?20:p.algorithm==3?65:20,p.algorithm==4?1000:400)&&
 voiceRange(p.mix,0,1)&&voiceRange(p.drive,0,12)&&voiceRange(p.gateDb,-90,-25)&&voiceRange(p.wave,0,1)&&
 (p.toneHz==0||voiceRange(p.toneHz,20,20000))&&voiceRange(p.formant,-6,6)&&voiceRange(p.clarity,0,1)&&
 voiceRange(p.retuneMs,0,250)&&voiceRange(p.strength,0,1)&&voiceRange(p.transpose,-12,12);
}
struct __attribute__((packed)) StripOptions {
 uint8_t density,sparkles,noise,palette,reactive,backgroundEnabled,backgroundBrightness,sensitivity;
 uint32_t releaseMs,seed;
};
static_assert(sizeof(StripOptions)==16,"Strip options wire");
inline bool validStripOptions(const StripOptions &o){return o.density<=100&&o.sparkles<=100&&o.noise<=100&&o.palette<=2&&o.reactive<=1&&o.backgroundEnabled<=1&&o.backgroundBrightness<=100&&o.sensitivity<=100&&o.releaseMs<=3600000;}
}

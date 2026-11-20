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
// Independent global voice processing; never enlarges a 512B scene packet.
struct __attribute__((packed)) EqBand {float frequency,gainDb,q;};
struct __attribute__((packed)) VoiceTone {
 uint8_t eqEnabled=0,presenceEnabled=0;
 float highpassHz=0,lowpassHz=0;EqBand bands[3]{{180,0,1},{900,0,1},{2800,0,1}};
 float presenceFrequency=2500,presenceWidth=2200,presenceDrive=2,presenceAmount=0;
};
static_assert(sizeof(VoiceTone)==62,"Independent helmet tone profile");
inline bool validTone(const VoiceTone &p){
 if(p.eqEnabled>1||p.presenceEnabled>1||!(p.highpassHz==0||voiceRange(p.highpassHz,20,1000))||!(p.lowpassHz==0||voiceRange(p.lowpassHz,2000,20000)))return false;
 for(int i=0;i<3;i++)if(!voiceRange(p.bands[i].frequency,30,16000)||!voiceRange(p.bands[i].gainDb,-12,12)||!voiceRange(p.bands[i].q,.2f,8))return false;
 return voiceRange(p.presenceFrequency,80,6000)&&voiceRange(p.presenceWidth,100,6000)&&voiceRange(p.presenceDrive,0,12)&&voiceRange(p.presenceAmount,0,1);
}
struct __attribute__((packed)) StripOptions {
 uint8_t density,sparkles,noise,palette,reactive,backgroundEnabled,backgroundBrightness,sensitivity;
 uint32_t releaseMs,seed;
};
static_assert(sizeof(StripOptions)==16,"Strip options wire");
inline bool validStripOptions(const StripOptions &o){return o.density<=100&&o.sparkles<=100&&o.noise<=100&&o.palette<=2&&o.reactive<=1&&o.backgroundEnabled<=1&&o.backgroundBrightness<=100&&o.sensitivity<=100&&o.releaseMs<=3600000;}
}

#pragma once
#include <Preferences.h>
#include "config.h"
#include "ws2811.h"
namespace Helmet {
// Integration boundary for the future local microphone/ADC/DSP/DAC patch.
// This stub does NOT touch an amplifier, I2S, UART, GPIO or report real audio.
class VoicePort {
 bool desired=false;
public:
 void setEnabled(bool enabled){desired=enabled;}
 bool requested()const{return desired;}
 bool available()const{return false;}
};
inline bool supported(const Show::Cue &c){
 if(!Show::valid(c)||c.dimmerMask||c.relayMask||c.audio.track)return false;
 for(int i=0;i<12;i++)if(c.dimmers[i].value||c.animations[i].mode)return false;
 for(int i=0;i<2;i++)if(c.relays[i].on||c.relays[i].mode)return false;
 return true;
}
class Runner {
 Show::Ws2811 strip{HELMET_STRIP_PIN};
public:
 VoicePort voice;
 bool active=false,ready=false;uint32_t run=0,sequence=0;uint16_t step=0;
 void begin(){ready=strip.begin();}
 void stop(){strip.stop();voice.setEnabled(false);active=false;}
 bool apply(const Show::Packet &p,uint32_t now,bool recovering=false){
  if(!supported(p.cue)||!p.transition)return false;
  if(run==p.run&&!Show::newer(p.transition,sequence)&&!(recovering&&p.transition==sequence))return true;
  if(!strip.apply(p.cue.strip,now,p.reason!=0||recovering))return false;
  voice.setEnabled(p.cue.audio.enabled!=0);
  run=p.run;sequence=p.transition;step=p.step;active=true;tick(now);return true;
 }
 void tick(uint32_t now){strip.tick(now);}
 ControlState snapshot(bool linkFresh)const{
  ControlState s{};s.ready=linkFresh||active;
  s.audioState=s.audioVolume=s.audioMode=s.audioEq=s.audioDrive=255;s.audioAge=65535;
  // audioEnabled stays 0: there is no implemented voice backend yet.
  s.fault=ready?0:5;return s;
 }
};
}

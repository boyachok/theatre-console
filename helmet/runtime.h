#pragma once
#include <Preferences.h>
#include "config.h"
#include "ws2811.h"
#include "voice_engine.h"
#include "defaults.h"
namespace Helmet {
inline bool supported(const Show::Cue &c){
 Show::AcCue noAC{};
 if(!Show::valid(c)||c.dimmerMask||c.inversionSet||c.invertedMask||c.relayMask||c.audio.track||Show::audioDelay(c)||c.audioLoop||memcmp(&c.ac,&noAC,sizeof(noAC)))return false;
 for(int i=0;i<12;i++)if(c.dimmers[i].value||c.animations[i].mode||c.lightDelayMs[i])return false;
 for(int i=0;i<2;i++)if(c.relays[i].on||c.relays[i].mode)return false;
 return true;
}
class Runner {
 Show::Ws2811 strip{HELMET_STRIP_PIN};
public:
 VoicePort voice;Defaults defaults;
 bool diagnostic=false,resumeRequested=false;
 bool active=false,ready=false,standalone=false;uint32_t run=0,sequence=0,lastCueCRC=0;uint16_t step=0;
 void begin(){defaults.begin();ready=strip.begin();voice.begin();const auto &b=defaults.settings();if(ready)strip.apply(b.strip,millis(),true,b.options);voice.configure(b.voice,true);active=standalone=true;}
 void stop(){diagnostic=false;strip.stop();voice.setEnabled(false);active=standalone=false;}
 bool dryTest(){if(!voice.available())return false;if(!diagnostic)resumeRequested=voice.requested();auto p=defaults.settings().voice;p.mode=0;p.gain=8;p.volume=.15f;p.gateDb=-90;if(!voice.configure(p,true))return false;diagnostic=true;return true;}
 bool importLegacy(){if((active&&!standalone)||diagnostic||!voice.available())return false;bool enabled=voice.requested();voice.setEnabled(false);bool ok=defaults.importLegacy();if(ok)voice.configure(defaults.settings().voice,enabled);else voice.setEnabled(enabled);return ok;}
 bool endTest(){if(!diagnostic)return false;voice.configure(defaults.settings().voice,resumeRequested);diagnostic=false;return true;}
 bool apply(const Show::Packet &p,uint32_t now,bool recovering=false){
  if(!supported(p.cue)||!p.transition)return false;
  if(run==p.run&&!Show::newer(p.transition,sequence)&&!(recovering&&active&&p.transition==sequence))return true;
  uint32_t crc=Show::crc32(&p.cue,sizeof(p.cue));if(recovering&&active&&!standalone&&!diagnostic&&step==p.step&&lastCueCRC==crc){run=p.run;sequence=p.transition;tick(now);return true;}
  if(!Show::validVoice(p.cue.voice)||(p.cue.audio.enabled&&!voice.available()))return false;
  if(!strip.apply(p.cue.strip,now,p.reason!=0,p.cue.stripOptions))return false;
  voice.configure(p.cue.voice,p.cue.audio.enabled!=0);diagnostic=false;lastCueCRC=crc;
  run=p.run;sequence=p.transition;step=p.step;active=true;standalone=false;defaults.remember(p.cue,now);tick(now);return true;
 }
 void tick(uint32_t now){strip.tick(now,voice.level());defaults.tick(now);}
 ControlState snapshot(bool linkFresh){
  ControlState s{};s.ready=linkFresh||active;
  s.audioState=s.audioVolume=s.audioMode=s.audioEq=s.audioDrive=255;s.audioAge=65535;
  auto audio=voice.diagnostics();s.audioEnabled=audio.ready&&audio.enabled;s.audioState=audio.ready?(audio.enabled?1:0):255;
  s.fault=ready&&audio.ready?0:5;return s;
 }
};
}

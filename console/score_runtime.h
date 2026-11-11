#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include "actors.h"
#include "score_wire.h"
#include "dimmer_animation.h"
#include "slave_score_store.h"
#include "ws2811.h"
#include "ac_dimmer.h"
namespace Show {
class Runner {
 Actors &actors;Preferences relayConfig;DimmerChannel lights[12],acLight;Ramp volume;Ws2811 strip;AcDimmer ac;bool targetLoop=false,loopUpdate=false;
 uint8_t written[12]{},relayMode2=0,lastVolume=255;bool relay2=false,relayTarget[2]{},relayPending[2]{};
 uint32_t relayDue[2]{},lastTick=0,lastVolumeAt=0,stopAt=0;
 uint16_t targetTrack=0;bool audioPending=false,audioStarting=false,stopPending=false;uint32_t expectedPlay=0;uint8_t targetVolume=0;uint32_t audioFadeIn=0,audioSceneAt=0,audioStartAt=0,audioDelayMs=0;
 void confirmAudioStart(){if(audioStarting&&actors.player.playSerial==expectedPlay){audioStarting=false;volume=Ramp{0.f,0.f,actors.player.playSentAt,0};volume.target(targetVolume,audioFadeIn,0,actors.player.playSentAt);}}
 void writeRelay(uint8_t index,bool on){
  if(index==0){actors.apply(RELAY,0,on,true);return;}
  if(!relayMode2){pinMode(RELAY2_PIN,INPUT);relay2=false;return;}
  digitalWrite(RELAY2_PIN,(on==(relayMode2==1))?HIGH:LOW);pinMode(RELAY2_PIN,OUTPUT);relay2=on;
 }
public:
 bool active=false;uint32_t run=0,sequence=0;uint16_t step=0;
 explicit Runner(Actors &a):actors(a){}
 Mains::State acStatus(){return ac.diagnostics();}
 void begin(){ac.begin();relayConfig.begin("stage-io",false);relayMode2=relayConfig.getUChar("relayMode2",0);if(relayMode2>2)relayMode2=0;writeRelay(1,false);strip.begin();}
 void stopAudio(){targetLoop=loopUpdate=false;audioPending=audioStarting=stopPending=false;targetTrack=0;lastVolume=0;audioSceneAt=audioStartAt=audioDelayMs=0;volume=Ramp{};actors.player.stop();}
 void stop(){ac.stop();strip.stop();stopAudio();active=false;stopAt=0;for(int i=0;i<2;i++)relayPending[i]=false;writeRelay(1,false);actors.off();}
 bool apply(const Packet &p,uint32_t now,bool recovering=false){
  if(!valid(p.cue)||!p.transition)return false;
  if(run==p.run&&!newer(p.transition,sequence)&&!(recovering&&p.transition==sequence))return true;
  const auto &c=p.cue;bool restart=p.reason!=0||recovering;bool fresh=p.run!=run||newer(p.transition,sequence);
  if(!ac.configure(c.ac.enabled,fresh&&(!active||p.step!=step||p.reason!=0)))return false;
  if(!strip.apply(c.strip,now,restart))return false;
  for(int i=0;i<12;i++){
   if(!active)lights[i].reset(actors.dimmerValue(i),now);
   lights[i].apply(c.dimmers[i],c.animations[i],c.dimmerMask&(1<<i),now,!active||restart||p.run!=run||p.step!=step,c.lightDelayMs[i]);
   written[i]=actors.dimmerValue(i);
  }
  if(!active)acLight.reset(0,now);
  acLight.apply(c.ac.dimmer,c.ac.animation,c.ac.enabled,now,!active||restart||p.run!=run||p.step!=step,c.ac.delayMs);
  for(int i=0;i<2;i++){
   if(c.relays[i].mode){if(i==0)actors.apply(RELAY_CONFIG,0,c.relays[i].mode,true);else if(relayMode2!=c.relays[i].mode){bool old=relay2;relayMode2=c.relays[i].mode;relayConfig.putUChar("relayMode2",relayMode2);writeRelay(1,old);}}
   bool on=(c.relayMask&(1<<i))&&c.relays[i].on;bool actual=i==0?actors.snapshot().relayOn:relay2;
   relayTarget[i]=on;relayPending[i]=on!=actual;relayDue[i]=now+(on?c.relays[i].onMs:c.relays[i].offMs);
  }
  confirmAudioStart(); // A PLAY transmitted just before this cue is already running, not a pending start.
  uint16_t track=c.audio.enabled?c.audio.track:0;uint32_t delay=audioDelay(c);bool newScene=!active||p.run!=run||p.step!=step;bool loop=c.audioLoop!=0;
  targetVolume=c.audio.volume;audioFadeIn=c.audio.inMs;stopAt=0;stopPending=false;
  if(!track){if(audioPending||audioStarting)actors.player.stop();volume.target(0,c.audio.inMs,c.audio.outMs,now);stopAt=now+c.audio.outMs;stopPending=true;audioPending=audioStarting=false;}
  else if(!active||track!=targetTrack||restart||((audioPending||audioStarting)&&(newScene||delay!=audioDelayMs||loop!=targetLoop))){
   if(newScene||track!=targetTrack||restart)audioSceneAt=now;
   audioStartAt=audioSceneAt+delay;actors.player.stop();volume=Ramp{0.f,0.f,now,0};audioPending=true;audioStarting=false;lastVolume=255;
  }else if(!audioPending&&!audioStarting)volume.target(c.audio.volume,c.audio.inMs,c.audio.outMs,now);
  if(track&&loop!=targetLoop&&!audioPending&&!audioStarting)loopUpdate=true;
  if(!track||audioPending)loopUpdate=false;
  targetLoop=loop;audioDelayMs=delay;targetTrack=track;run=p.run;sequence=p.transition;step=p.step;active=true;tick(now);return true;
 }
 void tick(uint32_t now,bool acLease=true){
  ac.tick(acLease);strip.tick(now);if(!active||now-lastTick<1)return;lastTick=now;
  for(int i=0;i<12;i++){uint8_t v=(uint8_t)std::max(0.f,std::min(255.f,lights[i].value(now)));if(v!=written[i]){if(actors.setDimmer(i,v))written[i]=v;}}
  ac.level((uint8_t)std::max(0.f,std::min(255.f,acLight.value(now))));
  for(int i=0;i<2;i++)if(relayPending[i]&&(int32_t)(now-relayDue[i])>=0){writeRelay(i,relayTarget[i]);relayPending[i]=false;}
  if(audioPending&&(int32_t)(now-audioStartAt)>=0&&actors.player.bootSettled()&&actors.player.freeSlots()>=3){
   if(actors.player.startTrack(targetTrack,targetLoop)){audioPending=false;audioStarting=true;expectedPlay=actors.player.playSerial+1;if(!expectedPlay)expectedPlay=1;lastVolume=0;}
  }
  confirmAudioStart();
  if(loopUpdate&&!audioPending&&!audioStarting&&actors.player.setRepeat(targetLoop))loopUpdate=false;
  if(!audioPending&&!audioStarting&&now-lastVolumeAt>=100){lastVolumeAt=now;uint8_t v=(uint8_t)std::max(0.f,std::min(30.f,volume.value(now)));if(v!=lastVolume&&actors.player.setVolume(v))lastVolume=v;}
  if(stopPending&&(int32_t)(now-stopAt)>=0){actors.player.stop();stopPending=false;}
 }
};
}

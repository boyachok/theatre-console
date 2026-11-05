#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include "actors.h"
#include "score_wire.h"
#include "slave_score_store.h"
#include "ws2811.h"
namespace Show {
class Runner {
 Actors &actors;Preferences relayConfig;Ramp lights[12],volume;Ws2811 strip;
 uint8_t written[12]{},relayMode2=0,lastVolume=255;bool relay2=false,relayTarget[2]{},relayPending[2]{};
 uint32_t relayDue[2]{},lastTick=0,lastVolumeAt=0,stopAt=0;
 uint16_t targetTrack=0;bool audioPending=false,stopPending=false;uint8_t targetVolume=0;uint32_t audioFadeIn=0;
 void writeRelay(uint8_t index,bool on){
  if(index==0){actors.apply(RELAY,0,on,true);return;}
  if(!relayMode2){pinMode(RELAY2_PIN,INPUT);relay2=false;return;}
  digitalWrite(RELAY2_PIN,(on==(relayMode2==1))?HIGH:LOW);pinMode(RELAY2_PIN,OUTPUT);relay2=on;
 }
public:
 bool active=false;uint32_t run=0,sequence=0;uint16_t step=0;
 explicit Runner(Actors &a):actors(a){}
 void begin(){relayConfig.begin("stage-io",false);relayMode2=relayConfig.getUChar("relayMode2",0);if(relayMode2>2)relayMode2=0;writeRelay(1,false);strip.begin();}
 void stop(){strip.stop();active=false;audioPending=false;stopPending=false;stopAt=0;for(int i=0;i<2;i++)relayPending[i]=false;writeRelay(1,false);actors.off();}
 bool apply(const Packet &p,uint32_t now,bool recovering=false){
  if(!valid(p.cue)||!p.transition)return false;
  if(run==p.run&&!newer(p.transition,sequence)&&!(recovering&&p.transition==sequence))return true;
  const auto &c=p.cue;bool restart=p.reason!=0||recovering;
  if(!strip.apply(c.strip,now,restart))return false;
  for(int i=0;i<12;i++){
   if(!active)lights[i]=Ramp{(float)actors.dimmerValue(i),(float)actors.dimmerValue(i),now,0};
   lights[i].target(c.dimmerMask&(1<<i)?c.dimmers[i].value:0,c.dimmers[i].inMs,c.dimmers[i].outMs,now);
   written[i]=actors.dimmerValue(i);
  }
  for(int i=0;i<2;i++){
   if(c.relays[i].mode){if(i==0)actors.apply(RELAY_CONFIG,0,c.relays[i].mode,true);else if(relayMode2!=c.relays[i].mode){bool old=relay2;relayMode2=c.relays[i].mode;relayConfig.putUChar("relayMode2",relayMode2);writeRelay(1,old);}}
   bool on=(c.relayMask&(1<<i))&&c.relays[i].on;bool actual=i==0?actors.snapshot().relayOn:relay2;
   relayTarget[i]=on;relayPending[i]=on!=actual;relayDue[i]=now+(on?c.relays[i].onMs:c.relays[i].offMs);
  }
  uint16_t track=c.audio.enabled?c.audio.track:0;targetVolume=c.audio.volume;audioFadeIn=c.audio.inMs;
  stopAt=0;stopPending=false;
  if(!track){volume.target(0,c.audio.inMs,c.audio.outMs,now);stopAt=now+c.audio.outMs;stopPending=true;audioPending=false;}
  else if(track!=targetTrack||restart){actors.player.stop();volume=Ramp{0.f,0.f,now,0};volume.target(c.audio.volume,c.audio.inMs,c.audio.outMs,now);audioPending=true;lastVolume=255;}
  else volume.target(c.audio.volume,c.audio.inMs,c.audio.outMs,now);
  targetTrack=track;run=p.run;sequence=p.transition;step=p.step;active=true;tick(now);return true;
 }
 void tick(uint32_t now){
  strip.tick(now);if(!active||now-lastTick<10)return;lastTick=now;
  for(int i=0;i<12;i++){uint8_t v=(uint8_t)std::max(0.f,std::min(255.f,lights[i].value(now)));if(v!=written[i]){if(actors.setDimmer(i,v))written[i]=v;}}
  for(int i=0;i<2;i++)if(relayPending[i]&&(int32_t)(now-relayDue[i])>=0){writeRelay(i,relayTarget[i]);relayPending[i]=false;}
  if(audioPending&&actors.player.bootSettled()&&actors.player.freeSlots()>=3){
   if(actors.player.enqueue(0x13,0,1)&&actors.player.enqueue(0x18,2,1)&&actors.player.enqueue(0x07,targetTrack,2)){audioPending=false;lastVolume=0;volume=Ramp{0.f,0.f,now,0};volume.target(targetVolume,audioFadeIn,0,now);}
  }
  if(!audioPending&&now-lastVolumeAt>=100){lastVolumeAt=now;uint8_t v=(uint8_t)std::max(0.f,std::min(30.f,volume.value(now)));if(v!=lastVolume&&actors.player.enqueue(0x13,v,1))lastVolume=v;}
  if(stopPending&&(int32_t)(now-stopAt)>=0){actors.player.stop();stopPending=false;}
 }
};
}

#pragma once
#include <cmath>
#include "score_wire.h"
namespace Show {
// Stateless hash: channel scheduling/tick frequency cannot consume another channel's RNG.
inline uint32_t dimmerHash(uint32_t seed,uint32_t cycle){uint32_t x=seed+cycle*0x9e3779b9UL;x^=x>>16;x*=0x7feb352dUL;x^=x>>15;x*=0x846ca68bUL;x^=x>>16;return x;}
class DimmerOscillator {
 DimmerAnimation config{};uint32_t lastAt=0,position=0,cycle=0;
 void advance(uint32_t now){if(config.mode){uint64_t elapsed=(uint64_t)position+(uint32_t)(now-lastAt);cycle+=(uint32_t)(elapsed/config.periodMs);position=elapsed%config.periodMs;}lastAt=now;}
public:
 void apply(const DimmerAnimation &next,uint32_t now,bool restart){
  advance(now);
  if(restart||config.mode!=next.mode||config.phaseSeed!=next.phaseSeed){cycle=0;position=next.mode&&next.mode!=RANDOM?(uint64_t)next.periodMs*next.phaseSeed/3600:0;}
  else if(config.mode&&next.mode&&config.periodMs!=next.periodMs)position=(uint64_t)position*next.periodMs/config.periodMs;
  config=next;lastAt=now;
 }
 float factor(uint32_t now){
  advance(now);if(!config.mode)return 1.f;
  double t=(double)position/config.periodMs,wave=0;
  switch(config.mode){
   case SINE:wave=(1.f-std::cos(6.28318530718f*(float)t))*.5f;break;
   case SQUARE:{uint32_t high=(uint64_t)config.periodMs*config.duty/100;if(config.duty&&config.duty<100)high=std::max<uint32_t>(1,std::min<uint32_t>(config.periodMs-1,high));wave=position<high?1:0;break;}
   case RAMP:{double apex=config.duty/100.;wave=apex==0?1-t:apex==1?t:t<apex?t/apex:(1-t)/(1-apex);break;}
   case RANDOM:{double from=(double)dimmerHash(config.phaseSeed,cycle)/4294967295.,to=(double)dimmerHash(config.phaseSeed,cycle+1)/4294967295.;double mix=config.smooth?std::min(1.,t*100/config.smooth):0;mix=mix*mix*(3-2*mix);wave=from+(to-from)*mix;break;}
   default:break;
  }
  return (float)(config.floor/100.+(1-config.floor/100.)*wave);
 }
};
class DimmerChannel {
 DimmerOscillator oscillator;Dimmer previous{};DimmerAnimation animation{};bool configured=false,enabled=false,pending=false;
 float from=0,maximum=0;uint32_t at=0,duration=0,requestedAt=0,due=0,delayMs=0;
 void begin(const Dimmer &d,const DimmerAnimation &effect,bool on,uint32_t now,bool restart,float actual){
  float target=on?d.value:0;duration=target>=maximum?d.inMs:d.outMs;from=actual;maximum=target;at=now;
  DimmerAnimation off{};oscillator.apply(on?effect:off,now,restart);
 }
public:
 void reset(float actual,uint32_t now){from=maximum=actual;at=now;duration=0;configured=pending=false;oscillator=DimmerOscillator{};}
 bool waiting()const{return pending;}
 float value(uint32_t now){
  if(pending){if((int32_t)(now-due)<0)return 0;pending=false;begin(previous,animation,enabled,due,true,0);}
  float mix=duration?std::min(1.f,(float)(uint32_t)(now-at)/duration):1;return from*(1-mix)+maximum*oscillator.factor(now)*mix;
 }
 void apply(const Dimmer &next,const DimmerAnimation &effect,bool on,uint32_t now,bool restart,uint32_t delay=0){
  bool unchanged=configured&&on==enabled&&!memcmp(&next,&previous,sizeof(next))&&!memcmp(&effect,&animation,sizeof(effect));
  if(!restart&&unchanged&&delay==delayMs)return;
  float actual=value(now);bool wasPending=pending;
  if(!restart&&unchanged&&!wasPending){delayMs=delay;return;} // Editing delay does not interrupt an already running light.
  if(on&&next.value&&delay&&(restart||!configured||wasPending||!enabled||maximum==0)){
   if(restart||!wasPending)requestedAt=now;due=requestedAt+delay;pending=true;from=maximum=0;duration=0;
  }else{pending=false;begin(next,effect,on,now,restart||wasPending,actual);}
  previous=next;animation=effect;enabled=on;configured=true;delayMs=delay;
  if(pending&&(int32_t)(now-due)>=0){due=now;value(now);} // Shortened LIVE deadline is not retroactive.
 }
};
}

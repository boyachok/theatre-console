#pragma once
#include <cmath>
#include <cstring>
#include "helmet_voice_wire.h"
//48kHz output EQ + a parallel, gated clean-mic consonant band. Limiter remains downstream.
namespace VoiceToneDSP {
constexpr float kPi=3.14159265358979323846f;
struct Coeff {float b0=1,b1=0,b2=0,a1=0,a2=0;};
inline Coeff filter(int type,float hz,float q=0.70710678f,float gain=0,float rate=48000){
 float w=2*kPi*hz/rate,c=cosf(w),s=sinf(w),alpha=s/(2*q),A=powf(10,gain/40),a0;Coeff r;
 if(type==0){a0=1+alpha/A;r={1+alpha*A,-2*c,1-alpha*A,-2*c,1-alpha/A};}
 else if(type==1){a0=1+alpha;r={(1+c)/2,-1-c,(1+c)/2,-2*c,1-alpha};}
 else if(type==2){a0=1+alpha;r={(1-c)/2,1-c,(1-c)/2,-2*c,1-alpha};}
 else {a0=1+alpha;r={alpha,0,-alpha,-2*c,1-alpha};}
 r.b0/=a0;r.b1/=a0;r.b2/=a0;r.a1/=a0;r.a2/=a0;return r;
}
class Biquad {
 Coeff current{},target{};float z1=0,z2=0;bool initialized=false;
public:
 void set(Coeff c){target=c;if(!initialized){current=c;initialized=true;}}
 void reset(){z1=z2=0;}
 float tick(float x){
  current.b0+=.002f*(target.b0-current.b0);current.b1+=.002f*(target.b1-current.b1);current.b2+=.002f*(target.b2-current.b2);current.a1+=.002f*(target.a1-current.a1);current.a2+=.002f*(target.a2-current.a2);
  float y=current.b0*x+z1;z1=current.b1*x-current.a1*y+z2;z2=current.b2*x-current.a2*y;
  if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)){reset();return 0;}return y;
 }
};
inline float softClip(float x){if(x>=3)return 1;if(x<=-3)return -1;float xx=x*x;return x*(27+xx)/(27+9*xx);}
class Processor {
 Biquad bands[3],highpass,lowpass,extract,returnBand,antiAlias;
 Show::VoiceTone previous{};bool configured=false;float eqMix=0,presenceMix=0,drive=0,lastBand=0;
public:
 void configure(const Show::VoiceTone &p){
  if(configured&&!memcmp(&previous,&p,sizeof(p)))return;
  if(!Show::validTone(p))return;previous=p;configured=true;
  for(int i=0;i<3;i++)bands[i].set(filter(0,p.bands[i].frequency,p.bands[i].q,p.bands[i].gainDb));
  highpass.set(p.highpassHz?filter(1,p.highpassHz):Coeff{});lowpass.set(p.lowpassHz?filter(2,p.lowpassHz):Coeff{});
  float q=fmaxf(.15f,fminf(8,p.presenceFrequency/p.presenceWidth));
  extract.set(filter(3,p.presenceFrequency,q));returnBand.set(filter(3,p.presenceFrequency,q));antiAlias.set(filter(2,14000,.70710678f,0,96000));
 }
 float tick(float processed,float gatedMic){
  float wanted=previous.presenceEnabled?previous.presenceAmount:0;
  presenceMix+=.002f*(wanted-presenceMix);drive+=.002f*(previous.presenceDrive-drive);
  if(presenceMix>.000001f||wanted>0){
   float band=extract.tick(gatedMic),g=1+drive,normal=1+.3f*drive;
   //2x oversampling and low-pass before decimation, then return only selected band.
   antiAlias.tick(softClip((lastBand+band)*.5f*g)/normal);
   float shaped=antiAlias.tick(softClip(band*g)/normal);lastBand=band;
   processed+=presenceMix*returnBand.tick(shaped);
  }else{extract.reset();returnBand.reset();antiAlias.reset();lastBand=0;}
  eqMix+=.002f*((previous.eqEnabled?1.f:0.f)-eqMix);
  if(eqMix>.000001f||previous.eqEnabled){float y=highpass.tick(processed);y=lowpass.tick(y);for(auto &b:bands)y=b.tick(y);processed+=eqMix*(y-processed);}
  else{highpass.reset();lowpass.reset();for(auto &b:bands)b.reset();}
  return std::isfinite(processed)?processed:0;
 }
};
}

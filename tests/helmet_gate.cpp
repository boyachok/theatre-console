#define THEATRE_HOST_TEST
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include "../helmet/voice_engine.h"

static double block(Helmet::VoiceProcessor &dsp, const Settings &settings, float amplitude, unsigned n, Helmet::VoiceStats &stats) {
 uint32_t input[512]{};int32_t output[512]{};
 for(unsigned i=0;i<256;i++){
  float sample=amplitude*std::sin((n*256+i)*2*3.141592653589793*173/48000);
  input[2*i]=(uint32_t)(int32_t)std::lround(sample*8388607.f)<<8;
 }
 dsp.process(settings,input,output,256,stats);double power=0;
 for(unsigned i=0;i<256;i++){assert(output[2*i]==output[2*i+1]);double value=output[2*i]/2147483647.;power+=value*value;}
 return std::sqrt(power/256);
}
int main(){
 for(int mode=0;mode<7;mode++){
  auto dsp=std::make_unique<Helmet::VoiceProcessor>();Settings s;s.enabled=true;s.gain=1;s.volume=.4f;s.notch=false;s.reduceThumps=false;s.toneHz=4500;s.gateDb=-25;s.frequency=154;s.mix=mode==1?0:.5f;
  s.mode=mode==0?DRY:ROBOT;s.vocoder=mode==3;s.autotune=mode==4;s.monotone=mode==5;s.speechChip=mode==6;
  Helmet::VoiceStats stats;double rms=0;
  for(unsigned n=0;n<220;n++)rms=block(*dsp,s,.003f,n,stats);
  if(rms>=1e-6){std::fprintf(stderr,"Closed gate leaks: case=%d rms=%g (dry/mix0 must be gated too)\n",mode,rms);return 1;}
  assert(stats.inputLevel>0&&stats.gateGain<.0001f&&stats.gateDb==-25); // Light/mic meter remains active.
  for(unsigned n=220;n<440;n++)rms=block(*dsp,s,.2f,n,stats);
  assert(rms>1e-4&&stats.gateGain>.99f);
  for(unsigned n=440;n<720;n++)rms=block(*dsp,s,.003f,n,stats);
  assert(rms<1e-6); // Delayed dry/wet tails settle; gate really closes again.
  s.gateDb=-90;
  for(unsigned n=720;n<940;n++)rms=block(*dsp,s,.003f,n,stats);
  assert(rms>1e-6); // Explicit gate bypass remains available.
 }
 puts("Helmet noise gate: dry, mix0 and all five real DSP paths suppress below-threshold input, open on speech and close after tails; bypass and ungated microphone metering preserved");
}

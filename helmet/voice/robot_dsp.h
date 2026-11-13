#pragma once
#include <math.h>
#include <stdint.h>

// Original implementation: 12-band analysis/synthesis vocoder plus ring mod.
// No pitch detection: the vocoder carrier deliberately holds a fixed pitch.
class RobotDSP {
  static constexpr float FS = 48000.0f;
  static constexpr float VOC_FS = 12000.0f;
  static constexpr float PI_ = 3.14159265358979323846f;
  static constexpr int BANDS = 12;
  struct Filter {
    float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    __attribute__((always_inline)) inline float tick(float x) {
      const float y=b0*x+z1;
      z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
      return y;
    }
    void band(float hz) {
      const float w=2*PI_*hz/VOC_FS, alpha=sinf(w)/(2*1.6f), inv=1/(1+alpha);
      b0=alpha*inv; b1=0; b2=-b0; a1=-2*cosf(w)*inv; a2=(1-alpha)*inv;
    }
    void low(float hz) {
      if(hz == 0) {
        b0=1; b1=b2=a1=a2=z1=z2=0;
        return;
      }
      const float w=2*PI_*hz/FS, c=cosf(w), alpha=sinf(w)*0.70710678f, inv=1/(1+alpha);
      b0=(1-c)*0.5f*inv; b1=2*b0; b2=b0; a1=-2*c*inv; a2=(1-alpha)*inv;
    }
    void clear() { z1=z2=0; }
  } analysis[BANDS][2], synthesis[BANDS][2], color, decimation[2];
  float env[BANDS]={}, carrierEnv[BANDS]={}, carrierGain[BANDS]={};
  float phase=0, step=85/FS, shape=0, shift=0, consonants=0.5f;
  float colorHz=0, previousSpeech=0, consonantHp=0, fade=1;
  uint32_t noiseState=0x43574f52;
  bool vocoder=false;
  uint32_t innerCount=0, subSample=0;
  float previousWet=0,nextWet=0;
  static float blep(float phase,float increment) {
    if (phase<increment) { const float t=phase/increment; return 2*t-t*t-1; }
    if (phase>1-increment) { const float t=(phase-1)/increment; return t*t+2*t+1; }
    return 0;
  }
public:
  RobotDSP() {
    for(int i=0;i<BANDS;++i) {
      const float center=150*powf(4800.0f/150,i/11.0f);
      for(int j=0;j<2;++j) { analysis[i][j].band(center); synthesis[i][j].band(center); }
    }
    color.low(4500); colorHz=4500;
    decimation[0].low(4500);decimation[1].low(4500);
  }
  void configure(bool useVocoder,float frequency,float wave,float toneHz,float formant,float clarity) {
    if(useVocoder!=vocoder) {
      vocoder=useVocoder; fade=0; color.clear();phase=0;subSample=0;previousWet=nextWet=0;
      previousSpeech=consonantHp=0;decimation[0].clear();decimation[1].clear();
      for(int i=0;i<BANDS;++i) {
        env[i]=carrierEnv[i]=0;carrierGain[i]=1;
        for(int j=0;j<2;++j) { analysis[i][j].clear(); synthesis[i][j].clear(); }
      }
    }
    step=frequency/(vocoder?VOC_FS:FS); shape=wave*12; shift=formant/(12*log2f(4800.0f/150)/11);
    consonants=clarity;
    if(fabsf(toneHz-colorHz)>0.5f) { color.low(toneHz); colorHz=toneHz; }
  }
  float tick(float speech) {
    float wet;
    if(!vocoder) {
      float carrier=sinf(2*PI_*phase);
      carrier=carrier*(1+shape)/(1+shape*fabsf(carrier));
      wet=speech*carrier;
    } else {
      const float bandLimited=decimation[1].tick(decimation[0].tick(speech));
      if(subSample==0) {
      previousWet=nextWet;
      const float saw=2*phase-1-blep(phase,step);
      noiseState^=noiseState<<13; noiseState^=noiseState>>17; noiseState^=noiseState<<5;
      const float noise=static_cast<float>(noiseState>>8)/8388608.0f-1;
      float bands[BANDS];
      for(int i=0;i<BANDS;++i) {
        const float voice=analysis[i][1].tick(analysis[i][0].tick(bandLimited));
        const float magnitude=fabsf(voice);
        env[i]+=(magnitude>env[i]?0.0276f:0.00208f)*(magnitude-env[i]);
        const float noiseMix=i>=8?0.35f*consonants:0.015f;
        const float carrier=synthesis[i][1].tick(synthesis[i][0].tick(saw+noiseMix*noise));
        carrierEnv[i]+=0.004f*(fabsf(carrier)-carrierEnv[i]);
        if((innerCount&15)==0)carrierGain[i]=1/fmaxf(carrierEnv[i],0.025f);
        bands[i]=carrier*carrierGain[i];
      }
      wet=0;
      for(int i=0;i<BANDS;++i) {
        const float source=i-shift;
        if(source<0 || source>BANDS-1) continue;
        const int index=static_cast<int>(source);
        const int next=index<BANDS-1?index+1:index;
        const float amount=source-index;
        wet+=bands[i]*(env[index]+amount*(env[next]-env[index]));
      }
      // Small high-passed direct component preserves s/f/sh articulation.
      consonantHp=0.214f*(consonantHp+bandLimited-previousSpeech);
      previousSpeech=bandLimited;
      wet=0.7f*wet+0.65f*consonants*consonantHp;
      nextWet=wet;phase+=step;if(phase>=1)phase-=1;++innerCount;
      }
      wet=previousWet+(nextWet-previousWet)*(subSample*0.25f);
      subSample=(subSample+1)&3;
    }
    if(!vocoder){phase+=step;if(phase>=1)phase-=1;}
    fade+=0.001f*(1-fade);
    return color.tick(wet)*fade;
  }
};

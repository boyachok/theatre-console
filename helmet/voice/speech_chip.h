#pragma once
#include "autotune_core.h"
#include "speech_chip_range.h"

// LPC-10 speech resynthesis at 8 kHz. Fixed-pitch excitation keeps the same
// phase across overlapping frames; unvoiced speech excites the filter with noise.
class SpeechChip {
  static constexpr int Size = 320;
  static constexpr int Hop = 80;
  static constexpr int Order = 10;
  static constexpr unsigned Mask = 511;
  static constexpr int Decimation = 6;
  static constexpr float Rate = 8000;
  float input[Mask+1] = {}, pulse[Mask+1] = {}, noise[Mask+1] = {}, output[Mask+1] = {};
  float window[Size];
  RVATFilter antiAlias[2] = {};
  uint32_t time = 0, random = 0x4711A31Du;
  int subSample = 0, hop = 0;
  float phase = 0, phaseStep = 110/Rate, pulseGain = 0;
  float voicing = 0, previous = 0, next = 0;
  int unvoicedFrames = 0, breathReduction = 0;
  float breathFloor = 1, breathGain = 1;

  float pulseSample() {
    // TI patent US4209836A, Table VIII. The final zero is for interpolation.
    static const int8_t chirp[] = {
      0,42,-44,50,-78,18,37,20,2,-31,-59,2,95,90,5,15,38,-4,-91,-91,
      -42,-35,-36,-4,37,43,34,33,15,-1,-8,-18,-19,-17,-9,-10,-6,0,3,2,1,0
    };
    // A pitch cycle addresses the chirp at its original 8 kHz sample spacing.
    const float position = phase*pulseGain*pulseGain;
    const int index = static_cast<int>(position);
    float excitation = 0;
    if(index < static_cast<int>(sizeof(chirp))-1) {
      excitation = chirp[index]+(position-index)*(chirp[index+1]-chirp[index]);
    }
    return pulseGain*excitation/255.8964634f;
  }

  float breathAttenuation(float periodicityScore) {
    // Leave short consonants and frames that contain voicing at full level.
    // Apply the reduction only after a run of about 80 ms of unvoiced audio.
    if(periodicityScore >= .55f) {
      unvoicedFrames = 0;
      breathGain = 1;
      return 1;
    }
    if(unvoicedFrames < 8) ++unvoicedFrames;
    if(unvoicedFrames < 8 || breathReduction == 0) {
      breathGain = 1;
      return 1;
    }
    breathGain += .30f*(breathFloor-breathGain);
    return breathGain;
  }

  float noiseSample() {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    return ((random >> 8) / 8388608.f - 1) * 1.7320508f;
  }

  float periodicity(uint32_t start) const {
    constexpr int MaxLag = 120;
    constexpr int Length = Size - MaxLag;
    float reference[Length], mean = 0;
    for(int i=0;i<Length;++i) mean += input[(start+i)&Mask];
    mean /= Length;
    float referencePower = 0, shiftedPower = 0, shiftedSum = 0;
    for(int i=0;i<Length;++i) {
      const float value = input[(start+i)&Mask];
      reference[i] = value - mean;
      referencePower += reference[i]*reference[i];
      shiftedPower += value*value;
      shiftedSum += value;
    }
    if(referencePower < 1e-10f) return 0;
    float bestSquared = 0;
    for(int lag=1;lag<=MaxLag;++lag) {
      const float leaving = input[(start+lag-1)&Mask];
      const float entering = input[(start+lag+Length-1)&Mask];
      shiftedSum += entering-leaving;
      shiftedPower += entering*entering-leaving*leaving;
      if(lag < 12) continue;
      float cross = 0;
      for(int i=0;i<Length;++i) cross += reference[i] * input[(start+lag+i)&Mask];
      const float power = referencePower * (shiftedPower-shiftedSum*shiftedSum/Length);
      if(cross > 0 && power > 1e-20f && cross*cross > bestSquared*power) {
        bestSquared = cross*cross / power;
      }
    }
    return sqrtf(fminf(1,bestSquared));
  }

  void analyze(uint32_t start, float *coefficients) const {
    float frame[Size], correlation[Order+1];
    for(int i=0;i<Size;++i) {
      const uint32_t index = start+i;
      frame[i] = (input[index&Mask] - .92f*input[(index-1)&Mask])*window[i];
    }
    for(int lag=0;lag<=Order;++lag) {
      float sum = 0;
      for(int i=lag;i<Size;++i) sum += frame[i]*frame[i-lag];
      correlation[lag] = sum;
    }
    // Levinson-Durbin recursion. Regularization and reflection bounds limit
    // resonances even on nearly pure input tones.
    float error = fmaxf(correlation[0]*1.002f,1e-12f);
    coefficients[0] = 1;
    for(int order=1;order<=Order;++order) {
      float residual = correlation[order];
      for(int i=1;i<order;++i) residual += coefficients[i]*correlation[order-i];
      const float reflection = rvat_clamp(-residual/error,-.995f,.995f);
      for(int left=1,right=order-1;left<=right;++left,--right) {
        const float a = coefficients[left], b = coefficients[right];
        coefficients[left] = a+reflection*b;
        coefficients[right] = b+reflection*a;
      }
      coefficients[order] = reflection;
      error = fmaxf(error*(1-reflection*reflection),1e-12f);
    }
    float bandwidth = .985f;
    for(int i=1;i<=Order;++i) {
      coefficients[i] *= bandwidth;
      bandwidth *= .985f;
    }
  }

  void renderFrame() {
    constexpr int Warmup = 160;
    const uint32_t start = time-(Size-1);
    float sourcePower = 0;
    for(int i=0;i<Size;++i) {
      const float sample = input[(start+i)&Mask];
      sourcePower += sample*sample;
    }
    if(sourcePower == 0) {
      voicing = 0;
      unvoicedFrames = 0;
      breathGain = 1;
      return;
    }

    float coefficients[Order+1], history[Order] = {}, frame[Size];
    analyze(start,coefficients);
    const float periodicityScore = periodicity(start);
    const float voiced = rvat_clamp((periodicityScore-.55f)/.3f,0,1);
    voicing += .65f*(voiced-voicing);
    float deEmphasis = 0, power = 0;
    for(int i=-Warmup;i<Size;++i) {
      const unsigned index = (start+i)&Mask;
      float sample = voicing*pulse[index]+(1-voicing)*noise[index];
      for(int j=0;j<Order;++j) sample -= coefficients[j+1]*history[j];
      for(int j=Order-1;j>0;--j) history[j] = history[j-1];
      history[0] = sample;
      deEmphasis = sample+.92f*deEmphasis;
      if(i >= 0) {
        frame[i] = deEmphasis;
        power += deEmphasis*deEmphasis;
      }
    }
    if(!isfinite(power) || power < 1e-12f) return;
    // A periodic Hann window with four overlaps has constant weight two.
    const float gain = .5f*sqrtf(sourcePower/power)*breathAttenuation(periodicityScore);
    for(int i=0;i<Size;++i) output[(start+Size+i)&Mask] += gain*frame[i]*window[i];
  }

public:
  static constexpr int Delay = (Size+1)*Decimation;
  SpeechChip() {
    for(int i=0;i<Size;++i) window[i] = .5f-.5f*cosf(2*RV_AT_PI*i/Size);
    const float w = 2*RV_AT_PI*3300/48000;
    const float c = cosf(w);
    const float alpha = sinf(w)*.70710678f;
    const float inv = 1/(1+alpha);
    for(auto &filter:antiAlias) {
      filter.b0 = (1-c)*.5f*inv;
      filter.b1 = 2*filter.b0;
      filter.b2 = filter.b0;
      filter.a1 = -2*c*inv;
      filter.a2 = (1-alpha)*inv;
    }
    configure(110);
  }

  void configure(float hz, int reduction = 0) {
    if(reduction != breathReduction) {
      breathReduction = reduction;
      breathFloor = (100-reduction)*.01f;
    }
    const float increment = rvat_clamp(hz,RV_CHIP_MIN_HZ,RV_CHIP_MAX_HZ)/Rate;
    if(increment == phaseStep && pulseGain > 0) return;
    phaseStep = increment;
    pulseGain = sqrtf(1/phaseStep);
  }

  void reset() {
    memset(input,0,sizeof(input));
    memset(pulse,0,sizeof(pulse));
    memset(noise,0,sizeof(noise));
    memset(output,0,sizeof(output));
    for(auto &filter:antiAlias) filter.z1 = filter.z2 = 0;
    time = subSample = hop = 0;
    phase = voicing = previous = next = 0;
    unvoicedFrames = 0;
    breathGain = 1;
  }

  float tick(float sample) {
    const float filtered = rvat_filter(&antiAlias[1],rvat_filter(&antiAlias[0],sample));
    if(subSample == 0) {
      const unsigned index = time&Mask;
      input[index] = filtered;
      pulse[index] = pulseSample();
      noise[index] = noiseSample();
      phase += phaseStep;
      if(phase >= 1) phase -= 1;
      if(++hop == Hop) {
        renderFrame();
        hop = 0;
      }
      previous = next;
      next = output[index];
      output[index] = 0;
      ++time;
    }
    const float interpolated = previous+(next-previous)*(subSample*(1.f/Decimation));
    if(++subSample == Decimation) subSample = 0;
    return interpolated;
  }
};

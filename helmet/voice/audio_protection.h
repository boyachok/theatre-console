#pragma once
#include "autotune_core.h"

class InputLowCut {
  const float normalAlpha = expf(-2 * RV_AT_PI * 70 / 48000);
  RVATFilter stronger = {};
  float previous = 0, normal = 0, blend = 0;

public:
  InputLowCut() {
    const float omega = 2 * RV_AT_PI * 130 / 48000;
    const float c = cosf(omega);
    const float alpha = sinf(omega) * .70710678f;
    const float inverse = 1 / (1 + alpha);
    stronger.b0 = (1 + c) * .5f * inverse;
    stronger.b1 = -2 * stronger.b0;
    stronger.b2 = stronger.b0;
    stronger.a1 = -2 * c * inverse;
    stronger.a2 = (1 - alpha) * inverse;
  }

  float tick(float input, bool reduceThumps) {
    normal = normalAlpha * (normal + input - previous);
    previous = input;
    const float filtered = rvat_filter(&stronger, input);
    blend += .004f * ((reduceThumps ? 1.f : 0.f) - blend);
    return normal + blend * (filtered - normal);
  }

  float baseline() const { return normal; }
};

// Two 1 ms buffers let the gain ramp down before a peak is played.
// Both ends of a block's ramp obey that block's measured peak limit.
class OutputPeakLimiter {
  static constexpr int Window = 48;
  static constexpr float Ceiling = .89f;
  float buffered[2][Window] = {};
  float peaks[2] = {};
  int block = 0, index = 0;
  float inputPeak = 0, gain = 1, ramp = 1, step = 0;

  static float limitFor(float peak) {
    return peak > Ceiling ? Ceiling / peak : 1.f;
  }

public:
  static constexpr int Delay = 2 * Window;

  float tick(float input) {
    if(index == 0) {
      const float limit = limitFor(peaks[block]);
      ramp = fminf(gain, limit);
      const float nextLimit = fminf(limit, limitFor(peaks[1-block]));
      step = (nextLimit - ramp) / Window;
    }
    ramp += step;
    // Release more slowly than the attack to avoid rapid volume modulation.
    gain = fminf(ramp, gain + .00025f * (1-gain));
    const float delayed = buffered[block][index];
    buffered[block][index] = input;
    inputPeak = fmaxf(inputPeak, fabsf(input));
    if(++index == Window) {
      peaks[block] = inputPeak;
      inputPeak = 0;
      block = 1-block;
      index = 0;
    }
    return gain * delayed;
  }

  bool limiting() const { return gain < .999f; }
};

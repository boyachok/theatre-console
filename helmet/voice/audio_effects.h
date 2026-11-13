#pragma once
#include "voice_settings.h"
#include "robot_dsp.h"
#include "autotune_core.h"
#include "speech_chip.h"

// The same integration is exercised by the host audio tests. This object owns
// large delay buffers; keep it static, never on the FreeRTOS task stack.
class VoiceEffects {
  RobotDSP robot;
  SpeechChip chip;
  RVATFilter color = {};
  Settings processor;
  int active = -1, desired = -1;
  float transition = 0, mix = 0, drive = 0, driveReference = 0, colorHz = -1, testPhase = 0;
  bool initialized = false;
  static int engine(const Settings &s) {
    if(s.mode == DRY) return -1;
    if(s.mode == TEST) return -2;
    if(s.mix == 0) return -1;
    return algorithmId(s);
  }
  void configureColor(float hz) {
    if(hz == colorHz) return;
    if(hz == 0) {
      color = {};
      color.b0 = 1;
      colorHz = 0;
      return;
    }
    const float w = 2 * RV_AT_PI * hz / 48000, c = cosf(w);
    const float alpha = sinf(w) * .70710678f, inv = 1 / (1 + alpha);
    color.b0 = (1-c)*.5f*inv; color.b1 = 2*color.b0; color.b2 = color.b0;
    color.a1 = -2*c*inv; color.a2 = (1-alpha)*inv; colorHz = hz;
  }
public:
  RVAutotune tune;
  void configure(const Settings &control) {
    desired = engine(control);
    if(!initialized) {
      rvat_init(&tune); processor = control; active = desired; initialized = true;
    }
    if(active != desired && transition < .001f) {
      active = desired; processor = control; color.z1 = color.z2 = 0;
      driveReference = 0;
      if(active == 4) chip.reset();
    }
    if(active == desired) processor = control;
    robot.configure(active == 1, processor.frequency, processor.wave,
                    processor.toneHz, processor.formant, processor.clarity);
    rvat_configure(&tune, processor.tuneKey, processor.tuneScale,
                   processor.retuneMs, processor.tuneStrength, processor.transpose);
    rvat_robot_configure(&tune, active == 3, processor.frequency);
    if(active == 4) chip.configure(processor.frequency,processor.breathReduction);
    configureColor(processor.toneHz);
  }
  float tick(float clean, float gate) {
    const bool pitchEngine = active == 2 || active == 3;
    // Gate source audio before its delay, so the ends of words and consonants
    // travel through the gate and the pitch shifter on the same time line.
    const float source = active >= 0 ? clean * gate : clean;
    const float tuned = rvat_tick(&tune, source, pitchEngine);
    float signal = clean;
    mix += .004f * ((active >= 0 ? processor.mix : 0) - mix);
    drive += .004f * ((active >= 0 ? processor.drive : 0) - drive);
    if(active >= 0) {
      float wet, dry;
      if(active == 4) {
        wet = rvat_filter(&color,chip.tick(source));
        dry = tune.filled > SpeechChip::Delay
            ? tune.input[(tune.time-1-SpeechChip::Delay)&RV_AT_MASK] : 0;
      } else if(pitchEngine) {
        wet = rvat_filter(&color,tuned);
        dry = tune.dry;
      } else {
        wet = robot.tick(source);
        dry = source;
      }
      const float magnitude = fabsf(wet);
      // Anchor the shaper to the recent signal level instead of full scale.
      const float follow = magnitude > driveReference ? .01f : .0005f;
      driveReference += follow*(magnitude-driveReference);
      const float reference = fmaxf(driveReference,1e-9f);
      const float driven = wet*(1+drive)*reference/(reference+drive*magnitude);
      signal = dry * (1-mix) + driven * mix;
    }
    if(active == -2) signal = .125f * sinf(testPhase);
    testPhase += 2 * RV_AT_PI * 1000 / 48000;
    if(testPhase >= 2 * RV_AT_PI) testPhase -= 2 * RV_AT_PI;
    // Mode changes (including dry <-> delayed effects) switch at near silence.
    transition += .01f * ((active == desired ? 1.0f : 0.0f) - transition);
    return isfinite(signal) ? signal * transition : 0;
  }
};

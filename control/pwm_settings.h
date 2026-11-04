#pragma once
#include <stdint.h>
#include <cstring>
#include "control_types.h"
struct __attribute__((packed)) PwmSettings {
  uint32_t magic=0x31574d50;uint8_t version=1,inverted=0,level[PWM_COUNT];uint16_t reserved=0;
  PwmSettings(){for(auto &v:level)v=255;}
  bool valid()const{return magic==0x31574d50&&version==1&&reserved==0;}
};
struct __attribute__((packed)) PwmStatus {
  uint8_t level[PWM_COUNT],inverted,onMask;uint16_t reserved=0;uint32_t revision=0;
};
static_assert(sizeof(PwmSettings)==16&&sizeof(PwmStatus)==16,"PWM config sizes");
inline uint8_t physicalPWM(uint8_t logical,bool inverted){return inverted?255-logical:logical;}

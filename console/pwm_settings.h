#pragma once
#include <stdint.h>
#include <cstring>
#include "control_types.h"
struct __attribute__((packed)) PwmSettings {
  uint32_t magic=0x31574d50;uint8_t version=1,inverted=0,level[PWM_COUNT];uint16_t reserved=0;
  PwmSettings(){for(auto &v:level)v=255;}
  bool valid()const{return magic==0x31574d50&&version==1&&reserved==0;}
};
// New canonical record. Leave legacy pwmConfig bytes intact for non-destructive migration/rollback.
struct __attribute__((packed)) DimSettings {
 uint32_t magic=0x324d4944;uint8_t version=1,reserved=0;uint16_t inverted=0;uint8_t level[PWM_COUNT];
 DimSettings(){for(auto &v:level)v=255;}
 bool valid()const{return magic==0x324d4944&&version==1&&!reserved&&!(inverted&~4095);}
};
static_assert(sizeof(DimSettings)==16,"12-channel polarity settings");
struct __attribute__((packed)) PwmStatus {
  uint8_t level[PWM_COUNT],inverted,onMask;uint16_t reserved=0;uint32_t revision=0;
};
static_assert(sizeof(PwmSettings)==16&&sizeof(PwmStatus)==16,"PWM config sizes");
inline uint16_t reportedDimmerInversions(const PwmStatus &p){return p.inverted|((p.reserved&0xfff0)==0xd000?((p.reserved&15)<<8):0);}
inline uint8_t physicalPWM(uint8_t logical,bool inverted){return inverted?255-logical:logical;}

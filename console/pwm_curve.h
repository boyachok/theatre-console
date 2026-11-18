#pragma once
#include <cmath>
#include <stdint.h>
// Opt-in bench builds, not a silent fleet migration. 10bits still fit XTAL40MHz/25kHz.
#ifndef THEATRE_PWM_BITS
#define THEATRE_PWM_BITS 8
#endif
#ifndef THEATRE_PWM_HZ
#define THEATRE_PWM_HZ 25000
#endif
#ifndef THEATRE_PWM_GAMMA
#define THEATRE_PWM_GAMMA 1.0f
#endif
#ifndef THEATRE_PWM_GRAY
#define THEATRE_PWM_GRAY 0.5f
#endif
static_assert(THEATRE_PWM_BITS==8||THEATRE_PWM_BITS==10||THEATRE_PWM_BITS==12,"Supported PWM precision:8,10 or12");
static_assert(THEATRE_PWM_HZ>=2000&&THEATRE_PWM_HZ<=25000&&80000000UL%THEATRE_PWM_HZ==0,"Invalid bench PWM frequency");
namespace PwmCurve {
inline float shape(float nominal,float gamma=THEATRE_PWM_GAMMA,float gray=THEATRE_PWM_GRAY){
 if(!std::isfinite(nominal)||nominal<=0)return 0;if(nominal>=255)return 1;
 float x=nominal/255.f;float y=gamma==1.f?x:std::pow(x,gamma);
 // Optional middle-gray anchor; endpoints remain exactly off/on, no forced glow floor.
 return y<=.5f?y*2*gray:gray+(y-.5f)*2*(1-gray);
}
inline uint16_t duty(float nominal,bool inverted=false,uint8_t bits=THEATRE_PWM_BITS,float gamma=THEATRE_PWM_GAMMA,float gray=THEATRE_PWM_GRAY){
 const uint16_t max=(1U<<bits)-1;float x=shape(nominal,gamma,gray);uint16_t n=(uint16_t)std::lround(x*max);return inverted?max-n:n;
}
}

#define THEATRE_HOST_TEST 1
#define THEATRE_PWM_HZ 2000
#include <cassert>
#include <cstdio>
#include "../console/score_runtime.h"
int main(){
 static_assert(PWM_BITS==8&&PWM_HZ==2000,"Camera bench changes frequency only");
 static_assert(80000000UL/PWM_HZ==40000&&80000000UL/PWM_HZ<=65535,"MCPWM period fits 16-bit timer");
 Actors actor;actor.begin();Show::Runner runner(actor);runner.begin();
 for(unsigned i=0;i<12;i++)assert(mockPWMFrequency[PWM_PINS[i]]==2000);
 for(unsigned i=0;i<8;i++)assert(mockPWMResolution[PWM_PINS[i]]==8);
 for(unsigned level:{0U,1U,13U,26U,64U,128U,254U,255U}){
  Show::Packet p{};p.run=17;p.transition=level+1;p.cue.dimmerMask=0xfff;
  for(unsigned i=0;i<12;i++)p.cue.dimmers[i].value=level;
  assert(runner.apply(p,1000+level));runner.tick(1001+level);
  for(unsigned i=0;i<12;i++){assert(mockPWM[PWM_PINS[i]]==level);assert(actor.dimmerFineValue(i)==level);}
  assert(!actor.snapshot().relayOn);
 }
 for(unsigned i=0;i<256;i++)assert(PwmCurve::duty(i)==i&&PwmCurve::duty(i,true)==255-i);
 runner.stop();for(unsigned i=0;i<12;i++)assert(mockPWM[PWM_PINS[i]]==0);
 puts("Camera bench2kHz/8bit:12 PWM channels, exact off/full/low levels, linear gamma/inversion and STOP; HAL mocked, no camera/thermal guarantee");
}

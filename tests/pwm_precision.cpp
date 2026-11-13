#define THEATRE_HOST_TEST 1
#define THEATRE_PWM_BITS 10
#include <cassert>
#include <cmath>
#include <set>
#include <cstdio>
#include "../console/score_runtime.h"
int main(){
 static_assert(PWM_BITS==10&&PWM_HZ==25000,"10-bit bench keeps25kHz");
 for(int i=0;i<=255;i++)assert(PwmCurve::duty((float)i,false,8)==i);
 assert(PwmCurve::duty(0)==0&&PwmCurve::duty(255)==1023&&PwmCurve::duty(0,true)==1023&&PwmCurve::duty(255,true)==0);
 assert(PwmCurve::duty(.25f)>0);uint16_t previous=0;
 for(int i=0;i<=25500;i++){auto v=PwmCurve::duty(i/100.f,false,10,1.8f,.35f);assert(v>=previous);previous=v;}
 assert(std::abs(PwmCurve::shape(127.5f,1,.18f)-.18f)<.00001f);
 Actors actor;actor.begin();assert(mockPWMResolution[PWM_PINS[0]]==10&&mockPWMFrequency[PWM_PINS[0]]==25000);
 Show::Runner runner(actor);runner.begin();Show::Packet p{};p.run=1;p.transition=1;p.cue.dimmerMask=1;p.cue.dimmers[0]={255,1000,1000};assert(runner.apply(p,1000));std::set<uint32_t> levels;
 for(unsigned t=1000;t<=2000;t++){runner.tick(t);levels.insert(mockPWM[PWM_PINS[0]]);}assert(levels.size()>800&&mockPWM[PWM_PINS[0]]==1023);
 p.transition++;p.cue.dimmers[0].value=0;assert(runner.apply(p,2000));for(unsigned t=2000;t<=3000;t++)runner.tick(t);assert(mockPWM[PWM_PINS[0]]==0&&actor.dimmerFineValue(0)==0);
 runner.stop();assert(mockPWM[PWM_PINS[0]]==0);puts("10-bit25kHz bench: >800 real programmed fade levels, fractional end-to-end, gamma/gray monotonic/exact off/full; no physical waveform measurement");
}

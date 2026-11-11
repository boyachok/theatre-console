#include <cassert>
#include <cstdio>
#include "../console/ac_phase.h"
using namespace Mains;
Pulse qualify(Phase &p,uint8_t level=128,uint64_t start=1000,uint32_t half=10000){p.arm(start);p.level(level);Pulse pulse{};for(int i=0;i<=6;i++){uint64_t t=start+(uint64_t)i*half;p.controller(t,true);bool sent=p.zeroCross(t,pulse);assert(sent==(i==6&&level!=0));}return pulse;}
int main(){
 Phase p;Pulse out{};assert(!p.zeroCross(1000,out)&&p.state().fault==OFF_STATE);auto pulse=qualify(p);assert(p.state().qualified&&!p.state().latched&&pulse.fire-pulse.zero>4900&&pulse.fire-pulse.zero<5100);assert(p.authorize(pulse,pulse.zero+40));
 p.level(200);p.controller(71000,true);assert(p.zeroCross(71000,out)&&p.state().paired==128);p.controller(81000,true);assert(p.zeroCross(81000,out)&&p.state().paired==200);
 p.level(0);assert(!p.authorize(out,81001));p.controller(91000,true);assert(!p.zeroCross(91000,out));p.disable();assert(!p.state().enabled&&!p.zeroCross(101000,out));
 Phase full;auto max=qualify(full,255);assert(max.fire-max.zero==300&&max.end==max.zero+9500);Phase low;auto min=qualify(low,1);assert(min.fire-min.zero<=9000&&min.fire-min.zero>=8500);Phase sixty;auto hz60=qualify(sixty,128,1000,8333);assert(hz60.fire-hz60.zero>4000&&hz60.fire-hz60.zero<4300);
 Phase missing;qualify(missing);missing.controller(96002,true);missing.watchdog(96002);assert(missing.state().fault==ZERO_LOST&&missing.state().latched);assert(!missing.zeroCross(101000,out));missing.controller(101000,true);assert(missing.state().latched);missing.arm(102000);assert(!missing.state().latched&&!missing.state().qualified);
 Phase stalled;qualify(stalled);stalled.watchdog(112001);assert(stalled.state().fault==CONTROL_LOST);
 Phase link;qualify(link);link.controller(62000,false);assert(!link.state().latched&&link.authorize(pulse,62001));link.controller(63000,true);assert(!link.state().latched);link.arm(64000);assert(!link.state().latched);
 Phase late;auto scheduled=qualify(late);assert(!late.authorize(scheduled,scheduled.fire+151)&&late.state().fault==TIMING&&late.state().late==1);
 Phase noisy;qualify(noisy);assert(!noisy.zeroCross(61100,out)&&!noisy.zeroCross(61200,out)&&!noisy.zeroCross(61300,out));assert(noisy.state().fault==TIMING&&noisy.state().rejected==3);
 Phase invalid;qualify(invalid);assert(!invalid.zeroCross(81000,out)&&invalid.state().latched);Phase unstable;qualify(unstable);assert(!unstable.zeroCross(70000,out)&&unstable.state().fault==TIMING);
 Phase resource;resource.resourceFailure();assert(resource.state().fault==RESOURCE&&resource.state().latched);
 // The nonlinear table is monotone, with bounded pulses; the gate always ends before the next half-wave.
 uint64_t previous=12000;for(int v=1;v<=255;v++){Phase a;auto q=qualify(a,v);uint64_t delay=q.fire-q.zero;assert(delay<=previous&&delay>=300&&q.fire+200<q.end);previous=delay;}
 puts("AC phase logic: default OFF, six-interval qualification, 50/60Hz tracking, monotone RMS-power map, paired half-waves, 200us/edge margins, zero/noise/jitter/late/CPU/resource fault latches, Wi-Fi-loss hold and explicit rearm passed (pure logic only; no mains or waveform measurement)");
}

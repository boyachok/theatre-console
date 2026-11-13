#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
#include "../console/ac_phase.h"
using namespace Mains;
Pulse qualify(Phase &p,uint64_t start=1000,uint8_t value=128,uint32_t half=10000){p.arm(start);p.level(value);Pulse out{};for(int i=0;i<=6;i++){auto t=start+(uint64_t)i*half;p.controller(t,true);bool emitted=p.zeroCross(t,out);assert(emitted==(i==6&&value!=0));}assert(p.state().qualified&&!p.state().latched);return out;}
int main(){
 Pulse out{};Phase defaults;assert(!defaults.state().enabled&&!defaults.zeroCross(1000,out));
 Phase normal;auto first=qualify(normal);assert(first.fire-first.zero>=4900&&first.fire-first.zero<=5100&&normal.authorize(first,first.zero+30));
 normal.level(200);normal.controller(71000,true);assert(normal.zeroCross(71000,out)&&normal.state().paired==128);normal.controller(81000,true);assert(normal.zeroCross(81000,out)&&normal.state().paired==200);normal.level(0);assert(!normal.authorize(out,81001));
 Phase sixty;auto hz60=qualify(sixty,1000,128,8333);assert(hz60.fire-hz60.zero>4000&&hz60.fire-hz60.zero<4300);
 // Captured real-board intervals from the user's fault report, not synthetic clean intervals.
 std::vector<uint32_t> raw={9994,10005,347,9653,344,9658,10001,345,9659,9997,10003,10001,9999,348,9652,10003,10000,10009,9995,10005,9995,346,9660,9996,10002,10004,10001,10000,10002,9999,352,9645,355,9650,10002,9999,9998,348,9654,346,9653,348,9653,10003,9997,350,9652,10001,10000,10002,346,9654,345,9653,10002,10003,342,9660,9996,351,9653,9998,348,9654};
 Phase replay;replay.arm(1000);replay.level(128);uint64_t t=1000;replay.controller(t,true);replay.zeroCross(t,out);uint64_t pulses=0;
 for(int round=0;round<10000;round++)for(auto delta:raw){t+=delta;replay.controller(t,true);replay.watchdog(t);if(replay.zeroCross(t,out)){pulses++;assert(out.fire+200<out.end&&replay.authorize(out,t+30));}assert(!replay.state().latched);}
 assert(replay.state().qualified&&replay.state().syncLosses==0&&replay.state().duplicateEdges>100000&&replay.state().halfUs>9950&&replay.state().halfUs<10050);printf("Real trace replay: %llu seconds, %llu gated half-waves, duplicates=%u, no lock losses (simulation)\n",(unsigned long long)(t/1000000),(unsigned long long)pulses,replay.state().duplicateEdges);
 // Previously three early edges caused permanent TIMING. Now they are harmless and do not move the anchor.
 Phase noise;qualify(noise);for(auto at:{61100ULL,61200ULL,61350ULL,62000ULL}){noise.controller(at,true);assert(!noise.zeroCross(at,out));}noise.controller(71000,true);assert(noise.zeroCross(71000,out)&&noise.state().qualified&&!noise.state().latched&&noise.state().duplicateEdges==4);
 // Reject an edge outside the prediction window, then accept the real edge.
 noise.controller(79500,true);assert(!noise.zeroCross(79500,out)&&noise.state().ignoredWindow==1);noise.controller(81000,true);assert(noise.zeroCross(81000,out));
 Phase lost;auto old=qualify(lost);lost.controller(97000,true);lost.watchdog(97000);assert(!lost.state().qualified&&!lost.state().latched&&lost.state().requested==128&&lost.state().syncLosses==1&&!lost.authorize(old,97001));
 for(int i=0;i<=6;i++){uint64_t at=101000+i*10000;lost.controller(at,true);bool pulse=lost.zeroCross(at,out);assert(pulse==(i==6));}assert(lost.state().qualified&&lost.state().recoveries==1&&!lost.state().latched&&!lost.authorize(old,161010));
 Phase gap;qualify(gap);gap.controller(81000,true);assert(!gap.zeroCross(81000,out)&&!gap.state().qualified&&!gap.state().latched&&gap.event().reason==ZC_GAP&&gap.event().intervalUs==20000);for(int i=1;i<=6;i++){gap.controller(81000+i*10000,true);gap.zeroCross(81000+i*10000,out);}assert(gap.state().qualified&&gap.state().recoveries==1);
 Phase late;auto p=qualify(late);assert(!late.authorize(p,p.fire+151)&&!late.state().latched&&late.state().qualified&&late.state().skippedPulses==1&&late.event().reason==PULSE_LATE);late.controller(71000,true);assert(late.zeroCross(71000,out)&&late.authorize(out,71030));
 Phase cpu;qualify(cpu);cpu.watchdog(112001);assert(cpu.state().latched&&cpu.state().fault==CONTROL_LOST);cpu.controller(121000,true);assert(!cpu.zeroCross(121000,out));cpu.arm(122000);assert(!cpu.state().latched&&!cpu.state().qualified&&cpu.state().requested==0);
 Phase drift;qualify(drift);uint64_t now=61000;for(int i=0;i<6000;i++){uint32_t interval=9950+(i/20)%100;now+=interval;drift.controller(now,false);assert(drift.zeroCross(now,out));assert(drift.state().qualified&&!drift.state().latched);}assert(drift.state().syncLosses==0);
 // Exact on-device race: watchdog sampled 269650608us, ISR accepted Z-C at 269650611us before lock acquisition.
 Phase clockRace;clockRace.arm(269590500);clockRace.level(128);for(int i=0;i<=6;i++){uint64_t z=269590611ULL+i*10000;clockRace.controller(z-100,true);clockRace.zeroCross(z,out);}assert(clockRace.state().qualified);clockRace.watchdog(269650608);assert(clockRace.state().qualified&&!clockRace.state().latched&&clockRace.state().syncLosses==0&&clockRace.state().staleClockSamples==1);clockRace.controller(269690611,true);clockRace.watchdog(269690611);assert(!clockRace.state().qualified&&clockRace.state().syncLosses==1); // Real 40ms loss still detected.
 // Monitoring during slow boot/STOP never arms a load and must not acquire a CPU latch at zero.
 Phase ready;ready.arm(1000);for(int i=0;i<=20;i++){ready.watchdog(1000+i*10000);ready.zeroCross(1000+i*10000,out);}assert(ready.state().qualified&&!ready.state().latched&&ready.state().requested==0);ready.controller(202000,true);ready.level(128);ready.zeroCross(211000,out);assert(ready.zeroCross(221000,out)&&ready.state().qualified);
 uint64_t previous=12000;for(int v=1;v<=255;v++){Phase a;auto q=qualify(a,1000,v);auto delay=q.fire-q.zero;assert(delay<=previous&&delay>=300&&q.fire+200<q.end);previous=delay;}
 puts("PASS: six-edge qualification, 50/60Hz, real duplicate trace, prediction window, automatic safe reacquisition, stale pulse/STOP guards, bounded nonlinear map, transient deadline skip, hard CPU latch, Wi-Fi-independent continuity; no physical waveform claim");
}

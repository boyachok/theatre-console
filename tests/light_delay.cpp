#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <cmath>
#include "../console/score_runtime.h"
using namespace Show;
int main(){
 Dimmer d{255,1000,1000};DimmerAnimation off{};DimmerChannel light;light.reset(200,0);light.apply(d,off,true,100,true,1000);assert(light.waiting()&&light.value(1099)==200);assert(light.value(1100)==200&&!light.waiting());assert(std::fabs(light.value(1600)-227.5)<.01);
 light.apply(d,off,true,1700,false,1000);assert(std::fabs(light.value(1800)-238.5)<.01);light.apply(d,off,true,2100,false,500);assert(light.value(2200)==255);
 Dimmer lower{100,1000,1000};light.apply(lower,off,true,2300,true,500);assert(light.value(2301)==255&&light.value(2799)==255&&light.value(2800)==255);assert(std::fabs(light.value(3300)-177.5)<.01);
 Dimmer immediate{255,0,0};DimmerChannel pending;pending.reset(80,0);pending.apply(immediate,off,true,100,true,1000);auto changed=immediate;changed.value=100;pending.apply(changed,off,true,500,false,1000);assert(pending.value(1099)==80&&pending.value(1100)==100);
 pending.apply(immediate,off,true,2000,true,2000);pending.apply(immediate,off,true,2500,false,4000);assert(pending.value(5999)==100&&pending.value(6000)==255);pending.apply(changed,off,true,7000,true,5000);pending.apply(changed,off,true,8000,false,100);assert(pending.value(8000)==100&&!pending.waiting());
 pending.apply(immediate,off,true,9000,true,1000);pending.apply(immediate,off,false,9500,false,1000);assert(!pending.waiting()&&pending.value(11000)==0);
 // A delayed transition to zero holds the old light too; it is not the same as disabling a module.
 pending.reset(100,12000);auto zero=immediate;zero.value=0;pending.apply(zero,off,true,12000,true,1000);assert(pending.value(12999)==100&&pending.value(13000)==0);
 DimmerAnimation sine{SINE,1000,0,0,50,0};DimmerChannel wave;wave.reset(0,0);wave.apply(immediate,sine,true,0,true);assert(std::fabs(wave.value(250)-127.5)<.01);Dimmer fadeOut{0,1000,1000};wave.apply(fadeOut,off,true,250,true,500);assert(std::fabs(wave.value(500)-255)<.01);assert(std::fabs(wave.value(750)-127.5)<.01);assert(std::fabs(wave.value(1250)-63.75)<.01);
 DimmerChannel wrap;wrap.reset(40,0xffffff00u);wrap.apply(immediate,off,true,0xffffff00u,true,500);assert(wrap.value(243)==40&&wrap.value(244)==255);
 mockNow=10000;Actors actor;actor.begin();Runner runner(actor);runner.begin();assert(mockModes[AC_ZC_PIN]==INPUT&&mockLevels[AC_GATE_PIN]==LOW&&runner.acStatus().enabled&&runner.acStatus().requested==0);
 Show::Packet cue{};cue.run=30;cue.transition=1;cue.cue.dimmerMask=3;cue.cue.dimmers[0]=cue.cue.dimmers[1]=immediate;cue.cue.lightDelayMs[0]=500;cue.cue.lightDelayMs[1]=1000;assert(runner.apply(cue,10000));runner.tick(10499);assert(actor.dimmerValue(0)==0&&actor.dimmerValue(1)==0);runner.tick(10500);assert(actor.dimmerValue(0)==255&&actor.dimmerValue(1)==0);
 cue.transition++;assert(runner.apply(cue,10600));auto recovered=cue;recovered.run=90;recovered.transition=40;assert(runner.apply(recovered,10800,true));cue=recovered;runner.tick(10999);assert(actor.dimmerValue(1)==0);runner.tick(11000);assert(actor.dimmerValue(1)==255);cue.transition++;cue.step=1;cue.cue.dimmers[0].value=0;cue.cue.dimmers[1].value=40;assert(runner.apply(cue,12000));assert(actor.dimmerValue(0)==255&&actor.dimmerValue(1)==255);runner.tick(12500);assert(actor.dimmerValue(0)==0&&actor.dimmerValue(1)==255);runner.stop();runner.tick(15000);assert(actor.dimmerValue(0)==0&&actor.dimmerValue(1)==0);assert(runner.apply(cue,15001)&&!runner.active);
 cue.cue.lightDelayMs[0]=3600001;assert(!valid(cue.cue));cue.cue.lightDelayMs[0]=0;cue.cue.audioLoop=2;assert(!valid(cue.cue));cue.cue.audioLoop=0;cue.cue.ac.enabled=1;cue.cue.ac.animation={SQUARE,20,0,0,50,0};assert(!valid(cue.cue));cue.cue.ac.animation.periodMs=40;assert(valid(cue.cue));cue.cue.ac.delayMs=3600001;assert(!valid(cue.cue));
 // Boot/STOP keep Z-C monitoring at zero. A scene needs no separate ARM and no new acquisition.
 Mains::Pulse gate{};for(int i=0;i<=6;i++){mockNow=15940+i*10;runner.tick(mockNow);assert(!runner.acZeroForTest((uint64_t)mockNow*1000,gate));}assert(runner.acStatus().qualified);
 Show::Packet mains{};mains.run=91;mains.transition=1;mains.cue.ac.enabled=1;mains.cue.ac.dimmer.value=128;mockNow=16001;assert(runner.apply(mains,16001));assert(runner.acStatus().qualified&&runner.acStatus().requested==128&&!runner.acStatus().latched);
 mockNow=16041;runner.tick(mockNow,false);assert(!runner.acStatus().qualified&&!runner.acStatus().latched&&runner.acStatus().requested==128);
 for(int i=0;i<=6;i++){mockNow=16050+i*10;runner.tick(mockNow,false);runner.acZeroForTest((uint64_t)mockNow*1000,gate);}assert(runner.acStatus().qualified&&runner.acStatus().recoveries==1);
 runner.stop();assert(runner.acStatus().enabled&&runner.acStatus().qualified&&runner.acStatus().requested==0&&!runner.active);mockNow=16120;runner.tick(mockNow);assert(!runner.acZeroForTest((uint64_t)mockNow*1000,gate));
 mains.transition++;mockNow=16121;assert(runner.apply(mains,mockNow)&&runner.acStatus().qualified&&runner.acStatus().requested==128);
 mains.transition++;mains.cue.ac.enabled=0;mockNow=16122;assert(runner.apply(mains,mockNow)&&runner.acStatus().qualified&&runner.acStatus().requested==0);
 puts("Held light delay: previous static/animation continues until due, zero-target delayed fade, handoff from attained level, independent channels/LIVE/shorten/wrap/STOP; AC boot/STOP monitor ready, scene-only enable, Wi-Fi-loss hold, automatic Z-C reacquire without web ARM (GPIO mocked)");
}

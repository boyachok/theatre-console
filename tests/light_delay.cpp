#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <cmath>
#include "../console/score_runtime.h"
using namespace Show;
int main(){
 Dimmer d{255,1000,1000};DimmerAnimation off{};DimmerChannel light;light.reset(200,0);light.apply(d,off,true,100,true,1000);assert(light.waiting()&&light.value(1099)==0);assert(light.value(1100)==0&&!light.waiting());assert(std::fabs(light.value(1600)-127.5)<.01);
 light.apply(d,off,true,1700,false,1000);assert(std::fabs(light.value(1800)-178.5)<.01);light.apply(d,off,true,2100,false,500);assert(light.value(2200)==255); // Delay edit cannot black out or re-fade an already running light.
 light.apply(d,off,true,2300,true,500);assert(light.value(2301)==0&&light.value(2799)==0&&light.value(2800)==0);assert(std::fabs(light.value(3300)-127.5)<.01);
 Dimmer immediate{255,0,0};DimmerChannel pending;pending.reset(0,0);pending.apply(immediate,off,true,100,true,1000);auto changed=immediate;changed.value=100;pending.apply(changed,off,true,500,false,1000);assert(pending.value(1099)==0&&pending.value(1100)==100); // Unrelated LIVE edits do not postpone start.
 pending.apply(immediate,off,true,2000,true,2000);pending.apply(immediate,off,true,2500,false,4000);assert(pending.value(5999)==0&&pending.value(6000)==255);pending.apply(immediate,off,true,7000,true,5000);pending.apply(immediate,off,true,8000,false,100);assert(pending.value(8000)==255); // Shortened deadline starts now.
 pending.apply(immediate,off,true,9000,true,1000);pending.apply(immediate,off,false,9500,false,1000);assert(!pending.waiting()&&pending.value(11000)==0);pending.apply(immediate,off,true,12000,true,1000);auto zero=immediate;zero.value=0;pending.apply(zero,off,true,12500,false,1000);assert(pending.value(14000)==0&&!pending.waiting());
 DimmerAnimation sine{SINE,1000,900,0,50,0};DimmerChannel wave;wave.reset(0,0);wave.apply(immediate,sine,true,0,true,1000);assert(wave.value(999)==0&&std::fabs(wave.value(1000)-127.5)<.01&&std::fabs(wave.value(1250)-255)<.01);
 DimmerChannel wrap;wrap.reset(0,0xffffff00u);wrap.apply(immediate,off,true,0xffffff00u,true,500);assert(wrap.value(243)==0&&wrap.value(244)==255);
 mockNow=10000;Actors actor;actor.begin();Runner runner(actor);runner.begin();assert(mockModes[AC_ZC_PIN]==INPUT&&mockLevels[AC_GATE_PIN]==LOW&&!runner.acStatus().enabled);
 Show::Packet cue{};cue.run=30;cue.transition=1;cue.cue.dimmerMask=3;cue.cue.dimmers[0]=cue.cue.dimmers[1]=immediate;cue.cue.lightDelayMs[0]=500;cue.cue.lightDelayMs[1]=1000;assert(runner.apply(cue,10000));runner.tick(10499);assert(actor.dimmerValue(0)==0&&actor.dimmerValue(1)==0);runner.tick(10500);assert(actor.dimmerValue(0)==255&&actor.dimmerValue(1)==0);
 cue.transition++;assert(runner.apply(cue,10600));runner.tick(10999);assert(actor.dimmerValue(1)==0);runner.tick(11000);assert(actor.dimmerValue(1)==255);cue.transition++;cue.step=1;assert(runner.apply(cue,12000));assert(actor.dimmerValue(0)==0&&actor.dimmerValue(1)==0);runner.stop();runner.tick(15000);assert(actor.dimmerValue(0)==0&&actor.dimmerValue(1)==0);assert(runner.apply(cue,15001)&&!runner.active);
 cue.cue.lightDelayMs[0]=3600001;assert(!valid(cue.cue));cue.cue.lightDelayMs[0]=0;cue.cue.audioLoop=2;assert(!valid(cue.cue));cue.cue.audioLoop=0;cue.cue.ac.enabled=1;cue.cue.ac.animation={SQUARE,20,0,0,50,0};assert(!valid(cue.cue));cue.cue.ac.animation.periodMs=40;assert(valid(cue.cue));cue.cue.ac.delayMs=3600001;assert(!valid(cue.cue));
 Show::Packet mains{};mains.run=91;mains.transition=1;mains.cue.ac.enabled=1;mains.cue.ac.dimmer.value=128;mockNow=16000;assert(runner.apply(mains,16000));runner.tick(16001,false);assert(runner.acStatus().latched);mains.transition++;mains.cue.audio.volume=10;mockNow=16002;assert(runner.apply(mains,16002)&&runner.acStatus().latched);mains.transition++;mains.step=1;mockNow=16003;assert(runner.apply(mains,16003)&&!runner.acStatus().latched);runner.stop();assert(!runner.acStatus().enabled);
 puts("Light delay actual channels/Runner: independent deadlines, blackout during wait, fade/phase from due, LIVE stability/shortening, delay edits while running, scene restart, disable/zero/STOP cancellation, wrap, AC default-low pins and score10 bounds passed (GPIO mocked)");
}

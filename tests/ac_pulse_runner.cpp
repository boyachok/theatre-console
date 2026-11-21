#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include "../console/score_runtime.h"
int main(){
 mockPrefsBytes.clear();mockPrefsWriteOK=true;Actors actors;actors.begin();Show::Runner runner(actors);runner.begin();assert(runner.acReport().pulseUs==200);
 Show::Packet p{};p.run=8;p.transition=1;p.cue.ac.enabled=1;p.cue.ac.dimmer.value=77;assert(runner.apply(p,0)&&runner.active);assert(runner.saveAcPulse(300)==NOT_READY&&mockPrefsBytes["pulse-v1"].empty());
 runner.stop();auto writes=actors.pwmWriteCount(),relays=actors.relayWriteCount(0),offs=actors.allOffCount();auto uart=mockUARTOut.size();assert(runner.saveAcPulse(300)==APPLIED);assert(!runner.active&&runner.acReport().requested==0&&runner.acReport().pulseUs==300);assert(actors.pwmWriteCount()==writes&&actors.relayWriteCount(0)==relays&&actors.allOffCount()==offs&&mockUARTOut.size()==uart);
 assert(runner.apply(p,1)&&!runner.active);p.transition=2;mockNow=2;assert(runner.apply(p,2)&&runner.active);Mains::Pulse pulse;for(int i=0;i<=7;i++){uint64_t t=10000+i*10000;mockNow=t/1000;runner.tick(mockNow);if(runner.acZeroForTest(t,pulse))assert(pulse.pulseUs==300);}assert(runner.acStatus().qualified);
 auto report=runner.acReport();assert(sizeof(report)==64&&AcPulse::validReport(report));report.pulseUs=401;assert(!AcPulse::validReport(report));report=runner.acReport();report.reserved[0]=1;assert(!AcPulse::validReport(report));
 puts("Actual Runner: AC pulse cannot change while a scene runs; STOP-only save preserves PWM/relays/player and stopped transition latch, next explicit scene uses saved width; no physical mains test");
}

#define THEATRE_HOST_TEST
#define THEATRE_AC_LEGACY_082 1
#include <cassert>
#include <cstdio>
#include "../console/score_runtime.h"
int main(){
 mockPrefsBytes.clear();mockPrefsWriteOK=true;
 // Unsupported/invalid old pulse record is ignored and never rewritten in the rollback image.
 mockPrefsBytes["pulse-v1"]={1,2,3,4,5};auto pulseRecord=mockPrefsBytes["pulse-v1"];
 Actors first;first.begin();assert(first.setDimmerInversions(4095,4095));assert(first.apply(PWM_FREQUENCY,0,2001,true)==APPLIED);
 Actors actors;actors.begin();assert(actors.frequencyStatus().active==2001&&actors.frequencyStatus().saved==2001);assert(actors.dimmerInversions()==4095);for(int i=0;i<12;i++)assert(mockPWMFrequency[PWM_PINS[i]]==2001);
 Show::Runner runner(actors);runner.begin();assert(AcProfile::LEGACY&&!AcProfile::PULSE_CONFIGURABLE&&runner.acPulseUs()==200);
 assert(runner.saveAcPulse(200)==INVALID&&runner.saveAcPulse(400)==INVALID&&mockPrefsBytes["pulse-v1"]==pulseRecord);
 Show::Packet p{};p.run=42;p.transition=1;p.step=2;p.cue.ac.enabled=1;p.cue.ac.dimmer.value=77;p.cue.dimmerMask=4095;for(int i=0;i<12;i++)p.cue.dimmers[i].value=(i+1)*10;
 assert(runner.apply(p,0)&&runner.active);Mains::Pulse pulse;bool sent=false;
 for(uint32_t t=10;t<=80;t+=10){mockNow=t;runner.tick(t);sent=runner.acZeroForTest(t*1000ULL,pulse);if(sent)assert(pulse.fire>pulse.zero&&pulse.fire+200<pulse.end);}
 assert(sent&&runner.acStatus().qualified);auto count=actors.pwmWriteCount();auto seq=runner.sequence;auto run=runner.run;auto step=runner.step;
 // Keep controller healthy while Z-C disappears. No power-cycle, GO, re-apply or scene change.
 for(uint32_t t=81;t<=120;t++){mockNow=t;runner.tick(t);}
 assert(!runner.acStatus().qualified&&!runner.acStatus().latched&&runner.acStatus().fault==Mains::ZERO_LOST&&runner.active);
 for(int i=0;i<=6;i++){uint32_t t=130+i*10;mockNow=t;runner.tick(t);bool s=runner.acZeroForTest(t*1000ULL,pulse);assert(s==(i==6));}
 assert(runner.acStatus().qualified&&!runner.acStatus().latched&&runner.acStatus().recoveries==1&&runner.active&&runner.sequence==seq&&runner.run==run&&runner.step==step);
 assert(actors.pwmWriteCount()==count);for(int i=0;i<12;i++)assert(actors.dimmerValue(i)==(i+1)*10&&mockPWMFrequency[PWM_PINS[i]]==2001);
 runner.stop();auto stoppedWrites=actors.pwmWriteCount();for(uint32_t t=200;t<=280;t+=10){mockNow=t;runner.tick(t);assert(!runner.acZeroForTest(t*1000ULL,pulse));}
 assert(!runner.active&&runner.acStatus().requested==0&&actors.pwmWriteCount()==stoppedWrites&&mockPrefsBytes["pulse-v1"]==pulseRecord);
 puts("Targeted AC082 Runner: fixed200us/ignored untouched pulse NVS, adjustable12-channel PWM2001Hz/inversions retained; Z-C loss holds scene and recovers after6 intervals without reboot/re-GO, no extra DIM writes; STOP still authoritative. Host simulation only.");
}

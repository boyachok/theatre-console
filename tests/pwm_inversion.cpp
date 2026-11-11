#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include "../console/score_runtime.h"
int main(){
 mockPrefsBytes.clear();PwmSettings legacy;legacy.inverted=1;legacy.level[0]=91;auto *bytes=(const uint8_t*)&legacy;mockPrefsBytes["pwmConfig"]={bytes,bytes+sizeof(legacy)};auto old=mockPrefsBytes["pwmConfig"];
 Actors actor;actor.begin();assert(actor.dimmerInversions()==1&&actor.dimmerValue(0)==0&&mockPWM[PWM_PINS[0]]==255);assert(actor.setDimmerInversions(0,0)&&actor.dimmerInversions()==1); // Old JSON leaves existing polarity intact.
 assert(actor.setDimmerInversions(0x801,0xfff));assert(actor.pwmStatus().level[0]==91&&reportedDimmerInversions(actor.pwmStatus())==0x801);
 for(int i=0;i<12;i++){assert(actor.setDimmer(i,73));assert(mockPWM[PWM_PINS[i]]==((0x801&(1<<i))?182:73));assert(actor.dimmerValue(i)==73);assert(mockPWMFrequency[PWM_PINS[i]]==25000);}
 actor.off();for(int i=0;i<12;i++)assert(mockPWM[PWM_PINS[i]]==((0x801&(1<<i))?255:0));assert(mockPrefsBytes["pwmConfig"]==old);
 Actors reboot;reboot.begin();assert(reboot.dimmerInversions()==0x801&&reboot.pwmStatus().level[0]==91);assert(mockPWM[PWM_PINS[11]]==255&&reboot.dimmerValue(11)==0);
 assert(reboot.setDimmerInversions(0,0xfff));for(int i=0;i<12;i++)assert(mockPWM[PWM_PINS[i]]==0);assert(!reboot.setDimmerInversions(2,1)&&!reboot.setDimmerInversions(0,4096));
 mockPrefsWriteOK=false;assert(!reboot.setDimmerInversions(1,1)&&reboot.dimmerInversions()==0);mockPrefsWriteOK=true;
 assert(reboot.apply(PWM_CONFIG,1,256+71,true)==APPLIED&&reboot.dimmerInversions()==1&&reboot.pwmStatus().level[0]==71);Actors again;again.begin();assert(again.dimmerInversions()==1&&again.pwmStatus().level[0]==71);
 Show::Runner runner(again);runner.begin();Show::Packet p{};p.run=11;p.transition=1;p.cue.dimmerMask=0xfff;p.cue.inversionSet=0xfff;p.cue.invertedMask=1<<10;for(auto &d:p.cue.dimmers)d.value=100;assert(runner.apply(p,1000));for(int i=0;i<12;i++)assert(mockPWM[PWM_PINS[i]]==(i==10?155:100));runner.stop();assert(mockPWM[PWM_PINS[10]]==255&&again.dimmerValue(10)==0);
 // Losing a heartbeat rejects new activation but never blackouts already accepted legacy outputs/relay.
 assert(again.apply(RELAY_CONFIG,0,1,true)==APPLIED&&again.apply(RELAY,0,1,true)==APPLIED);assert(again.setDimmer(3,120));again.tick(false);assert(again.dimmerValue(3)==120&&again.snapshot().relayOn);assert(again.apply(PWM,4,200,false)==LEASE_EXPIRED&&again.dimmerValue(3)==120&&again.snapshot().relayOn);
 puts("12 PWM inversions: 8 LEDC+4 MCPWM transform once, logical preview/state unchanged, inverted OFF/STOP/startup, legacy NVS preserved, atomic canonical record/presets/reboot/failures, explicit vs absent scene mask, no Wi-Fi blackout passed (hardware mocked)");
}

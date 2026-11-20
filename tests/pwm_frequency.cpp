#ifndef THEATRE_HOST_TEST
#define THEATRE_HOST_TEST
#endif
#include <cassert>
#include <cstdio>
#include "../console/actors.h"
int main(){
 mockPrefsBytes.clear();mockRelayConfig=0;mockAttachOK=mockPrefsWriteOK=true;
 assert(validControl(PWM_FREQUENCY,0,2000)&&validControl(PWM_FREQUENCY,0,25000));assert(!validControl(PWM_FREQUENCY,1,2000)&&!validControl(PWM_FREQUENCY,0,1999)&&!validControl(PWM_FREQUENCY,0,25001));
 assert(PwmFrequency::valid(25000,8)&&PwmFrequency::valid(25000,10)&&!PwmFrequency::valid(25000,12)&&PwmFrequency::valid(9765,12));
 Actors first;first.begin();auto initial=first.frequencyStatus();assert(initial.active==25000&&initial.saved==25000&&PwmFrequency::validReport(initial));
 first.setDimmerInversions(4095,4095);first.setDimmer(11,40);assert(first.apply(PWM_FREQUENCY,0,2000,true)==NOT_READY);first.setDimmer(11,0);
 assert(first.apply(PWM_FREQUENCY,0,2000,false)==LEASE_EXPIRED);
 const auto writes=first.pwmWriteCount(),offs=first.allOffCount(),relays=first.relayWriteCount(0);auto nvs=mockPrefsBytes;
 assert(first.apply(PWM_FREQUENCY,0,2001,true)==APPLIED);auto pending=first.frequencyStatus();assert(pending.active==25000&&pending.saved==2001&&PwmFrequency::validReport(pending));
 assert(first.pwmWriteCount()==writes&&first.allOffCount()==offs&&first.relayWriteCount(0)==relays);for(int i=0;i<DIMMER_COUNT;i++)assert(mockPWMFrequency[PWM_PINS[i]]==25000);
 assert(mockPrefsBytes["dimConfig"]==nvs["dimConfig"]&&mockPrefsBytes["pwmConfig"]==nvs["pwmConfig"]);
 auto record=mockPrefsBytes["pwm-freq"];mockPrefsWriteOK=false;assert(first.apply(PWM_FREQUENCY,0,4000,true)==IO_ERROR);assert(mockPrefsBytes["pwm-freq"]==record&&first.frequencyStatus().saved==2001);mockPrefsWriteOK=true;
 Actors reboot;reboot.begin();assert(reboot.frequencyStatus().active==2001&&reboot.frequencyStatus().saved==2001&&reboot.dimmerInversions()==4095);for(int i=0;i<DIMMER_COUNT;i++)assert(mockPWMFrequency[PWM_PINS[i]]==2001&&reboot.dimmerValue(i)==0);
 for(int i=0;i<DIMMER_COUNT;i++)assert(reboot.setDimmer(i,128));assert(reboot.apply(PWM_FREQUENCY,0,3000,true)==NOT_READY);reboot.off();
 mockPrefsBytes["pwm-freq"][3]^=1;Actors damaged;damaged.begin();assert(damaged.snapshot().fault==5&&!damaged.setDimmer(0,20));assert(damaged.apply(PWM_FREQUENCY,0,25000,true)==APPLIED);Actors repaired;repaired.begin();assert(repaired.frequencyStatus().active==25000&&repaired.snapshot().fault==0);
 auto invalid=initial;invalid.bits=12;assert(!PwmFrequency::validReport(invalid));invalid=initial;invalid.periodTicks++;assert(!PwmFrequency::validReport(invalid));
 puts("12-channel PWM frequency: integer2000..25000, precision bound, CRC NVS separate from levels/inversion, STOP-only persisted setting, no hardware writes/reboot/relay/player changes, all12 adopt after explicit boot, no lease/active/failed NVS/corrupt record fail closed; native model, NOT electrical waveforms");
}

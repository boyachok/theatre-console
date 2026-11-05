#include <cassert>
#include <cstdio>
#include "../control/actors.h"
// Packed wire fields must be read by index: GCC rejects range-for array references.
int main(){
  Actors a;a.begin();auto s=a.snapshot();assert(!s.ready&&!s.relayOn&&s.relayMode==0);
  const uint8_t expected[]={4,5,6,7,9,10,11,12};
  for(int i=0;i<8;i++)assert(PWM_PINS[i]==expected[i]&&s.pwm[i]==0&&mockPWM[PWM_PINS[i]]==0);
  assert(PWM_HZ==25000&&PWM_BITS==8&&mockLEDCClock==LEDC_USE_XTAL_CLK&&a.pwmFrequency()==25000);
  for(uint8_t pin:PWM_PINS)assert(mockPWMFrequency[pin]==25000&&mockPWMResolution[pin]==8);
  assert(mockModes[RELAY_PIN]==INPUT);
  assert(a.apply(PWM,8,200,false)==LEASE_EXPIRED);
  a.tick(true);s=a.snapshot();assert(s.ready);for(int i=0;i<8;i++)assert(s.pwm[i]==0);
  assert(a.apply(LEGACY_ARM,0,0,true)==INVALID);
  assert(a.apply(LEGACY_AUDIO_ENABLE,0,1,true)==INVALID);
  for(int ch=1;ch<=8;ch++)assert(a.apply(PWM,ch,100+ch,true)==APPLIED&&mockPWM[PWM_PINS[ch-1]]==(uint32_t)(100+ch));
  a.tick(false);s=a.snapshot();assert(!s.ready&&s.fault==1);for(int i=0;i<8;i++)assert(s.pwm[i]==0);
  a.tick(true);s=a.snapshot();assert(s.ready);for(int i=0;i<8;i++)assert(s.pwm[i]==0); // Ready again, NOT restored.
  assert(a.apply(PWM,8,77,true)==APPLIED&&mockPWM[12]==77);
  assert(a.apply(RELAY_CONFIG,0,2,true)==APPLIED);assert(a.snapshot().pwm[7]==77);
  assert(mockLevels[8]==HIGH);assert(a.apply(RELAY,0,1,true)==APPLIED&&mockLevels[8]==LOW);
  assert(a.apply(ALL_OFF,0,0,true)==APPLIED&&a.snapshot().ready&&mockLevels[8]==HIGH);
  assert(a.apply(PWM,1,42,true)==APPLIED); // No re-enable step after ALL OFF.
  auto defaults=a.pwmStatus();for(int i=0;i<8;i++)assert(defaults.level[i]==255);
  assert(a.apply(PWM_SWITCH,2,1,true)==APPLIED&&a.snapshot().pwm[1]==255);
  assert(a.apply(PWM_CONFIG,2,80,true)==APPLIED&&a.snapshot().pwm[1]==80&&a.snapshot().pwm[0]==42);
  assert(a.apply(RELAY,0,1,true)==APPLIED);
  assert(a.apply(PWM_CONFIG,2,80+256,true)==APPLIED&&a.snapshot().pwm[1]==80&&mockPWM[5]==175&&a.snapshot().relayOn);
  assert(a.apply(RELAY_CONFIG,0,1,true)==APPLIED&&a.snapshot().relayOn&&mockLevels[8]==HIGH&&a.snapshot().pwm[0]==42);
  assert(a.apply(RELAY_CONFIG,0,2,true)==APPLIED&&a.snapshot().relayOn&&mockLevels[8]==LOW&&a.snapshot().pwm[1]==80);
  assert(a.apply(PWM_SWITCH,2,0,true)==APPLIED&&mockPWM[5]==255); // Inverted logical OFF.
  assert(a.apply(PWM_CONFIG,2,0+256,true)==APPLIED&&a.apply(PWM_SWITCH,2,1,true)==APPLIED);
  assert(a.snapshot().pwm[1]==0&&(a.pwmStatus().onMask&2)); // Explicit zero preset still ON.
  assert(a.apply(PWM_CONFIG,2,50+256,true)==APPLIED&&a.snapshot().pwm[1]==50&&mockPWM[5]==205);
  assert(a.apply(PWM,2,75,true)==APPLIED);
  assert(a.apply(PWM_CONFIG,2,50,true)==APPLIED&&a.snapshot().pwm[1]==75); // Inversion-only preserves live level.
  mockPrefsWriteOK=false;assert(a.apply(PWM_CONFIG,2,99,true)==IO_ERROR&&a.snapshot().pwm[1]==75&&a.snapshot().relayOn);mockPrefsWriteOK=true;
  assert(a.apply(PWM_CONFIG,2,50+256,true)==APPLIED);
  Actors reboot;reboot.begin();assert(reboot.pwmStatus().level[1]==50&&(reboot.pwmStatus().inverted&2));
  assert(reboot.pwmStatus().onMask==0&&reboot.snapshot().pwm[1]==0&&mockPWM[5]==255); // No restoration at boot.
  reboot.tick(true);assert(reboot.apply(PWM_SWITCH,2,1,true)==APPLIED&&reboot.snapshot().pwm[1]==50);
  reboot.tick(false);assert(!reboot.pwmStatus().onMask&&mockPWM[5]==255);
  reboot.tick(true);assert(reboot.snapshot().pwm[1]==0);
  Actors b;b.begin();assert(mockLevels[8]==HIGH&&!b.snapshot().ready);
  assert(b.apply(AUDIO_TRACK,0,1,true)==PLAYER_OFFLINE);
  mockNow=2000;const uint8_t reply[]={0xAA,1,1,1,0xAD};for(uint8_t c:reply)mockUARTIn.push_back(c);
  b.tick(true);assert(b.player.ready());assert(b.apply(AUDIO_VOLUME,0,10,true)==APPLIED);
  assert(b.apply(AUDIO_TRACK,0,1,true)==APPLIED); // No audio enable or ARM.
  b.tick(false);assert(!b.snapshot().ready&&b.snapshot().fault==1);
  assert(b.apply(AUDIO_NEXT,0,0,false)==LEASE_EXPIRED);
  assert(b.apply(AUDIO_STOP,0,0,false)==APPLIED);
  b.tick(true);assert(b.apply(AUDIO_TRACK,0,1,true)==APPLIED);
  mockAttachOK=false;Actors broken;broken.begin();assert(broken.apply(PWM,1,10,true)==IO_ERROR);
  mockAttachOK=true;mockClockOK=false;Actors badClock;badClock.begin();
  assert(badClock.pwmFrequency()==0&&badClock.apply(PWM,1,10,true)==IO_ERROR);
  s=badClock.snapshot();for(int i=0;i<8;i++)assert(s.pwm[i]==0);
  puts("8-channel 25 kHz/XTAL/8-bit PWM, clock failure, zero startup, direct sends, safe recovery, active-low relay and always-ready audio tests passed");
}

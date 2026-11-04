#include <Arduino.h>
#include <Preferences.h>
#include <cassert>
#include <cstdio>
#include "../control/protocol.h"
namespace old04 {
#include "../wifi04/actors.h"
}
ControlState normalized(old04::Actors &a){
  auto state=a.snapshot();static_assert(sizeof(state)==sizeof(LegacyState),"Archived state size");
  LegacyState legacy;memcpy(&legacy,&state,sizeof(legacy));return expandLegacy(legacy);
}
int main(){
  old04::Actors old;old.begin();mockNow=2000;
  const uint8_t reply[]={0xAA,1,1,1,0xAD};for(uint8_t b:reply)mockUARTIn.push_back(b);old.tick(true);
  assert(old.player.ready());auto s=normalized(old);assert(!s.ready&&!s.audioEnabled);
  // Execute the exact numeric preparations against the archived 0.4.1 actor implementation.
  Op prep=legacyPreparation(3,s,AUDIO_TRACK,1);assert(prep==LEGACY_ARM);
  assert(old.apply((old04::Op)prep,0,0,true)==old04::APPLIED);
  for(int i=0;i<4;i++)assert(old.snapshot().pwm[i]==0);
  s=normalized(old);prep=legacyPreparation(3,s,AUDIO_TRACK,1);assert(prep==LEGACY_AUDIO_ENABLE);
  assert(old.apply((old04::Op)prep,0,1,true)==old04::APPLIED);
  s=normalized(old);assert(!legacyPreparation(3,s,AUDIO_TRACK,1));
  assert(old.apply((old04::Op)AUDIO_TRACK,0,1,true)==old04::APPLIED);
  assert(old.apply((old04::Op)PWM,4,200,true)==old04::APPLIED&&mockPWM[7]==200);
  assert(!old04::validControl((old04::Op)PWM,5,200));
  prep=legacyPreparation(3,normalized(old),RELAY_CONFIG,2);assert(prep==LEGACY_DISARM);
  assert(old.apply((old04::Op)prep,0,0,true)==old04::APPLIED);
  assert(!legacyPreparation(3,normalized(old),RELAY_CONFIG,2));
  assert(old.apply((old04::Op)RELAY_CONFIG,0,2,true)==old04::APPLIED&&mockLevels[8]==HIGH);
  assert(old.apply((old04::Op)ALL_OFF,0,0,true)==old04::APPLIED);
  assert(legacyPreparation(3,normalized(old),PWM,100)==LEGACY_ARM);
  puts("Compatibility preparations verified against archived 0.4.1 actors without energizing outputs during preparation");
}

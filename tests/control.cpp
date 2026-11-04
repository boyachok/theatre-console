#include <cassert>
#include <cstdio>
#include <cstring>
#include "../control/control_types.h"
#include "../control/xy_protocol.h"
int main(){
  assert(safeDuringOTA(ALL_OFF,0)&&safeDuringOTA(PWM_SWITCH,0)&&!safeDuringOTA(PWM_SWITCH,1)&&!safeDuringOTA(PWM_CONFIG,0)&&!safeDuringOTA(AUDIO_TRACK,1));
  assert(freshDeadline(100,800)&&!freshDeadline(100,2100)&&!freshDeadline(800,100));
  assert(freshDeadline(0xfffffff0,20)&&!freshDeadline(20,0xfffffff0));
  assert(sizeof(ControlState)==32&&sizeof(LegacyState)==24);
  for(unsigned i=1;i<=8;i++){assert(validControl(PWM,i,0));assert(validControl(PWM,i,255));}
  for(int ch=1;ch<=8;ch++){assert(validControl(PWM_SWITCH,ch,1)&&validControl(PWM_SWITCH,ch,0));assert(validControl(PWM_CONFIG,ch,511)&&validControl(PWM_CONFIG,ch,0));}
  assert(!validControl(PWM_CONFIG,9,1)&&!validControl(PWM_CONFIG,1,512)&&!validControl(PWM_SWITCH,1,2));
  assert(needsLiveLink(PWM_SWITCH,1)&&!needsLiveLink(PWM_SWITCH,0));
  assert(!validControl(PWM,0,100)&&!validControl(PWM,9,100)&&!validControl(PWM,1,256));
  assert(!validControl(LEGACY_ARM,0,0)&&!validControl(LEGACY_DISARM,0,0)&&!validControl(LEGACY_AUDIO_ENABLE,0,1));
  assert(validControl(RELAY,0,1)&&!validControl(RELAY,0,2));
  assert(validControl(RELAY_CONFIG,0,2)&&!validControl(RELAY_CONFIG,0,3));
  assert(validControl(AUDIO_TRACK,0,65535)&&!validControl(AUDIO_TRACK,0,0));
  assert(validControl(AUDIO_VOLUME,0,30)&&!validControl(AUDIO_VOLUME,0,31));
  assert(validControl(AUDIO_MODE,0,7)&&!validControl(AUDIO_MODE,0,8));
  assert(validControl(AUDIO_EQ,0,4)&&!validControl(AUDIO_EQ,0,5));
  assert(needsLiveLink(PWM,1)&&!needsLiveLink(PWM,0));assert(needsLiveLink(RELAY,1)&&!needsLiveLink(RELAY,0));
  assert(needsLiveLink(AUDIO_TRACK,1)&&needsLiveLink(AUDIO_NEXT,0)&&!needsLiveLink(AUDIO_STOP,0));
  assert(newSequence(1,0)&&newSequence(20,19)&&!newSequence(19,19)&&!newSequence(18,19));
  assert(newSequence(1,0xfffffffe)&&!newSequence(0xfffffffe,1));
  ControlState s{};assert(legacyPreparation(4,s,PWM,200)==0);
  assert(legacyPreparation(3,s,PWM,200)==LEGACY_ARM);
  assert(legacyPreparation(3,s,AUDIO_TRACK,1)==LEGACY_ARM);
  s.ready=1;assert(legacyPreparation(3,s,AUDIO_TRACK,1)==LEGACY_AUDIO_ENABLE);
  s.audioEnabled=1;assert(legacyPreparation(3,s,AUDIO_TRACK,1)==0);
  assert(legacyPreparation(3,s,RELAY_CONFIG,1)==LEGACY_DISARM);
  assert(legacyPreparation(3,s,ALL_OFF,0)==0&&legacyPreparation(3,s,AUDIO_STOP,0)==0);
  s.pwm[0]=123;s.pwm[7]=255;s.audioTrack=300;s.audioAge=42;
  auto old=shrinkState(s);auto restored=expandLegacy(old);
  assert(restored.ready&&restored.pwm[0]==123&&restored.pwm[7]==0&&restored.audioTrack==300&&restored.audioAge==42);
  uint8_t frame[36],data[]={0,1};size_t len=xyFrame(0x07,data,2,frame);
  const uint8_t play1[]={0xAA,0x07,0x02,0,1,0xB4};assert(len==6&&!memcmp(frame,play1,6));
  len=xyFrame(4,nullptr,0,frame);const uint8_t stop[]={0xAA,4,0,0xAE};assert(len==4&&!memcmp(frame,stop,4));
  uint8_t volume[]={30};xyFrame(0x13,volume,1,frame);assert(frame[4]==0xDC);assert(xyFrame(1,nullptr,33,frame)==0);
  XYParser p;bool ready=false;for(uint8_t c:play1)ready=p.feed(c);assert(ready);
  uint8_t bad[]={0xAA,1,1,1,0};for(uint8_t c:bad)ready=p.feed(c);assert(!ready);
  uint8_t status[]={0xAA,1,1,1,0xAD};for(uint8_t c:status)ready=p.feed(c);assert(ready);
  for(unsigned i=0;i<10000;i++)p.feed((uint8_t)(i*37));
  puts("8 PWM, automatic readiness policy, legacy preparation, wire conversion and UART codec checks passed");
}

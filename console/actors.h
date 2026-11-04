#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "control_types.h"
#include "hardware.h"
#include "pwm_settings.h"
#include "player.h"
#include "extra_pwm.h"
class Actors {
  uint16_t duty[DIMMER_COUNT]{};PwmSettings settings;ExtraPwm extra;
  uint8_t relayMode=0,fault=0;uint16_t onMask=0;uint32_t configRevision=1;
  bool linkReady=false,relayOn=false,pwmReady=true;
  Preferences config;
  void relayWrite(bool on){
    if(!relayMode){pinMode(RELAY_PIN,INPUT);relayOn=false;return;}
    digitalWrite(RELAY_PIN,(on==(relayMode==1))?HIGH:LOW);pinMode(RELAY_PIN,OUTPUT);relayOn=on;
  }
  bool writePWM(uint8_t index,uint8_t value){
    bool ok=index<PWM_COUNT?ledcWrite(PWM_PINS[index],physicalPWM(value,settings.inverted&(1<<index))):extra.write(index-PWM_COUNT,value);
    if(!ok)return false;
    duty[index]=value;return true;
  }
public:
  Player player;
  void begin(){
    config.begin("stage-io",false);relayMode=config.getUChar("relayMode",0);if(relayMode>2)relayMode=0;
    PwmSettings loaded;
    if(config.getBytesLength("pwmConfig")==sizeof(loaded)&&config.getBytes("pwmConfig",&loaded,sizeof(loaded))==sizeof(loaded)&&loaded.valid())settings=loaded;
    relayWrite(false);
    if(!ledcSetClockSource(LEDC_USE_XTAL_CLK))pwmReady=false;
    for(int i=0;i<PWM_COUNT;i++){
      uint8_t pin=PWM_PINS[i],inactive=physicalPWM(0,settings.inverted&(1<<i));
      digitalWrite(pin,inactive?HIGH:LOW);pinMode(pin,OUTPUT);
      if(!ledcAttach(pin,PWM_HZ,PWM_BITS)||!writePWM(i,0))pwmReady=false;
    }
    for(int i=PWM_COUNT;i<DIMMER_COUNT;i++){digitalWrite(PWM_PINS[i],LOW);pinMode(PWM_PINS[i],OUTPUT);}
    if(!extra.begin(PWM_PINS+PWM_COUNT))pwmReady=false;
    if(!pwmReady)fault=5;player.begin();
  }
  uint8_t dimmerValue(uint8_t index)const{return index<DIMMER_COUNT?duty[index]:0;}
  bool setDimmer(uint8_t index,uint8_t value){if(index>=DIMMER_COUNT||!pwmReady||!writePWM(index,value))return false;if(value)onMask|=1U<<index;else onMask&=~(1U<<index);return true;}
  uint32_t pwmFrequency()const{return pwmReady?ledcReadFreq(PWM_PINS[0]):0;}
  PwmStatus pwmStatus()const{
    PwmStatus s{};for(int i=0;i<PWM_COUNT;i++)s.level[i]=settings.level[i];
    s.inverted=settings.inverted;s.onMask=onMask;s.revision=configRevision;return s;
  }
  void off(uint8_t reason=0){
    relayWrite(false);for(int i=0;i<DIMMER_COUNT;i++){writePWM(i,0);duty[i]=0;}onMask=0;
    player.stop();fault=pwmReady?reason:5;
  }
  void tick(bool heartbeatFresh){if(linkReady&&!heartbeatFresh)off(1);linkReady=heartbeatFresh;player.tick();}
  ControlState snapshot()const{
    ControlState s{};for(int i=0;i<PWM_COUNT;i++)s.pwm[i]=duty[i];
    s.ready=linkReady;s.relayOn=relayOn;s.relayMode=relayMode;s.fault=fault;s.audioEnabled=1;
    s.audioState=player.state;s.audioVolume=player.volume;s.audioMode=player.mode;s.audioEq=player.eq;s.audioDrive=player.drive;
    s.audioTrack=player.track;s.audioTracks=player.tracks;
    s.audioAge=player.lastRx?(uint16_t)std::min((uint32_t)65534,millis()-player.lastRx):65535;return s;
  }
  Result apply(Op op,uint8_t channel,uint16_t value,bool heartbeatFresh){
    if(!validControl(op,channel,value))return INVALID;
    linkReady=heartbeatFresh;
    if(op==ALL_OFF){off();return APPLIED;}
    if(needsLiveLink(op,value)&&!heartbeatFresh){off(1);return LEASE_EXPIRED;}
    if(op==PWM||op==PWM_SWITCH){
      uint8_t i=channel-1,v=op==PWM?value:(value?settings.level[i]:0);
      if(!pwmReady||!writePWM(i,v)){off(5);return IO_ERROR;}
      if(op==PWM_SWITCH?value:v>0)onMask|=1<<i;else onMask&=~(1<<i);
      fault=0;return APPLIED;
    }
    if(op==PWM_CONFIG){
      if(!heartbeatFresh&&onMask)off(1);
      uint8_t i=channel-1,level=value&255;bool inverse=value&256;
      if(settings.level[i]==level&&bool(settings.inverted&(1<<i))==inverse)return APPLIED;
      PwmSettings next=settings;next.level[i]=level;
      if(inverse)next.inverted|=1<<i;else next.inverted&=~(1<<i);
      if(config.putBytes("pwmConfig",&next,sizeof(next))!=sizeof(next))return IO_ERROR;
      uint8_t current=(onMask&(1<<i))&&settings.level[i]!=level?level:duty[i];
      settings=next;configRevision++;
      if(!pwmReady||!writePWM(i,current)){off(5);return IO_ERROR;}
      return APPLIED;
    }
    if(op==RELAY){if(!relayMode)return INVALID;relayWrite(value);if(pwmReady)fault=0;return APPLIED;}
    if(op==RELAY_CONFIG){
      if(relayMode==value)return APPLIED;
      if(config.putUChar("relayMode",value)!=1)return IO_ERROR;
      bool desired=relayOn;relayMode=value;relayWrite(desired);return APPLIED;
    }
    if(op==AUDIO_STOP){player.stop();return APPLIED;}
    if(op==AUDIO_QUERY){player.poll();return APPLIED;}
    if(op==UART_CLEAR){player.clearStats();return APPLIED;}
    if(op==UART_POLL){player.autoPoll=value;return APPLIED;}
    if(op==AUDIO_PAUSE)return player.enqueue(0x03)?APPLIED:IO_ERROR;
    if(!player.ready())return PLAYER_OFFLINE;
    bool ok=false;
    switch(op){
      case AUDIO_TRACK:ok=player.enqueue(0x07,value,2);break;
      case AUDIO_RESUME:ok=player.enqueue(0x02);break;
      case AUDIO_NEXT:ok=player.enqueue(0x06);break;
      case AUDIO_PREV:ok=player.enqueue(0x05);break;
      case AUDIO_VOLUME:ok=player.enqueue(0x13,value,1);if(ok)player.volume=value;break;
      case AUDIO_MODE:ok=player.enqueue(0x18,value,1);if(ok)player.mode=value;break;
      case AUDIO_EQ:ok=player.enqueue(0x1A,value,1);if(ok)player.eq=value;break;
      case AUDIO_DRIVE:ok=player.enqueue(0x0B,value,1);break;
      default:return INVALID;
    }
    return ok?APPLIED:IO_ERROR;
  }
};

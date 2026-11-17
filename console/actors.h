#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "control_types.h"
#include "hardware.h"
#include "pwm_settings.h"
#include "player.h"
#include "extra_pwm.h"
class Actors {
  float duty[DIMMER_COUNT]{};PwmSettings settings;ExtraPwm extra;uint16_t invertedMask=0;
  uint8_t relayMode=0,fault=0;uint16_t onMask=0;uint32_t configRevision=1;
  bool linkReady=false,relayOn=false,pwmReady=true;
  Preferences config;
  // Controller-owned RAM trace: records software writes, NOT a GPIO waveform.
  struct OutputEvent {uint32_t atUs=0;uint8_t kind=0,channel=0;float value=0;};
  OutputEvent outputEvents[32]{};uint8_t outputHead=0,outputCount=0;
  uint32_t pwmWrites=0,relayWrites[2]{},offCalls=0;
  void recordOutput(uint8_t kind,uint8_t channel,float value){outputEvents[outputHead]={micros(),kind,channel,value};outputHead=(outputHead+1)%32;if(outputCount<32)outputCount++;}
  bool saveDimSettings(const PwmSettings &next,uint16_t inverse){DimSettings saved;saved.inverted=inverse;memcpy(saved.level,next.level,PWM_COUNT);return config.putBytes("dimConfig",&saved,sizeof(saved))==sizeof(saved);}
  void relayWrite(bool on){
    relayWrites[0]++;recordOutput(2,1,on);
    if(!relayMode){pinMode(RELAY_PIN,INPUT);relayOn=false;return;}
    digitalWrite(RELAY_PIN,(on==(relayMode==1))?HIGH:LOW);pinMode(RELAY_PIN,OUTPUT);relayOn=on;
  }
  bool writePWM(uint8_t index,float value){
    bool inverse=invertedMask&(1<<index);float shaped=PwmCurve::shape(value);if(inverse)shaped=1.f-shaped;
    bool ok=index<PWM_COUNT?ledcWrite(PWM_PINS[index],PwmCurve::duty(value,inverse)):
      (PWM_BITS==8&&THEATRE_PWM_GAMMA==1.f&&THEATRE_PWM_GRAY==.5f?extra.write(index-PWM_COUNT,physicalPWM((uint8_t)value,inverse)):extra.writeFine(index-PWM_COUNT,shaped*255.f));
    if(!ok)return false;
    duty[index]=value;pwmWrites++;recordOutput(1,index+1,value);return true;
  }
public:
  Player player;
  void recordRelay2(bool on){relayWrites[1]++;recordOutput(2,2,on);}
  uint32_t pwmWriteCount()const{return pwmWrites;}
  uint32_t relayWriteCount(uint8_t index)const{return index<2?relayWrites[index]:0;}
  uint32_t allOffCount()const{return offCalls;}
  uint8_t outputEventCount()const{return outputCount;}
  const auto &outputEvent(uint8_t i)const{return outputEvents[(outputHead+32-outputCount+i)%32];}
  void begin(){
    config.begin("stage-io",false);relayMode=config.getUChar("relayMode",0);if(relayMode>2)relayMode=0;
    PwmSettings loaded;
    if(config.getBytesLength("pwmConfig")==sizeof(loaded)&&config.getBytes("pwmConfig",&loaded,sizeof(loaded))==sizeof(loaded)&&loaded.valid())settings=loaded;
    invertedMask=settings.inverted;DimSettings migrated;
    if(config.getBytesLength("dimConfig")==sizeof(migrated)&&config.getBytes("dimConfig",&migrated,sizeof(migrated))==sizeof(migrated)&&migrated.valid()){invertedMask=migrated.inverted;settings.inverted=invertedMask&255;memcpy(settings.level,migrated.level,PWM_COUNT);}
    relayWrite(false);
    if(!ledcSetClockSource(LEDC_USE_XTAL_CLK))pwmReady=false;
    for(int i=0;i<PWM_COUNT;i++){
      uint8_t pin=PWM_PINS[i],inactive=physicalPWM(0,invertedMask&(1<<i));
      digitalWrite(pin,inactive?HIGH:LOW);pinMode(pin,OUTPUT);
      if(!ledcAttach(pin,PWM_HZ,PWM_BITS)||!writePWM(i,0))pwmReady=false;
    }
    for(int i=PWM_COUNT;i<DIMMER_COUNT;i++){digitalWrite(PWM_PINS[i],(invertedMask&(1<<i))?HIGH:LOW);pinMode(PWM_PINS[i],OUTPUT);}
    if(!extra.begin(PWM_PINS+PWM_COUNT,(invertedMask>>PWM_COUNT)&15))pwmReady=false;
    for(int i=PWM_COUNT;i<DIMMER_COUNT;i++)if(!writePWM(i,0))pwmReady=false;
    if(!pwmReady)fault=5;player.begin();
  }
  uint16_t dimmerInversions()const{return invertedMask;}
  bool setDimmerInversions(uint16_t inverse,uint16_t configured){
    if((configured&~4095)||(inverse&~configured))return false;
    uint16_t next=(invertedMask&~configured)|(inverse&configured);if(next==invertedMask)return true;
    if(!saveDimSettings(settings,next))return false;
    uint16_t changed=next^invertedMask;invertedMask=next;settings.inverted=next&255;configRevision++;
    bool ok=pwmReady;for(int i=0;i<DIMMER_COUNT;i++)if(changed&(1<<i))ok=writePWM(i,duty[i])&&ok;
    if(!ok)off(5);return ok;
  }
  uint8_t dimmerValue(uint8_t index)const{return index<DIMMER_COUNT?(uint8_t)duty[index]:0;}
  float dimmerFineValue(uint8_t index)const{return index<DIMMER_COUNT?duty[index]:0;}
  bool setDimmerFine(uint8_t index,float value){if(index>=DIMMER_COUNT||!pwmReady||!std::isfinite(value)||value<0||value>255||!writePWM(index,value))return false;if(value>0)onMask|=1U<<index;else onMask&=~(1U<<index);return true;}
  bool setDimmer(uint8_t index,uint8_t value){if(index>=DIMMER_COUNT||!pwmReady||!writePWM(index,value))return false;if(value)onMask|=1U<<index;else onMask&=~(1U<<index);return true;}
  uint32_t pwmFrequency()const{return pwmReady?ledcReadFreq(PWM_PINS[0]):0;}
  PwmStatus pwmStatus()const{
    PwmStatus s{};for(int i=0;i<PWM_COUNT;i++)s.level[i]=settings.level[i];
    s.inverted=settings.inverted;s.reserved=0xd000|((invertedMask>>8)&15);s.onMask=onMask;s.revision=configRevision;return s;
  }
  void off(uint8_t reason=0){
    offCalls++;recordOutput(3,0,reason);
    relayWrite(false);for(int i=0;i<DIMMER_COUNT;i++){writePWM(i,0);duty[i]=0;}onMask=0;
    player.stop();fault=pwmReady?reason:5;
  }
  void tick(bool heartbeatFresh){linkReady=heartbeatFresh;player.tick();} // Link loss holds accepted outputs; never invents a new scene.
  ControlState snapshot()const{
    ControlState s{};for(int i=0;i<PWM_COUNT;i++)s.pwm[i]=duty[i];
    s.ready=linkReady;s.relayOn=relayOn;s.relayMode=relayMode;s.fault=fault;s.audioEnabled=1;
    s.audioState=player.state;s.audioVolume=player.volume;s.audioMode=player.mode;s.audioEq=player.eq;s.audioDrive=player.drive;
    s.audioTrack=player.track;s.audioTracks=player.tracks;
    s.audioAge=player.hasResponse()?(uint16_t)std::min((uint32_t)65534,millis()-player.lastRx):65535;return s;
  }
  Result apply(Op op,uint8_t channel,uint16_t value,bool heartbeatFresh){
    if(!validControl(op,channel,value))return INVALID;
    linkReady=heartbeatFresh;
    if(op==ALL_OFF){off();return APPLIED;}
    if(needsLiveLink(op,value)&&!heartbeatFresh)return LEASE_EXPIRED;
    if(op==PWM||op==PWM_SWITCH){
      uint8_t i=channel-1,v=op==PWM?value:(value?settings.level[i]:0);
      if(!pwmReady||!writePWM(i,v)){off(5);return IO_ERROR;}
      if(op==PWM_SWITCH?value:v>0)onMask|=1<<i;else onMask&=~(1<<i);
      fault=0;return APPLIED;
    }
    if(op==PWM_CONFIG){
      uint8_t i=channel-1,level=value&255;bool inverse=value&256;
      if(settings.level[i]==level&&bool(settings.inverted&(1<<i))==inverse)return APPLIED;
      PwmSettings next=settings;next.level[i]=level;
      if(inverse)next.inverted|=1<<i;else next.inverted&=~(1<<i);
      uint16_t nextInverse=(invertedMask&~(1U<<i))|(inverse?(1U<<i):0);
      if(!saveDimSettings(next,nextInverse))return IO_ERROR;
      uint8_t current=(onMask&(1<<i))&&settings.level[i]!=level?level:duty[i];
      settings=next;invertedMask=nextInverse;configRevision++;
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
    if(op==AUDIO_PAUSE)return player.pause()?APPLIED:IO_ERROR;
    if(!player.bootSettled())return PLAYER_OFFLINE; // One-way UART is usable; reply telemetry remains unknown until a real response.
    bool ok=false;
    switch(op){
      case AUDIO_TRACK:ok=player.playTrack(value);break;
      case AUDIO_RESUME:ok=player.resume();break;
      case AUDIO_NEXT:ok=player.next();break;
      case AUDIO_PREV:ok=player.previous();break;
      case AUDIO_VOLUME:ok=player.setVolume(value);break;
      case AUDIO_MODE:if(value!=1&&value!=2)return INVALID;ok=player.setRepeat(value==1);break;
      case AUDIO_EQ:ok=player.setEq(value);break;
      case AUDIO_DRIVE:if(value!=1)return INVALID;ok=player.selectTF();break;
      default:return INVALID;
    }
    return ok?APPLIED:IO_ERROR;
  }
};

#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "control_types.h"
#include "hardware.h"
#include "player.h"
// Logic-level outputs only. No software can make unknown relay hardware safe.
class Actors {
  uint16_t duty[4]{};
  uint8_t relayMode=0,fault=0;
  bool armed=false,relayOn=false,pwmReady=true;
  Preferences config;
  void relayWrite(bool on) {
    if(!relayMode){pinMode(RELAY_PIN,INPUT);relayOn=false;return;}
    uint8_t level=(on==(relayMode==1))?HIGH:LOW;
    digitalWrite(RELAY_PIN,level);pinMode(RELAY_PIN,OUTPUT);relayOn=on;
  }
public:
  Player player;
  void begin() {
    // Outputs are initialized before Wi-Fi or any potentially blocking startup.
    config.begin("stage-io",false);relayMode=config.getUChar("relayMode",0);
    if(relayMode>2)relayMode=0;
    relayWrite(false);
    for(uint8_t pin:PWM_PINS) {
      digitalWrite(pin,LOW);pinMode(pin,OUTPUT);
      if(!ledcAttach(pin,PWM_HZ,PWM_BITS))pwmReady=false;
      else ledcWrite(pin,0);
    }
    if(!pwmReady)fault=5;
    player.begin();
  }
  void off(uint8_t reason=0) {
    armed=false;relayWrite(false);
    for(int i=0;i<4;i++){ledcWrite(PWM_PINS[i],0);duty[i]=0;}
    player.stop();fault=pwmReady?reason:5;
  }
  bool isArmed()const{return armed;}
  void tick(bool heartbeatFresh) {
    // Called outside packet handlers too: a missing master cannot leave outputs latched on.
    if(armed&&!heartbeatFresh)off(1);
    player.tick();
  }
  ControlState snapshot()const {
    ControlState s{};for(int i=0;i<4;i++)s.pwm[i]=duty[i];
    s.armed=armed;s.relayOn=relayOn;s.relayMode=relayMode;s.fault=fault;
    s.audioEnabled=player.enabled;s.audioState=player.state;s.audioVolume=player.volume;
    s.audioMode=player.mode;s.audioEq=player.eq;s.audioDrive=player.drive;
    s.audioTrack=player.track;s.audioTracks=player.tracks;
    s.audioAge=player.lastRx?(uint16_t)std::min((uint32_t)65534,millis()-player.lastRx):65535;
    return s;
  }
  Result apply(Op op,uint8_t channel,uint16_t value,bool heartbeatFresh) {
    if(!validControl(op,channel,value))return INVALID;
    if(op==DISARM||op==ALL_OFF){off();return APPLIED;}
    if(op==ARM) {
      if(!heartbeatFresh)return LEASE_EXPIRED;
      if(!pwmReady)return IO_ERROR;
      armed=true;fault=0;return APPLIED;
    }
    if(requiresArm(op,value)) {
      if(!heartbeatFresh){off(1);return LEASE_EXPIRED;}
      if(!armed)return NOT_ARMED;
    }
    if(op==PWM) {
      if(!pwmReady||!ledcWrite(PWM_PINS[channel-1],value)){off(5);return IO_ERROR;}
      duty[channel-1]=value;return APPLIED;
    }
    if(op==RELAY){if(!relayMode)return INVALID;relayWrite(value);return APPLIED;}
    if(op==RELAY_CONFIG) {
      if(armed)return NOT_ARMED; // Configure only after DISARM.
      relayWrite(false);relayMode=value;relayWrite(false);config.putUChar("relayMode",relayMode);return APPLIED;
    }
    if(op==AUDIO_ENABLE){player.enabled=value;if(!value)player.stop();return APPLIED;}
    if(op==AUDIO_STOP){player.stop();return APPLIED;}
    if(op==AUDIO_QUERY){player.poll();return APPLIED;}
    if(op==UART_CLEAR){player.clearStats();return APPLIED;}
    if(op==UART_POLL){player.autoPoll=value;return APPLIED;}
    if(op==AUDIO_PAUSE)return player.enqueue(0x03)?APPLIED:IO_ERROR;
    if(!player.enabled)return AUDIO_DISABLED;
    if(!player.ready())return PLAYER_OFFLINE;
    bool ok=false;
    switch(op) {
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

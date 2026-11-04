#pragma once
#include <stdint.h>
constexpr uint8_t PWM_COUNT=8;
enum Op : uint8_t {
  LEGACY_ARM=1,LEGACY_DISARM,PWM,RELAY,RELAY_CONFIG,LEGACY_AUDIO_ENABLE,
  AUDIO_TRACK,AUDIO_RESUME,AUDIO_PAUSE,AUDIO_STOP,AUDIO_NEXT,AUDIO_PREV,
  AUDIO_VOLUME,AUDIO_MODE,AUDIO_EQ,AUDIO_DRIVE,AUDIO_QUERY,ALL_OFF,UART_CLEAR,UART_POLL,PWM_SWITCH,PWM_CONFIG
};
enum Result : uint8_t { APPLIED=0,NOT_READY,INVALID,IO_ERROR,LEGACY_AUDIO_DISABLED,PLAYER_OFFLINE,STALE,LEASE_EXPIRED };
struct __attribute__((packed)) ControlState {
  uint16_t pwm[PWM_COUNT];
  uint8_t ready,relayOn,relayMode,audioEnabled;
  uint8_t audioState,audioVolume,audioMode,audioEq,audioDrive,fault;
  uint16_t audioTrack,audioTracks,audioAge;
};
struct __attribute__((packed)) LegacyState {
  uint16_t pwm[4];
  uint8_t armed,relayOn,relayMode,audioEnabled;
  uint8_t audioState,audioVolume,audioMode,audioEq,audioDrive,fault;
  uint16_t audioTrack,audioTracks,audioAge;
};
static_assert(sizeof(ControlState)==32&&sizeof(LegacyState)==24,"Control state sizes");
inline bool freshDeadline(uint32_t now,uint32_t deadline){int32_t left=(int32_t)(deadline-now);return left>=0&&left<=1000;}
inline bool newSequence(uint32_t value,uint32_t last){return !last||(int32_t)(value-last)>0;}
inline bool needsLiveLink(Op op,uint16_t value){
  return ((op==PWM||op==PWM_SWITCH)&&value>0)||(op==RELAY&&value>0)||op==AUDIO_TRACK||op==AUDIO_RESUME||op==AUDIO_NEXT||op==AUDIO_PREV;
}
inline bool safeDuringOTA(Op op,uint16_t value){return op==ALL_OFF||op==AUDIO_STOP||((op==PWM||op==PWM_SWITCH||op==RELAY)&&value==0);}
inline bool validControl(Op op,uint8_t channel,uint16_t value){
  switch(op){
    case AUDIO_RESUME:case AUDIO_PAUSE:case AUDIO_STOP:case AUDIO_NEXT:case AUDIO_PREV:
    case AUDIO_QUERY:case ALL_OFF:case UART_CLEAR:return channel==0&&value==0;
    case PWM_SWITCH:return channel>=1&&channel<=PWM_COUNT&&value<=1;
    case PWM_CONFIG:return channel>=1&&channel<=PWM_COUNT&&value<=511;
    case PWM:return channel>=1&&channel<=PWM_COUNT&&value<=255;
    case RELAY:case UART_POLL:return channel==0&&value<=1;
    case RELAY_CONFIG:return channel==0&&value<=2;
    case AUDIO_TRACK:return channel==0&&value>=1;
    case AUDIO_VOLUME:return channel==0&&value<=30;
    case AUDIO_MODE:return channel==0&&value<=7;
    case AUDIO_EQ:return channel==0&&value<=4;
    case AUDIO_DRIVE:return channel==0&&value<=1;
    default:return false;
  }
}
inline bool legacyNeedsAudio(Op op){
  return op==AUDIO_TRACK||op==AUDIO_RESUME||op==AUDIO_NEXT||op==AUDIO_PREV||op==AUDIO_VOLUME||op==AUDIO_MODE||op==AUDIO_EQ||op==AUDIO_DRIVE;
}
inline Op legacyPreparation(uint8_t protocol,const ControlState &s,Op op,uint16_t value){
  if(protocol!=3)return (Op)0;
  if(op==RELAY_CONFIG&&s.ready)return LEGACY_DISARM;
  if(needsLiveLink(op,value)&&!s.ready)return LEGACY_ARM;
  if(legacyNeedsAudio(op)&&!s.audioEnabled)return LEGACY_AUDIO_ENABLE;
  return (Op)0;
}
inline ControlState expandLegacy(const LegacyState &old){
  ControlState s{};for(int i=0;i<4;i++)s.pwm[i]=old.pwm[i];
  s.ready=old.armed;s.relayOn=old.relayOn;s.relayMode=old.relayMode;s.audioEnabled=old.audioEnabled;
  s.audioState=old.audioState;s.audioVolume=old.audioVolume;s.audioMode=old.audioMode;s.audioEq=old.audioEq;s.audioDrive=old.audioDrive;s.fault=old.fault;
  s.audioTrack=old.audioTrack;s.audioTracks=old.audioTracks;s.audioAge=old.audioAge;return s;
}
inline LegacyState shrinkState(const ControlState &s){
  LegacyState old{};for(int i=0;i<4;i++)old.pwm[i]=s.pwm[i];
  old.armed=s.ready;old.relayOn=s.relayOn;old.relayMode=s.relayMode;old.audioEnabled=s.audioEnabled;
  old.audioState=s.audioState;old.audioVolume=s.audioVolume;old.audioMode=s.audioMode;old.audioEq=s.audioEq;old.audioDrive=s.audioDrive;old.fault=s.fault;
  old.audioTrack=s.audioTrack;old.audioTracks=s.audioTracks;old.audioAge=s.audioAge;return old;
}

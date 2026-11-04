#pragma once
#include <stdint.h>
// Shared wire types and portable policy checks (also compiled by host tests).
enum Op : uint8_t {
  ARM=1, DISARM, PWM, RELAY, RELAY_CONFIG, AUDIO_ENABLE,
  AUDIO_TRACK, AUDIO_RESUME, AUDIO_PAUSE, AUDIO_STOP, AUDIO_NEXT, AUDIO_PREV,
  AUDIO_VOLUME, AUDIO_MODE, AUDIO_EQ, AUDIO_DRIVE, AUDIO_QUERY, ALL_OFF,
  UART_CLEAR, UART_POLL
};
enum Result : uint8_t { APPLIED=0, NOT_ARMED, INVALID, IO_ERROR, AUDIO_DISABLED, PLAYER_OFFLINE, STALE, LEASE_EXPIRED };
struct __attribute__((packed)) ControlState {
  uint16_t pwm[4];
  uint8_t armed,relayOn,relayMode,audioEnabled;
  uint8_t audioState,audioVolume,audioMode,audioEq,audioDrive,fault;
  uint16_t audioTrack,audioTracks,audioAge;
};
static_assert(sizeof(ControlState)==24,"Control state wire format");
inline bool newSequence(uint32_t value,uint32_t last) {return !last || (int32_t)(value-last)>0;}
inline bool requiresArm(Op op,uint16_t value) {
  return (op==PWM&&value>0)||(op==RELAY&&value>0)||op==AUDIO_TRACK||op==AUDIO_RESUME||op==AUDIO_NEXT||op==AUDIO_PREV;
}
inline bool validControl(Op op,uint8_t channel,uint16_t value) {
  switch(op) {
    case ARM:case DISARM:case AUDIO_RESUME:case AUDIO_PAUSE:case AUDIO_STOP:
    case AUDIO_NEXT:case AUDIO_PREV:case AUDIO_QUERY:case ALL_OFF:case UART_CLEAR:return channel==0&&value==0;
    case PWM:return channel>=1&&channel<=4&&value<=255;
    case RELAY:case AUDIO_ENABLE:case UART_POLL:return channel==0&&value<=1;
    case RELAY_CONFIG:return channel==0&&value<=2;
    case AUDIO_TRACK:return channel==0&&value>=1;
    case AUDIO_VOLUME:return channel==0&&value<=30;
    case AUDIO_MODE:return channel==0&&value<=7;
    case AUDIO_EQ:return channel==0&&value<=4;
    case AUDIO_DRIVE:return channel==0&&value<=1; // USB=0, TF/SD=1; no flash assumed
    default:return false;
  }
}

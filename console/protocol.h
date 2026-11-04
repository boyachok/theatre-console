#pragma once
#include <Arduino.h>
#include <cstring>
#include "quality.h"
#include "control_types.h"
#include "uart_stats.h"
#include "pwm_settings.h"
#ifndef RADIO_MASTER
#define RADIO_MASTER 0
#endif
constexpr char RADIO_VERSION[]="0.7.0";
constexpr uint32_t MAGIC=0x32544753;
constexpr uint8_t PROTOCOL=4,LEGACY_PROTOCOL=3;
constexpr int MAX_NODES=10,WINDOW=120;
constexpr uint32_t POLL_MS=500,TIMEOUT_MS=400,OFFLINE_MS=3000;
enum Type : uint8_t {DISCOVER=1,HELLO,PROBE,ECHO,IDENTIFY,MAINTENANCE,COMMAND,COMMAND_ACK,DEBUG_REQUEST,DEBUG_REPORT,PWM_REPORT,SCORE_STORE,SCORE_RUN,SCORE_ACK};
struct __attribute__((packed)) DebugPacket {
  uint32_t magic;uint8_t protocol,type;uint16_t size;
  uint32_t session,deviceBoot,uptime;uint8_t nodeId,padding[3];UartStats uart;
};
struct __attribute__((packed)) PwmPacket {
  uint32_t magic;uint8_t protocol,type;uint16_t size;
  uint32_t session,deviceBoot,uptime;uint8_t nodeId,padding[3];
  PwmStatus config;uint32_t frequency,reportSeq;uint8_t reserved[16];
};
static_assert(sizeof(PwmPacket)==64,"PWM report size");
struct __attribute__((packed)) Packet {
  uint32_t magic;uint8_t protocol,type;uint16_t size;
  uint32_t session,seq;uint8_t nodeId,health;int8_t rssi;uint8_t reserved;
  uint32_t uptime,deviceBoot,commandId;uint16_t value;uint8_t op,channel,result,padding[3];ControlState state;
};
struct __attribute__((packed)) LegacyPacket {
  uint32_t magic;uint8_t protocol,type;uint16_t size;
  uint32_t session,seq;uint8_t nodeId,health;int8_t rssi;uint8_t reserved;
  uint32_t uptime,deviceBoot,commandId;uint16_t value;uint8_t op,channel,result,padding[3];LegacyState state;
};
static_assert(sizeof(Packet)==72&&sizeof(LegacyPacket)==64&&sizeof(DebugPacket)==100,"Wire sizes");
// Optional HELLO/ECHO telemetry. Older 0.5.0 producers leave this tag zero.
inline void setPWMFrequency(Packet &p,uint32_t hz){
  if(p.type!=HELLO&&p.type!=ECHO)return;
  p.padding[0]='P';p.padding[1]=(hz>>16)&0xff;p.padding[2]=3;p.value=hz&0xffff;
}
inline uint32_t reportedPWMFrequency(const Packet &p){
  if(p.protocol!=PROTOCOL||(p.type!=HELLO&&p.type!=ECHO)||p.padding[0]!='P'||(p.padding[2]<1||p.padding[2]>3))return 0;
  return p.value|((uint32_t)p.padding[1]<<16);
}
inline LegacyPacket legacyPacket(const Packet &p){
  LegacyPacket old{};std::memcpy(&old,&p,40);old.protocol=LEGACY_PROTOCOL;old.size=sizeof(old);old.state=shrinkState(p.state);return old;
}
inline Packet expandPacket(const LegacyPacket &old){
  Packet p{};std::memcpy(&p,&old,40);p.state=expandLegacy(old.state);return p;
}
inline Packet packet(Type type,uint32_t session,uint32_t seq=0){
  Packet p{};p.magic=MAGIC;p.protocol=PROTOCOL;p.size=sizeof(p);p.type=type;p.session=session;p.seq=seq;p.uptime=millis();return p;
}

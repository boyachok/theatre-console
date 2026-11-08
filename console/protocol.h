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
#include "release.h"
constexpr char RADIO_VERSION[]=THEATRE_VERSION;
constexpr uint32_t MAGIC=0x32544753;
constexpr uint8_t PROTOCOL=4,LEGACY_PROTOCOL=3;
constexpr int MAX_NODES=10,WINDOW=120;
constexpr uint32_t POLL_MS=500,TIMEOUT_MS=400,OFFLINE_MS=3000;
enum Type : uint8_t {DISCOVER=1,HELLO,PROBE,ECHO,IDENTIFY,MAINTENANCE,COMMAND,COMMAND_ACK,DEBUG_REQUEST,DEBUG_REPORT,PWM_REPORT,SCORE_STORE,SCORE_RUN,SCORE_ACK,REMOTE_HELLO,REMOTE_PING,REMOTE_PONG,REMOTE_EVENT,REMOTE_ACK};
struct __attribute__((packed)) DebugPacket {
  uint32_t magic;uint8_t protocol,type;uint16_t size;
  uint32_t session,deviceBoot,uptime;uint8_t nodeId,padding[3];UartStats uart;
};
// Optional extended UART report; old 100B reports remain accepted by a new master.
struct __attribute__((packed)) ExtendedDebugPacket {DebugPacket legacy;PlayerDebug player;};
static_assert(sizeof(ExtendedDebugPacket)==116,"Extended UART report size");
inline bool decodeDebugReport(const uint8_t *data,size_t len,DebugPacket &base,PlayerDebug &extra){
 if(len!=sizeof(DebugPacket)&&len!=sizeof(ExtendedDebugPacket))return false;
 memcpy(&base,data,sizeof(base));extra=PlayerDebug{};
 if(base.magic!=MAGIC||(base.protocol!=PROTOCOL&&base.protocol!=LEGACY_PROTOCOL)||base.size!=len||base.type!=DEBUG_REPORT)return false;
 if(len==sizeof(ExtendedDebugPacket)){ExtendedDebugPacket full;memcpy(&full,data,sizeof(full));extra=full.player;}
 return true;
}
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
  p.padding[0]='P';p.padding[1]=(hz>>16)&0xff;p.padding[2]=8;p.value=hz&0xffff;
}
// P/8 regular and P/9 helmet support score8 animations; P/6,P/7 are score7.
// Service layout stays 72B. Never advertise score7 peers as score8-ready.
inline bool helmetTag(const Packet &p){return p.protocol==PROTOCOL&&p.padding[0]=='P'&&(p.padding[2]==5||p.padding[2]==7||p.padding[2]==9);}
inline bool localScoreTag(const Packet &p){return p.protocol==PROTOCOL&&p.padding[0]=='P'&&(p.padding[2]==8||p.padding[2]==9);}
inline uint32_t reportedPWMFrequency(const Packet &p){
  if(p.protocol!=PROTOCOL||(p.type!=HELLO&&p.type!=ECHO)||p.padding[0]!='P'||(p.padding[2]<1||p.padding[2]>9)||helmetTag(p))return 0;
  return p.value|((uint32_t)p.padding[1]<<16);
}
inline LegacyPacket legacyPacket(const Packet &p){
  LegacyPacket old{};std::memcpy(&old,&p,40);old.protocol=LEGACY_PROTOCOL;old.size=sizeof(old);old.state=shrinkState(p.state);return old;
}
inline Packet expandPacket(const LegacyPacket &old){
  Packet p{};std::memcpy(&p,&old,40);p.state=expandLegacy(old.state);return p;
}
inline bool remoteType(uint8_t type){return type>=REMOTE_HELLO&&type<=REMOTE_ACK;}
inline bool remoteTag(const Packet &p){return p.padding[0]=='R'&&p.padding[1]==1;}
inline Packet packet(Type type,uint32_t session,uint32_t seq=0){
  Packet p{};p.magic=MAGIC;p.protocol=PROTOCOL;p.size=sizeof(p);p.type=type;p.session=session;p.seq=seq;p.uptime=millis();return p;
}

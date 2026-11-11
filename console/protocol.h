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
enum Type : uint8_t {DISCOVER=1,HELLO,PROBE,ECHO,IDENTIFY,MAINTENANCE,COMMAND,COMMAND_ACK,DEBUG_REQUEST,DEBUG_REPORT,PWM_REPORT,SCORE_STORE,SCORE_RUN,SCORE_ACK,REMOTE_HELLO,REMOTE_PING,REMOTE_PONG,REMOTE_EVENT,REMOTE_ACK,FIRMWARE_REPORT};
enum FirmwareRole:uint8_t { FW_SLAVE=1,FW_REMOTE=2,FW_HELMET=3 };
struct __attribute__((packed)) FirmwarePacket {
 uint32_t magic;uint8_t protocol,type;uint16_t size;uint32_t session,deviceBoot,uptime;uint8_t role,reserved[3];char version[24];uint8_t padding[16];
};
static_assert(sizeof(FirmwarePacket)==64,"Optional firmware report");
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
inline void requestFirmware(Packet &p){p.padding[0]='F';p.padding[1]='W';p.padding[2]=1;}
inline bool firmwareRequested(const Packet &p){return p.padding[0]=='F'&&p.padding[1]=='W'&&p.padding[2]==1;}
inline FirmwarePacket firmwareReport(uint32_t session,uint32_t boot,FirmwareRole role){FirmwarePacket p{};p.magic=MAGIC;p.protocol=PROTOCOL;p.type=FIRMWARE_REPORT;p.size=sizeof(p);p.session=session;p.deviceBoot=boot;p.uptime=millis();p.role=role;static_assert(sizeof(THEATRE_VERSION)<=24,"Firmware name too long");memcpy(p.version,THEATRE_VERSION,sizeof(THEATRE_VERSION));return p;}
inline bool validFirmware(const FirmwarePacket &p){
 if(p.magic!=MAGIC||p.protocol!=PROTOCOL||p.type!=FIRMWARE_REPORT||p.size!=sizeof(p)||!p.deviceBoot||p.role<FW_SLAVE||p.role>FW_HELMET||p.version[0]<'0'||p.version[0]>'9')return false;
 for(size_t i=0;i<sizeof(p.version);i++){char c=p.version[i];if(!c)return true;if(!((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='.'||c=='-'||c=='+'||c=='_'))return false;}return false;
}
// Optional HELLO/ECHO telemetry. Older 0.5.0 producers leave this tag zero.
inline void setPWMFrequency(Packet &p,uint32_t hz){
  if(p.type!=HELLO&&p.type!=ECHO)return;
  p.padding[0]='P';p.padding[1]=(hz>>16)&0xff;p.padding[2]=14;p.value=hz&0xffff;
}
// P/14 regular and P/15 helmet support score11 polarity/held-light delays/continuous audio.
// Service layout stays 72B. Older peers must never ignore new timed or mains outputs.
inline bool helmetTag(const Packet &p){return p.protocol==PROTOCOL&&p.padding[0]=='P'&&(p.padding[2]==5||p.padding[2]==7||p.padding[2]==9||p.padding[2]==11||p.padding[2]==13||p.padding[2]==15);}
inline bool localScoreTag(const Packet &p){return p.protocol==PROTOCOL&&p.padding[0]=='P'&&(p.padding[2]==14||p.padding[2]==15);}
inline uint32_t reportedPWMFrequency(const Packet &p){
  if(p.protocol!=PROTOCOL||(p.type!=HELLO&&p.type!=ECHO)||p.padding[0]!='P'||(p.padding[2]<1||p.padding[2]>15)||helmetTag(p))return 0;
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

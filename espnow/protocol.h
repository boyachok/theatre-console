#pragma once
#include <Arduino.h>
#include "quality.h"
#include "control_types.h"
#include "uart_stats.h"
#ifndef RADIO_MASTER
#define RADIO_MASTER 0
#endif
#ifndef RADIO_CHANNEL
#define RADIO_CHANNEL 6
#endif
static_assert(RADIO_CHANNEL>=1&&RADIO_CHANNEL<=11,"Use channel 1..11");
constexpr char RADIO_VERSION[]="0.3.1";
constexpr uint32_t MAGIC=0x32544753;
constexpr uint8_t PROTOCOL=3;
constexpr int MAX_NODES=10,WINDOW=120;
constexpr uint32_t POLL_MS=500,TIMEOUT_MS=400,OFFLINE_MS=3000;
enum Type : uint8_t { DISCOVER=1,HELLO,PROBE,ECHO,IDENTIFY,MAINTENANCE,COMMAND,COMMAND_ACK,DEBUG_REQUEST,DEBUG_REPORT };
struct __attribute__((packed)) DebugPacket {
  uint32_t magic;
  uint8_t protocol,type;
  uint16_t size;
  uint32_t session,deviceBoot,uptime;
  uint8_t nodeId,padding[3];
  UartStats uart;
};
static_assert(sizeof(DebugPacket)==100,"UART report wire size");
struct __attribute__((packed)) Packet {
  uint32_t magic;
  uint8_t protocol,type;
  uint16_t size;
  uint32_t session,seq;
  uint8_t nodeId,health;
  int8_t rssi;
  uint8_t reserved;
  uint32_t uptime;
  uint32_t deviceBoot,commandId;
  uint16_t value;
  uint8_t op,channel,result,padding[3];
  ControlState state;
};
static_assert(sizeof(Packet)==64,"Wire format must stay fixed");
inline Packet packet(Type type,uint32_t session,uint32_t seq=0) {
  Packet p{};p.magic=MAGIC;p.protocol=PROTOCOL;p.size=sizeof(p);
  p.type=type;p.session=session;p.seq=seq;p.uptime=millis();return p;
}

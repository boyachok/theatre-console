#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <algorithm>
namespace Show {
constexpr uint32_t MAGIC=0x32574f48;
constexpr uint8_t PROTOCOL=7,STORE=12,RUN=13,ACK=14,PREVIEW=15;
constexpr uint32_t REPEAT_MS=200,IDLE_MS=500;
constexpr uint8_t ACTIVE=1,LOADED=2,MISSING_PLAN=4;
// Compact published-scene reference. No output values travel in RUN.
struct __attribute__((packed)) RunPacket {
 uint32_t magic;uint8_t protocol,type;uint16_t size;
 uint32_t session,deviceBoot,run,transition,version,checksum;
 uint16_t step,total;uint8_t reason,health;int8_t rssi;uint8_t reserved;uint32_t sentAt;
};
constexpr uint16_t MAX_STEPS=512;
struct __attribute__((packed)) Dimmer {uint8_t value;uint32_t inMs,outMs;};
struct __attribute__((packed)) Relay {uint8_t on,mode;uint32_t onMs,offMs;};
struct __attribute__((packed)) Audio {uint16_t track;uint8_t volume,enabled;uint32_t inMs,outMs;};
struct __attribute__((packed)) Strip {uint8_t enabled,action,mode,r,g,b,br,bg,bb,brightness,width,direction,order,slow;uint16_t count;uint32_t duration,program;};
struct __attribute__((packed)) Cue {uint16_t dimmerMask;uint8_t relayMask;Dimmer dimmers[12];Relay relays[2];Audio audio;Strip strip;uint8_t reserved[3];};
struct __attribute__((packed)) Packet {
 uint32_t magic;uint8_t protocol,type;uint16_t size;
 uint32_t session,deviceBoot,run,transition,version,checksum;
 uint16_t step,total;uint8_t reason,health;int8_t rssi;uint8_t reserved;uint32_t sentAt;Cue cue;
};
struct __attribute__((packed)) FileHeader {uint32_t magic,version,checksum;uint16_t total,reserved;};
static_assert(sizeof(Cue)==170&&sizeof(Packet)==214&&sizeof(RunPacket)==44&&sizeof(FileHeader)==16,"Show wire sizes");
static_assert(offsetof(Packet,cue)==sizeof(RunPacket),"Shared scene header prefix");
inline RunPacket reference(const Packet &p){RunPacket r{};std::memcpy(&r,&p,sizeof(r));r.size=sizeof(r);return r;}
inline Packet expand(const RunPacket &r){Packet p{};std::memcpy(&p,&r,sizeof(r));return p;}
inline uint32_t crc32(const void *source,size_t count){
 const uint8_t *bytes=(const uint8_t*)source;uint32_t crc=0xffffffff;
 while(count--){crc^=*bytes++;for(int b=0;b<8;b++)crc=(crc>>1)^((crc&1)?0xedb88320:0);}return ~crc;
}
inline bool valid(const Cue &c){
 if(c.relayMask&~3||c.dimmerMask&~4095)return false;
 for(int i=0;i<12;i++)if(c.dimmers[i].inMs>3600000||c.dimmers[i].outMs>3600000)return false;
 for(int i=0;i<2;i++)if(c.relays[i].on>1||c.relays[i].mode>2||c.relays[i].onMs>3600000||c.relays[i].offMs>3600000)return false;
 return c.audio.volume<=30&&c.audio.enabled<=1&&c.audio.inMs<=3600000&&c.audio.outMs<=3600000&&c.strip.enabled<=1&&c.strip.action<=2&&c.strip.mode<=2&&c.strip.order<=1&&c.strip.slow<=1&&c.strip.count<=1024&&c.strip.brightness<=100&&c.strip.width<=100&&c.strip.direction<=1&&c.strip.duration<=3600000&&(!(c.strip.enabled&&c.strip.action==2)||(c.strip.count>0&&c.strip.duration>0&&c.strip.width>0));
}
struct Ramp {float from=0,to=0;uint32_t at=0,duration=0;
 float value(uint32_t now)const {return !duration?to:from+(to-from)*std::min(1.f,(float)(uint32_t)(now-at)/duration);}
 void target(float v,uint32_t up,uint32_t down,uint32_t now){float start=value(now);duration=v>=start?up:down;from=start;to=v;at=now;}
};
inline bool newer(uint32_t next,uint32_t last){return !last||(int32_t)(next-last)>0;}
}

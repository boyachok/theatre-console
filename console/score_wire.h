#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <algorithm>
#include "helmet_voice_wire.h"
namespace Show {
constexpr uint32_t MAGIC=0x32574f48;
constexpr uint8_t PROTOCOL=12,STORE=12,RUN=13,ACK=14,PREVIEW=15;
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
enum DimmerMode:uint8_t { STATIC=0,SINE=1,SQUARE=2,RAMP=3,RANDOM=4 };
struct __attribute__((packed)) DimmerAnimation {uint8_t mode;uint32_t periodMs,phaseSeed;uint8_t floor,duty,smooth;};
// Preserve the exact score7 static prefix for non-destructive local-cache migration.
struct __attribute__((packed)) LegacyCue {uint16_t dimmerMask;uint8_t relayMask;Dimmer dimmers[12];Relay relays[2];Audio audio;Strip strip;uint8_t reserved[3];};
struct __attribute__((packed)) PriorCue {uint16_t dimmerMask;uint8_t relayMask;Dimmer dimmers[12];Relay relays[2];Audio audio;Strip strip;uint8_t reserved[3];DimmerAnimation animations[12];};
struct __attribute__((packed)) AcCue {uint8_t enabled;Dimmer dimmer;uint32_t delayMs;DimmerAnimation animation;};
struct __attribute__((packed)) Score10Cue {uint16_t dimmerMask;uint8_t relayMask;Dimmer dimmers[12];Relay relays[2];Audio audio;Strip strip;uint8_t reserved[3];DimmerAnimation animations[12];uint32_t lightDelayMs[12];uint8_t audioLoop;AcCue ac;};
struct __attribute__((packed)) Score11Cue {uint16_t dimmerMask;uint8_t relayMask;Dimmer dimmers[12];Relay relays[2];Audio audio;Strip strip;uint8_t reserved[3];DimmerAnimation animations[12];uint32_t lightDelayMs[12];uint8_t audioLoop;AcCue ac;uint16_t invertedMask,inversionSet;};
struct __attribute__((packed)) Cue {uint16_t dimmerMask;uint8_t relayMask;Dimmer dimmers[12];Relay relays[2];Audio audio;Strip strip;uint8_t reserved[3];DimmerAnimation animations[12];uint32_t lightDelayMs[12];uint8_t audioLoop;AcCue ac;uint16_t invertedMask,inversionSet;StripOptions stripOptions;VoiceProfile voice;};
struct __attribute__((packed)) Packet {
 uint32_t magic;uint8_t protocol,type;uint16_t size;
 uint32_t session,deviceBoot,run,transition,version,checksum;
 uint16_t step,total;uint8_t reason,health;int8_t rssi;uint8_t reserved;uint32_t sentAt;Cue cue;
};
struct __attribute__((packed)) FileHeader {uint32_t magic,version,checksum;uint16_t total,reserved;};
static_assert(sizeof(LegacyCue)==170&&sizeof(DimmerAnimation)==12&&offsetof(Cue,animations)==170,"Animation extension layout");
static_assert(sizeof(PriorCue)==314&&offsetof(Cue,lightDelayMs)==314&&sizeof(AcCue)==26,"score9 prefix");
static_assert(sizeof(Cue)==468&&sizeof(Packet)==512&&sizeof(RunPacket)==44&&sizeof(FileHeader)==16,"Show wire sizes");
static_assert(sizeof(Score10Cue)==389&&offsetof(Cue,invertedMask)==389,"score10 prefix");
static_assert(sizeof(Score11Cue)==393&&offsetof(Cue,stripOptions)==393,"score11 migration prefix");
static_assert(offsetof(Packet,cue)==sizeof(RunPacket),"Shared scene header prefix");
inline RunPacket reference(const Packet &p){RunPacket r{};std::memcpy(&r,&p,sizeof(r));r.size=sizeof(r);return r;}
inline Packet expand(const RunPacket &r){Packet p{};std::memcpy(&p,&r,sizeof(r));return p;}
inline uint32_t crc32(const void *source,size_t count){
 const uint8_t *bytes=(const uint8_t*)source;uint32_t crc=0xffffffff;
 while(count--){crc^=*bytes++;for(int b=0;b<8;b++)crc=(crc>>1)^((crc&1)?0xedb88320:0);}return ~crc;
}
// Reserved 24-bit little-endian field becomes delayMs in score9. No packet growth.
inline uint32_t audioDelay(const Cue &c){return c.reserved[0]|((uint32_t)c.reserved[1]<<8)|((uint32_t)c.reserved[2]<<16);}
inline void setAudioDelay(Cue &c,uint32_t ms){c.reserved[0]=ms&255;c.reserved[1]=(ms>>8)&255;c.reserved[2]=(ms>>16)&255;}
inline bool validAnimation(const DimmerAnimation &a){
 if(a.mode>RANDOM||a.floor>100||a.duty>100||a.smooth>100)return false;
 return a.mode==STATIC?!(a.periodMs||a.phaseSeed||a.floor||a.duty||a.smooth):a.periodMs>=20&&a.periodMs<=3600000&&(a.mode==RANDOM||a.phaseSeed<3600);
}
inline bool valid(const Cue &c){
 if(audioDelay(c)>3600000||c.audioLoop>1||!validVoice(c.voice)||!validStripOptions(c.stripOptions))return false;
 if(c.ac.enabled>1||c.ac.delayMs>3600000||c.ac.dimmer.inMs>3600000||c.ac.dimmer.outMs>3600000||!validAnimation(c.ac.animation))return false;
 if(c.ac.animation.mode&&c.ac.animation.periodMs<40)return false;
 if(c.relayMask&~3||c.dimmerMask&~4095||c.invertedMask&~c.inversionSet||c.inversionSet&~4095)return false;
 for(int i=0;i<12;i++){
  if(c.dimmers[i].inMs>3600000||c.dimmers[i].outMs>3600000)return false;
  if(!validAnimation(c.animations[i])||c.lightDelayMs[i]>3600000)return false;
 }
 for(int i=0;i<2;i++)if(c.relays[i].on>1||c.relays[i].mode>2||c.relays[i].onMs>3600000||c.relays[i].offMs>3600000)return false;
 return c.audio.volume<=30&&c.audio.enabled<=1&&c.audio.inMs<=3600000&&c.audio.outMs<=3600000&&c.strip.enabled<=1&&c.strip.action<=2&&c.strip.mode<=6&&c.strip.order<=1&&c.strip.slow<=1&&c.strip.count<=1024&&c.strip.brightness<=100&&c.strip.width<=100&&c.strip.direction<=1&&c.strip.duration<=3600000&&(!(c.strip.enabled&&c.strip.action==2)||(c.strip.count>0&&c.strip.duration>0&&c.strip.width>0));
}
struct Ramp {float from=0,to=0;uint32_t at=0,duration=0;
 float value(uint32_t now)const {return !duration?to:from+(to-from)*std::min(1.f,(float)(uint32_t)(now-at)/duration);}
 void target(float v,uint32_t up,uint32_t down,uint32_t now){float start=value(now);duration=v>=start?up:down;from=start;to=v;at=now;}
};
inline bool newer(uint32_t next,uint32_t last){return !last||(int32_t)(next-last)>0;}
}

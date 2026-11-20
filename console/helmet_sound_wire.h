#pragma once
#include "helmet_voice_wire.h"
namespace HelmetSound {
constexpr uint32_t MAGIC=0x31475648;constexpr uint8_t VERSION=1,CONFIG=1;
struct __attribute__((packed)) Profile {Show::VoiceProfile voice{};Show::VoiceTone tone{};};
struct __attribute__((packed)) Packet {uint32_t magic=MAGIC;uint8_t protocol=VERSION,type=CONFIG;uint16_t size=sizeof(Packet);uint32_t session=0,deviceBoot=0,revision=0,checksum=0;Profile profile{};};
static_assert(sizeof(Profile)==121&&sizeof(Packet)==145,"Independent global voice, existing 512B scenes unchanged");
inline uint32_t hash(const Profile &p){uint32_t c=0xffffffffU;const auto *b=(const uint8_t*)&p;for(size_t i=0;i<sizeof(p);i++){c^=b[i];for(int j=0;j<8;j++)c=(c>>1)^(0xedb88320U&-(c&1));}return ~c;}
inline bool valid(const Profile &p){return p.voice.configured==1&&Show::validVoice(p.voice)&&Show::validTone(p.tone);}
inline bool valid(const Packet &p){return p.magic==MAGIC&&p.protocol==VERSION&&p.type==CONFIG&&p.size==sizeof(p)&&p.session&&p.deviceBoot&&p.revision&&valid(p.profile)&&p.checksum==hash(p.profile);}
}

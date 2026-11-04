#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
constexpr uint32_t LAN_MAGIC=0x34475453;
constexpr uint8_t LAN_VERSION=1;
constexpr uint16_t LAN_PORT=4210;
constexpr size_t LAN_MAX_PAYLOAD=256;
struct __attribute__((packed)) LanHeader {
  uint32_t magic;
  uint8_t version,reserved;
  uint16_t length;
  uint8_t mac[6],destination[6];
};
static_assert(sizeof(LanHeader)==20,"LAN envelope size");
inline bool broadcastMAC(const uint8_t *mac){for(int i=0;i<6;i++)if(mac[i]!=255)return false;return true;}
inline bool validSourceMAC(const uint8_t *mac) {
  uint8_t any=0;for(int i=0;i<6;i++)any|=mac[i];return any&&!(mac[0]&1);
}
inline size_t encodeLAN(const uint8_t *mac,const uint8_t *destination,const uint8_t *payload,size_t length,uint8_t *out) {
  if(!validSourceMAC(mac)||(!validSourceMAC(destination)&&!broadcastMAC(destination))||(length!=64&&length!=72&&length!=100&&length!=214))return 0;
  LanHeader h{};h.magic=LAN_MAGIC;h.version=LAN_VERSION;h.length=length;std::memcpy(h.mac,mac,6);std::memcpy(h.destination,destination,6);
  std::memcpy(out,&h,sizeof(h));std::memcpy(out+sizeof(h),payload,length);return sizeof(h)+length;
}
inline bool decodeLAN(const uint8_t *data,size_t length,LanHeader &h) {
  if(length<sizeof(h)||length>sizeof(h)+LAN_MAX_PAYLOAD)return false;
  std::memcpy(&h,data,sizeof(h));
  return h.magic==LAN_MAGIC&&h.version==LAN_VERSION&&!h.reserved&&validSourceMAC(h.mac)&&(validSourceMAC(h.destination)||broadcastMAC(h.destination))&&
         (h.length==64||h.length==72||h.length==100||h.length==214)&&length==sizeof(h)+h.length;
}
inline bool addressedTo(const LanHeader &h,const uint8_t *mac){return broadcastMAC(h.destination)||!std::memcmp(h.destination,mac,6);}
struct LanPeer {uint8_t mac[6]{};uint32_t ip=0,lastSeen=0;bool used=false;};
class LanPeers {
  LanPeer peers[16];
public:
  void clear(){for(auto &p:peers)p=LanPeer{};}
  uint32_t lookup(const uint8_t *mac)const {for(const auto &p:peers)if(p.used&&!std::memcmp(p.mac,mac,6))return p.ip;return 0;}
  bool observe(const uint8_t *mac,uint32_t ip,uint32_t now) {
    if(!ip||!validSourceMAC(mac))return false;
    for(auto &p:peers)if(p.used&&!std::memcmp(p.mac,mac,6)){p.ip=ip;p.lastSeen=now;return true;}
    LanPeer *slot=nullptr;
    for(auto &p:peers)if(!p.used){slot=&p;break;}
    if(!slot){slot=&peers[0];for(auto &p:peers)if((uint32_t)(now-p.lastSeen)>(uint32_t)(now-slot->lastSeen))slot=&p;}
    *slot=LanPeer{};slot->used=true;slot->ip=ip;slot->lastSeen=now;std::memcpy(slot->mac,mac,6);return true;
  }
};

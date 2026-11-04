#pragma once
#include "WiFi.h"
struct FakeDatagram {IPAddress ip;uint16_t port;std::vector<uint8_t> data;};
inline std::deque<FakeDatagram> mockUDPIn;
inline std::vector<FakeDatagram> mockUDPOut;
inline bool mockBindOK=true,mockBeginPacketOK=true,mockEndPacketOK=true;
inline size_t mockUDPWriteLimit=1000;
inline int mockBinds=0,mockStops=0;
class WiFiUDP {
  FakeDatagram incoming,outgoing;bool buffered=false;
public:
  bool begin(uint16_t){mockBinds++;return mockBindOK;}
  void stop(){mockStops++;buffered=false;mockUDPIn.clear();}
  bool beginPacket(IPAddress ip,uint16_t port){outgoing={ip,port,{}};return mockBeginPacketOK;}
  size_t write(const uint8_t *p,size_t n){n=std::min(n,mockUDPWriteLimit);outgoing.data.assign(p,p+n);return n;}
  bool endPacket(){if(mockEndPacketOK)mockUDPOut.push_back(outgoing);return mockEndPacketOK;}
  int parsePacket(){if(buffered||mockUDPIn.empty())return 0;incoming=mockUDPIn.front();mockUDPIn.pop_front();buffered=true;return incoming.data.size();}
  int read(uint8_t *out,size_t n){n=std::min(n,incoming.data.size());std::copy(incoming.data.begin(),incoming.data.begin()+n,out);buffered=false;return n;}
  void clear(){buffered=false;}
  IPAddress remoteIP()const{return incoming.ip;}
  uint16_t remotePort()const{return incoming.port;}
};

#include <cassert>
#include <cstdio>
#include "../control/wifi_transport.h"
int main(){
  mockNow=100;WifiTransport net;net.begin(true);
  assert(WiFi.ssid=="YOUR_WIFI_SSID"&&WiFi.password=="YOUR_WIFI_PASSWORD");
  assert(WiFi.STA.begins==1&&validSourceMAC(net.mac));
  assert(WiFi.host==net.hostname.c_str());
  assert(!memcmp(net.mac,WiFi.ownMac,6));
  assert(WiFi.currentMode==WIFI_STA&&!WiFi.sleep&&WiFi.autoReconnect&&!WiFi.persistentValue);
  assert(!net.ready()&&net.generation==0);
  WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,10,50);mockNow=1000;net.tick();
  assert(net.ready()&&net.generation==1&&MDNS.starts==1);
  assert(net.broadcastIP()==IPAddress(192,168,10,255));
  WiFi.mask=IPAddress(255,255,254,0);WiFi.address=IPAddress(192,168,10,50);
  assert(net.broadcastIP()==IPAddress(192,168,11,255));WiFi.mask=IPAddress(255,255,255,0);
  const uint8_t peer[]={0xA0,6,7,8,9,10},other[]={0xA0,9,9,9,9,9},all[]={255,255,255,255,255,255};
  uint8_t body[100]{},wire[120]{};LanMessage msg{};
  size_t len=encodeLAN(peer,net.mac,body,64,wire);
  mockUDPIn.push_back({IPAddress(192,168,10,60),LAN_PORT,std::vector<uint8_t>(wire,wire+len)});
  assert(net.receive(msg)&&msg.length==64&&msg.rssi==-55&&msg.atUs==1000000);
  assert(!memcmp(msg.mac,peer,6)&&net.peerIP(peer)==IPAddress(192,168,10,60));
  assert(net.send(peer,body,64)&&mockUDPOut.back().ip==IPAddress(192,168,10,60));
  LanHeader h{};assert(decodeLAN(mockUDPOut.back().data.data(),mockUDPOut.back().data.size(),h)&&addressedTo(h,peer));
  assert(net.send(all,body,64)&&mockUDPOut.back().ip==IPAddress(192,168,10,255));
  // DHCP changes peer IP, preserving its stable MAC identity.
  mockUDPIn.push_back({IPAddress(192,168,10,99),LAN_PORT,std::vector<uint8_t>(wire,wire+len)});
  assert(net.receive(msg)&&net.peerIP(peer)==IPAddress(192,168,10,99));
  // A packet sent to another MAC at our current IP must be ignored.
  len=encodeLAN(peer,other,body,64,wire);
  mockUDPIn.push_back({IPAddress(192,168,10,61),LAN_PORT,std::vector<uint8_t>(wire,wire+len)});
  assert(!net.receive(msg)&&net.peerIP(peer)==IPAddress(192,168,10,99));
  mockUDPIn.push_back({IPAddress(192,168,10,60),LAN_PORT,std::vector<uint8_t>(121,0)});
  assert(!net.receive(msg)&&net.rxDrops==1);
  mockUDPIn.push_back({IPAddress(192,168,10,60),LAN_PORT,{1,2,3}});
  assert(!net.receive(msg)&&net.rxInvalid==1);
  len=encodeLAN(peer,net.mac,body,64,wire);
  mockUDPIn.push_back({IPAddress(192,168,10,60),1234,std::vector<uint8_t>(wire,wire+len)});
  assert(!net.receive(msg)&&net.rxInvalid==2);
  mockUDPWriteLimit=1;assert(!net.send(peer,body,64)&&net.txErrors==1);mockUDPWriteLimit=1000;
  WiFi.state=0;assert(!net.ready());net.tick();
  assert(net.generation==2&&!net.ready()&&(uint32_t)net.peerIP(peer)==0&&MDNS.stops==1);
  uint32_t errors=net.txErrors;assert(!net.send(peer,body,64)&&net.txErrors==errors);
  mockNow=11000;net.tick();assert(WiFi.reconnects==1);
  WiFi.state=WL_CONNECTED;mockNow=12000;net.tick();assert(net.ready()&&net.generation==3);
  WiFi.address=IPAddress(192,168,10,70);mockNow=14000;net.tick();
  assert(net.ready()&&net.generation==5&&MDNS.starts==3); // Close old socket, bind new DHCP IP.
  WiFi.state=0;mockNow=15000;net.tick();assert(net.generation==6);
  WiFi.state=WL_CONNECTED;mockBindOK=false;mockNow=16000;net.tick();
  assert(!net.ready()&&net.generation==6);int attempts=mockBinds;
  mockNow=16100;net.tick();assert(mockBinds==attempts);
  mockBindOK=true;mockNow=17100;net.tick();assert(net.ready()&&net.generation==7);
  len=encodeLAN(peer,net.mac,body,72,wire);
  mockUDPIn.push_back({IPAddress(192,168,10,99),LAN_PORT,std::vector<uint8_t>(wire,wire+len)});
  assert(net.receive(msg)&&msg.length==72);
  uint32_t gen=net.generation;net.clearPeers();assert(net.ready()&&net.generation==gen&&(uint32_t)net.peerIP(peer)==0);
  puts("Wi-Fi join, discovery routing, RSSI, DHCP, reconnect generations and UDP error checks passed");
}

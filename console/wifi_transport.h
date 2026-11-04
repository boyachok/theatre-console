#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <esp_timer.h>
#include <esp_mac.h>
#include <atomic>
#include "network_config.h"
#include "lan_wire.h"
struct LanMessage {uint8_t mac[6],payload[LAN_MAX_PAYLOAD];uint16_t length;int8_t rssi;uint32_t atUs;};
class WifiTransport {
  WiFiUDP udp;
  LanPeers peers;
  IPAddress bound;
  bool active=false,mdns=false,webService=true;
  uint32_t lastRetry=0,lastBind=0;
  void closeLink() {
    udp.stop();if(mdns)MDNS.end();mdns=false;active=false;bound=IPAddress();peers.clear();generation++;
  }
public:
  uint8_t mac[6]{};
  String hostname;
  uint32_t generation=0,txErrors=0,rxInvalid=0;
  std::atomic<uint32_t> rxDrops{0};
  void begin(bool master,const char *prefix=nullptr,bool advertiseWeb=true) {
    webService=advertiseWeb;
    WiFi.persistent(false);WiFi.mode(WIFI_STA);
    // Arduino 3.x creates the STA netif asynchronously. Wait before reading its MAC
    // or setting hostname; WiFi.mode() alone can still leave the netif null.
    WiFi.STA.begin(false);WiFi.setAutoReconnect(true);WiFi.setSleep(false);
    WiFi.disconnect(false,false);
    if(!WiFi.macAddress(mac)||!validSourceMAC(mac))esp_read_mac(mac,ESP_MAC_WIFI_STA);
    char suffix[7];snprintf(suffix,sizeof(suffix),"%02x%02x%02x",mac[3],mac[4],mac[5]);
    hostname=String(prefix?prefix:(master?"stage-master-":"stage-slave-"))+suffix;
    WiFi.setHostname(hostname.c_str());WiFi.begin(STAGE_WIFI_SSID,STAGE_WIFI_PASSWORD);lastRetry=millis();
  }
  void clearPeers(){peers.clear();udp.clear();}
  bool ready()const{return active&&WiFi.status()==WL_CONNECTED&&(uint32_t)WiFi.localIP()!=0;}
  IPAddress peerIP(const uint8_t *peer)const{return IPAddress(peers.lookup(peer));}
  IPAddress broadcastIP()const {
    IPAddress ip=WiFi.localIP(),mask=WiFi.subnetMask();
    return IPAddress(ip[0]|(uint8_t)~mask[0],ip[1]|(uint8_t)~mask[1],ip[2]|(uint8_t)~mask[2],ip[3]|(uint8_t)~mask[3]);
  }
  void tick() {
    uint32_t now=millis();
    if(WiFi.status()!=WL_CONNECTED||(uint32_t)WiFi.localIP()==0) {
      if(active)closeLink();
      if(now-lastRetry>=10000){lastRetry=now;WiFi.reconnect();}
      return;
    }
    if(active&&bound==WiFi.localIP())return;
    if(active)closeLink();
    if(lastBind&&now-lastBind<1000)return;
    lastBind=now;
    if(!udp.begin(LAN_PORT)){txErrors++;return;}
    bound=WiFi.localIP();active=true;peers.clear();generation++;
    mdns=MDNS.begin(hostname.c_str());if(mdns&&webService)MDNS.addService("http","tcp",80);
    if(webService)Serial.printf("LAN http://%s  http://%s.local  SSID=%s  channel=%d  UDP=%u\n",bound.toString().c_str(),hostname.c_str(),STAGE_WIFI_SSID,WiFi.channel(),LAN_PORT);
    else Serial.printf("LAN role=remote IP=%s host=%s.local SSID=%s channel=%d UDP=%u (no HTTP server)\n",bound.toString().c_str(),hostname.c_str(),STAGE_WIFI_SSID,WiFi.channel(),LAN_PORT);
  }
  bool send(const uint8_t *destination,const uint8_t *data,size_t length) {
    if(!ready())return false;
    IPAddress ip=broadcastMAC(destination)?broadcastIP():peerIP(destination);
    if((uint32_t)ip==0)return false; // Not discovered yet, not a driver failure.
    uint8_t wire[sizeof(LanHeader)+LAN_MAX_PAYLOAD];size_t n=encodeLAN(mac,destination,data,length,wire);
    if(!n){txErrors++;return false;}
    if(!udp.beginPacket(ip,LAN_PORT)){txErrors++;return false;}
    if(udp.write(wire,n)!=n){txErrors++;return false;}
    if(!udp.endPacket()){txErrors++;return false;}
    return true; // Application ACK, not this return value, proves command acceptance.
  }
  bool receive(LanMessage &message) {
    if(!ready())return false;
    int n=udp.parsePacket();if(n<=0)return false;
    uint32_t atUs=(uint32_t)esp_timer_get_time();
    if(n>(int)(sizeof(LanHeader)+LAN_MAX_PAYLOAD)){udp.clear();rxDrops++;return false;}
    uint8_t wire[sizeof(LanHeader)+LAN_MAX_PAYLOAD];IPAddress remote=udp.remoteIP();uint16_t port=udp.remotePort();
    int got=udp.read(wire,sizeof(wire));LanHeader h{};
    if(got!=n||port!=LAN_PORT||!decodeLAN(wire,got,h)){udp.clear();rxInvalid++;return false;}
    if(!std::memcmp(h.mac,mac,6))return false;
    // A stale DHCP address must NEVER deliver a probe/command to the wrong board.
    if(!addressedTo(h,mac))return false;
    if(!peers.observe(h.mac,(uint32_t)remote,millis())){rxInvalid++;return false;}
    std::memcpy(message.mac,h.mac,6);message.length=h.length;
    std::memcpy(message.payload,wire+sizeof(h),h.length);message.rssi=(int8_t)WiFi.RSSI();message.atUs=atUs;
    return true;
  }
};

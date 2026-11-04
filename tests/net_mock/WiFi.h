#pragma once
#include "Arduino.h"
constexpr int WIFI_STA=1,WL_CONNECTED=3;
class IPAddress {
  uint8_t b[4]{};
public:
  IPAddress()=default;
  explicit IPAddress(uint32_t n){for(int i=0;i<4;i++)b[i]=(n>>(8*i))&255;}
  IPAddress(uint8_t a,uint8_t c,uint8_t d,uint8_t e):b{a,c,d,e}{}
  operator uint32_t()const{return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
  uint8_t operator[](int i)const{return b[i];}
  bool operator==(const IPAddress &r)const{return (uint32_t)*this==(uint32_t)r;}
  String toString()const{char s[20];snprintf(s,sizeof(s),"%u.%u.%u.%u",b[0],b[1],b[2],b[3]);return String(s);}
};
struct FakeSTA {bool started=false;int begins=0;bool begin(bool){begins++;started=true;return true;}};
struct FakeWiFi {
  FakeSTA STA;
  int state=0,currentMode=0,reconnects=0;
  bool persistentValue=true,autoReconnect=false,sleep=true;
  int signal=-55,currentChannel=6;
  uint8_t ownMac[6]={0xA0,1,2,3,4,5};
  IPAddress address,mask{255,255,255,0};
  std::string ssid,password,host;
  void persistent(bool b){persistentValue=b;}
  void mode(int m){currentMode=m;}
  void setAutoReconnect(bool b){autoReconnect=b;}
  void setSleep(bool b){sleep=b;}
  void disconnect(bool,bool){state=0;}
  uint8_t *macAddress(uint8_t *out){if(!STA.started)return nullptr;std::copy(ownMac,ownMac+6,out);return out;}
  void setHostname(const char *s){if(STA.started)host=s;}
  void begin(const char *s,const char *p){STA.started=true;ssid=s;password=p;}
  int status()const{return state;}
  int RSSI()const{return signal;}
  int channel()const{return currentChannel;}
  IPAddress localIP()const{return address;}
  IPAddress subnetMask()const{return mask;}
  void reconnect(){reconnects++;}
};
inline FakeWiFi WiFi;

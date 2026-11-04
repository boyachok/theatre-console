#pragma once
struct FakeMDNS {
  int starts=0,stops=0,services=0;
  bool begin(const char*){starts++;return true;}
  void end(){stops++;}
  void addService(const char*,const char*,int){services++;}
};
inline FakeMDNS MDNS;

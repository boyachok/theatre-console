#pragma once
#include <stdint.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>
inline uint8_t mockRelayConfig=0;
inline bool mockPrefsWriteOK=true;
inline std::map<std::string,std::vector<uint8_t>> mockPrefsBytes;
class Preferences {
public:
  void begin(const char*,bool){}
  uint8_t getUChar(const char*,uint8_t){return mockRelayConfig;}
  size_t putUChar(const char*,uint8_t value){if(!mockPrefsWriteOK)return 0;mockRelayConfig=value;return 1;}
  size_t getBytesLength(const char *key){return mockPrefsBytes[key].size();}
  size_t getBytes(const char *key,void *data,size_t n){auto &b=mockPrefsBytes[key];if(b.size()!=n)return 0;memcpy(data,b.data(),n);return n;}
  size_t putBytes(const char *key,const void *data,size_t n){if(!mockPrefsWriteOK)return 0;auto p=static_cast<const uint8_t*>(data);mockPrefsBytes[key]=std::vector<uint8_t>(p,p+n);return n;}
};

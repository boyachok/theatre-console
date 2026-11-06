#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>
constexpr size_t UPDATE_SIZE_UNKNOWN=(size_t)-1;constexpr int U_FLASH=0;
class FakeUpdate {
public:
 bool beginOK=true,writeOK=true,endOK=true,running=false;unsigned begins=0,writes=0,ends=0,aborts=0;std::vector<uint8_t> image;
 bool begin(size_t,int){begins++;running=beginOK;image.clear();return beginOK;}
 size_t write(uint8_t *data,size_t n){writes++;if(!writeOK)return 0;image.insert(image.end(),data,data+n);return n;}
 bool end(bool){ends++;running=false;return endOK;}
 void abort(){aborts++;running=false;}
};
inline FakeUpdate Update;

#pragma once
#include <stdint.h>
#include <stddef.h>
// XY-V17B datasheet 13288.pdf: AA, command, payload length, payload, sum mod 256.
inline size_t xyFrame(uint8_t command,const uint8_t *data,uint8_t len,uint8_t *out) {
  if(len>32)return 0;
  out[0]=0xAA;out[1]=command;out[2]=len;uint8_t sum=0xAA+command+len;
  for(uint8_t i=0;i<len;i++){out[3+i]=data[i];sum+=data[i];}
  out[3+len]=sum;return 4+len;
}
struct XYParser {
  uint8_t buf[36]{};uint8_t n=0;
  uint32_t checksumErrors=0,lengthErrors=0;
  void reset(){n=0;}
  bool feed(uint8_t c) {
    if(!n){if(c==0xAA)buf[n++]=c;return false;}
    buf[n++]=c;
    if(n==3&&buf[2]>32){lengthErrors++;n=c==0xAA?1:0;return false;}
    if(n<4||n!=4+buf[2])return false;
    uint8_t sum=0;for(uint8_t i=0;i<n-1;i++)sum+=buf[i];
    bool good=sum==buf[n-1];if(!good)checksumErrors++;n=0;return good;
  }
};

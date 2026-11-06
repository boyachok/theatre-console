#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
namespace YX {
constexpr size_t FRAME_SIZE=10;
enum Command:uint8_t {NEXT=0x01,PREVIOUS=0x02,PLAY_TRACK=0x03,VOLUME=0x06,EQ=0x07,SELECT=0x09,RESUME=0x0d,PAUSE=0x0e,STOP=0x16,REPEAT=0x19,
 CARD_INSERT=0x3a,CARD_REMOVE=0x3b,FILE_END=0x3d,INIT=0x3f,ERROR=0x40,ACK=0x41,STATUS=0x42,QUERY_VOLUME=0x43,QUERY_EQ=0x44,FILES=0x48,TRACK=0x4c};
inline uint16_t checksum(const uint8_t *b){uint16_t sum=0;for(int i=1;i<=6;i++)sum+=b[i];return (uint16_t)(0-sum);}
inline void frame(uint8_t command,uint16_t value,uint8_t (&out)[10],bool feedback=true){
 out[0]=0x7e;out[1]=0xff;out[2]=6;out[3]=command;out[4]=feedback?1:0;out[5]=value>>8;out[6]=value&255;
 uint16_t sum=checksum(out);out[7]=sum>>8;out[8]=sum&255;out[9]=0xef;
}
struct Parser {
 uint8_t buf[10]{},n=0;uint32_t checksumErrors=0,lengthErrors=0;
 void reset(){n=0;}
 void resync(){for(uint8_t i=1;i<n;i++)if(buf[i]==0x7e&&(i+1==n||buf[i+1]==0xff)){n-=i;memmove(buf,buf+i,n);return;}n=0;}
 bool feed(uint8_t byte){
  if(!n&&byte!=0x7e)return false;buf[n++]=byte;
  if((n==2&&buf[1]!=0xff)||(n==3&&buf[2]!=6)){lengthErrors++;resync();return false;}
  if(n<10)return false;
  if(buf[1]!=0xff||buf[2]!=6||buf[9]!=0xef){lengthErrors++;resync();return false;}
  uint16_t expected=checksum(buf),received=((uint16_t)buf[7]<<8)|buf[8];
  if(expected!=received){checksumErrors++;resync();return false;}
  n=0;return true;
 }
 uint16_t value()const{return ((uint16_t)buf[5]<<8)|buf[6];}
};
}

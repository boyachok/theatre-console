#pragma once
#include <Arduino.h>
#include <cstring>
#include "hardware.h"
#include "xy_protocol.h"
#include "uart_stats.h"
struct AudioFrame {uint8_t cmd=0,len=0,data[2]{};};
class Player {
  HardwareSerial uart{1};
  XYParser parser;
  AudioFrame queue[8];uint8_t head=0,count=0,queryIndex=0;
  uint32_t nextTx=0,lastByte=0,bootStopAt=0,lastTransmit=0;
  UartStats stats{};
  uint8_t rxHistory[16]{},rxHead=0,rxCount=0;
  uint16_t age(bool exists,uint32_t at)const {
    return exists?(uint16_t)std::min((uint32_t)65534,millis()-at):65535;
  }
public:
  bool enabled=false,autoPoll=true;
  uint8_t state=255,volume=255,mode=255,eq=255,drive=255;
  uint16_t track=0,tracks=0;
  uint32_t lastRx=0;
  void begin() {
    uart.begin(9600,SERIAL_8N1,PLAYER_RX,PLAYER_TX);
    stop();bootStopAt=millis()+1500;
  }
  bool bootSettled()const{return !bootStopAt;}
  uint8_t freeSlots()const{return 8-count;}
  bool ready()const{return !bootStopAt&&lastRx&&millis()-lastRx<3000;}
  bool enqueue(uint8_t cmd,uint16_t value=0,uint8_t len=0) {
    if(count==8||len>2)return false;
    AudioFrame &f=queue[(head+count)%8];f.cmd=cmd;f.len=len;
    if(len==1)f.data[0]=(uint8_t)value;
    if(len==2){f.data[0]=value>>8;f.data[1]=value&255;}
    count++;return true;
  }
  void stop() {head=count=0;enqueue(0x04);}
  void poll() {queryIndex=0;nextTx=millis();enqueue(0x01);}
  void clearStats() {
    stats=UartStats{};rxHead=rxCount=0;
    parser.checksumErrors=parser.lengthErrors=0;
    // Do not reset actual playback state or the safety-relevant last valid response.
  }
  UartStats diagnostics()const {
    UartStats s=stats;s.checksumErrors=parser.checksumErrors;s.lengthErrors=parser.lengthErrors;
    s.txAge=age(stats.txFrames,lastTransmit);s.rxAge=age(stats.rxBytes,lastByte);s.replyAge=age(lastRx!=0,lastRx);
    s.rxLen=rxCount;
    for(uint8_t i=0;i<rxCount;i++)s.lastRx[i]=rxHistory[(rxHead+16-rxCount+i)%16];
    s.txPin=PLAYER_TX;s.rxPin=PLAYER_RX;s.rxLevel=digitalRead(PLAYER_RX);s.polling=autoPoll;
    return s;
  }
  void tick() {
    uint32_t now=millis();
    if(bootStopAt&&(int32_t)(now-bootStopAt)>=0){bootStopAt=0;stop();}
    if(parser.n&&now-lastByte>100){parser.reset();stats.frameTimeouts++;}
    int budget=64;
    while(budget--&&uart.available()) {
      uint8_t c=uart.read();lastByte=now;stats.rxBytes++;
      rxHistory[rxHead]=c;rxHead=(rxHead+1)%16;if(rxCount<16)rxCount++;
      if(!parser.feed(c))continue;
      stats.validFrames++;
      uint8_t cmd=parser.buf[1],len=parser.buf[2];const uint8_t *d=parser.buf+3;
      if(stats.txLen==4+len&&!memcmp(parser.buf,stats.lastTx,stats.txLen))stats.loopbacks++;
      bool recognized=true;
      if(cmd==0x01&&len==1&&d[0]<=2)state=d[0];
      else if(cmd==0x0A&&len==1)drive=d[0];
      else if(cmd==0x0D&&len==2)track=(d[0]<<8)|d[1];
      else if(cmd==0x0C&&len==2)tracks=(d[0]<<8)|d[1];
      else recognized=false;
      if(recognized){lastRx=now;stats.replies++;}
    }
    if((int32_t)(now-nextTx)<0)return;
    if(count) {
      AudioFrame &f=queue[head];uint8_t bytes[6];size_t len=xyFrame(f.cmd,f.data,f.len,bytes);
      size_t written=uart.write(bytes,len);stats.txBytes+=written;
      if(written==len){stats.txFrames++;stats.txLen=len;memcpy(stats.lastTx,bytes,len);lastTransmit=now;}
      else stats.txFailures++;
      head=(head+1)%8;count--;nextTx=now+80;
    } else if(autoPoll) {
      static const uint8_t queries[]={0x01,0x0D,0x0C,0x0A};
      enqueue(queries[queryIndex++%4]);nextTx=now+400;
    }
  }
};

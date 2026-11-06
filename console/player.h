#pragma once
#include <Arduino.h>
#include <cstring>
#include "hardware.h"
#include "yx_protocol.h"
#include "uart_stats.h"
struct AudioFrame {uint8_t cmd=0;uint16_t value=0;};
class Player {
 HardwareSerial uart{1};YX::Parser parser;
 AudioFrame queue[8];uint8_t head=0,count=0,queryIndex=0,initStage=0;
 uint32_t nextTx=0,nextPoll=0,lastByte=0,lastTransmit=0,bootAt=0;
 bool initializing=true,hasReply=false,wantPlay=false,switchPending=false,repeatTrack=false;
 uint16_t requestedTrack=0;
 UartStats stats{};PlayerDebug extra{};
 uint8_t rxHistory[16]{},rxHead=0,rxCount=0,recent[4][10]{},recentHead=0,recentCount=0,fullTx[10]{};
 uint16_t age(bool exists,uint32_t at)const{return exists?(uint16_t)std::min((uint32_t)65534,millis()-at):65535;}
 bool enqueue(uint8_t cmd,uint16_t value=0){if(count==8)return false;queue[(head+count)%8]={cmd,value};count++;return true;}
 void restartInitialization(){initializing=true;initStage=0;bootAt=millis()+1500;wantPlay=switchPending=repeatTrack=false;state=volume=mode=eq=255;track=tracks=0;hasReply=false;lastRx=0;head=count=0;enqueue(YX::STOP);}
 bool echo()const{for(uint8_t i=0;i<recentCount;i++)if(!memcmp(parser.buf,recent[i],10))return true;return false;}
 void receive(uint32_t now){
  if(parser.n&&now-lastByte>100){parser.reset();stats.frameTimeouts++;}
  int budget=64;
  while(budget--&&uart.available()){
   uint8_t c=uart.read();lastByte=now;stats.rxBytes++;rxHistory[rxHead]=c;rxHead=(rxHead+1)%16;if(rxCount<16)rxCount++;
   if(!parser.feed(c))continue;stats.validFrames++;
   if(echo()){stats.loopbacks++;continue;}
   // Requests always use feedback=1. A request echo must NEVER become a player response.
   if(parser.buf[4]!=0)continue;
   uint8_t cmd=parser.buf[3];uint16_t value=parser.value();bool recognized=true;
   switch(cmd){
    case YX::ACK:if(value)recognized=false;else extra.acks++;break;
    case YX::ERROR:extra.lastError=value&255;extra.errors++;break;
    case YX::STATUS:if((value&255)<=2&&(value>>8)<=2){state=value&255;if((value>>8)==2)drive=1;else if((value>>8)==1)drive=0;}else recognized=false;break;
    case YX::QUERY_VOLUME:if(value<=30)volume=value;else recognized=false;break;
    case YX::QUERY_EQ:if(value<=5)eq=value;else recognized=false;break;
    case YX::TRACK:track=value;break;
    case YX::FILES:tracks=value;break;
    case YX::INIT:drive=(value&2)?1:(value&1)?0:255;if(!initializing)restartInitialization();break;
    case YX::CARD_INSERT:if(value&2){drive=1;restartInitialization();}break;
    case YX::CARD_REMOVE:if(value&2){drive=255;tracks=0;state=0;stop();}break;
    case YX::FILE_END:
     extra.ends++;
     if(wantPlay&&!switchPending&&(!requestedTrack||value==requestedTrack)){
      if(!repeatTrack){state=0;stop();} // Single cue: EOF must not replay/start the next track.
     }
     break;
    default:recognized=false;break;
   }
   if(recognized){lastRx=now;hasReply=true;stats.replies++;}
  }
 }
public:
 bool enabled=false,autoPoll=true;
 uint8_t state=255,volume=255,mode=255,eq=255,drive=255;
 uint16_t track=0,tracks=0;uint32_t lastRx=0,playSerial=0,playSentAt=0;
 void begin(){
  uart.begin(9600,SERIAL_8N1,PLAYER_RX,PLAYER_TX);extra.family=1;restartInitialization();nextTx=millis();
 }
 bool bootSettled()const{return !initializing;}
 bool ready()const{return !initializing&&hasReply&&millis()-lastRx<3000;}
 bool hasResponse()const{return hasReply;}
 uint8_t freeSlots()const{return 8-count;}
 bool startTrack(uint16_t file,bool loop=false){
  if(!file||initializing||freeSlots()<3)return false;
  // Atomic queue admission: never enqueue a partial mute/mode/play transaction.
  enqueue(YX::VOLUME,0);enqueue(YX::REPEAT,loop?0:1);enqueue(YX::PLAY_TRACK,file);
  repeatTrack=loop;wantPlay=switchPending=true;return true;
 }
 bool playTrack(uint16_t file){if(!file||initializing||freeSlots()<2)return false;enqueue(YX::REPEAT,1);enqueue(YX::PLAY_TRACK,file);repeatTrack=false;wantPlay=switchPending=true;return true;}
 bool resume(){if(initializing||!enqueue(YX::RESUME))return false;wantPlay=switchPending=true;return true;}
 bool pause(){return enqueue(YX::PAUSE);}
 bool next(){if(initializing||!enqueue(YX::NEXT))return false;wantPlay=switchPending=true;return true;}
 bool previous(){if(initializing||!enqueue(YX::PREVIOUS))return false;wantPlay=switchPending=true;return true;}
 bool setVolume(uint8_t value){
  if(value>30)return false;
  // Keep the newest unsent ramp point, but never rewrite the mute before a queued PLAY.
  for(uint8_t k=count;k>0;k--){auto &f=queue[(head+k-1)%8];if(f.cmd==YX::PLAY_TRACK||f.cmd==YX::STOP||f.cmd==YX::SELECT||f.cmd==YX::REPEAT)break;if(f.cmd==YX::VOLUME){f.value=value;return true;}}
  return enqueue(YX::VOLUME,value);
 }
 bool setRepeat(bool value){if(!enqueue(YX::REPEAT,value?0:1))return false;repeatTrack=value;return true;}
 bool setEq(uint8_t value){return value<=5&&enqueue(YX::EQ,value);}
 bool selectTF(){if(initializing)return false;stop();enqueue(YX::VOLUME,0);enqueue(YX::SELECT,2);enqueue(YX::STOP);return true;}
 void stop(){wantPlay=switchPending=false;requestedTrack=0;if(initializing&&initStage)return;head=count=0;enqueue(YX::STOP);} // Keep the safe STOP/mute/TF/STOP bootstrap intact under repeated offline ALL_OFF.
 void poll(){queryIndex=0;enqueue(YX::STATUS);}
 void clearStats(){stats=UartStats{};extra=PlayerDebug{};extra.family=1;rxHead=rxCount=0;recentHead=recentCount=0;parser.checksumErrors=parser.lengthErrors=0;}
 UartStats diagnostics()const{
  UartStats s=stats;s.checksumErrors=parser.checksumErrors;s.lengthErrors=parser.lengthErrors;
  s.txAge=age(stats.txFrames,lastTransmit);s.rxAge=age(stats.rxBytes,lastByte);s.replyAge=age(hasReply,lastRx);s.rxLen=rxCount;
  for(uint8_t i=0;i<rxCount;i++)s.lastRx[i]=rxHistory[(rxHead+16-rxCount+i)%16];
  s.txPin=PLAYER_TX;s.rxPin=PLAYER_RX;s.rxLevel=digitalRead(PLAYER_RX);s.polling=autoPoll;return s;
 }
 PlayerDebug debug()const{PlayerDebug d=extra;if(stats.txLen==10){d.txTail[0]=fullTx[8];d.txTail[1]=fullTx[9];}return d;}
 void tick(){
  uint32_t now=millis();receive(now);
  if(initializing&&!initStage&&(int32_t)(now-bootAt)>=0){
   head=count=0;enqueue(YX::STOP);enqueue(YX::VOLUME,0);enqueue(YX::SELECT,2);enqueue(YX::STOP);initStage=1;
  }
  if((int32_t)(now-nextTx)<0)return;
  if(count){
   AudioFrame f=queue[head];uint8_t bytes[10];YX::frame(f.cmd,f.value,bytes);
   size_t written=uart.write(bytes,sizeof(bytes));stats.txBytes+=written;
   head=(head+1)%8;count--;nextTx=now+80;
   if(written!=sizeof(bytes)){stats.txFailures++;if(initializing){head=count=0;initStage=0;bootAt=now+1500;}return;}
   stats.txFrames++;stats.txLen=10;memcpy(stats.lastTx,bytes,8);memcpy(fullTx,bytes,10);lastTransmit=now;
   memcpy(recent[recentHead],bytes,10);recentHead=(recentHead+1)%4;if(recentCount<4)recentCount++;
   if(f.cmd==YX::SELECT){nextTx=now+200;if(initializing)initStage=2;}
   if(initializing&&initStage==2&&f.cmd==YX::STOP)initializing=false;
   if(f.cmd==YX::PLAY_TRACK){requestedTrack=f.value;switchPending=false;playSentAt=now;if(!++playSerial)++playSerial;}
   else if(f.cmd==YX::NEXT||f.cmd==YX::PREVIOUS||f.cmd==YX::RESUME){requestedTrack=0;switchPending=false;}
  }else if(autoPoll&&!initializing&&(int32_t)(now-nextPoll)>=0){
   static const uint8_t queries[]={YX::STATUS,YX::QUERY_VOLUME,YX::TRACK,YX::FILES};
   enqueue(queries[queryIndex++%4]);nextPoll=now+400;
  }
 }
};

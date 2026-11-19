#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
namespace RemoteBattery {
constexpr uint8_t PIN=4;
constexpr uint16_t FULL_FLAG=1,LOW_FLAG=2;
struct Record {uint32_t magic=0x31544252;uint16_t version=1,fullMv=4200,lowMv=3500,flags=0;uint32_t crc=0;};
static_assert(sizeof(Record)==16,"Independent battery record, never overlaps button-test settings");
inline uint32_t checksum(const Record &r){uint32_t c=0xffffffffU;const auto *b=(const uint8_t*)&r;for(size_t i=0;i<offsetof(Record,crc);i++){c^=b[i];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320UL&-(c&1));}return ~c;}
inline bool validRecord(const Record &r){return r.magic==0x31544252&&r.version==1&&r.fullMv>=3700&&r.fullMv<=4350&&r.lowMv>=3000&&r.lowMv<=4150&&r.fullMv>=r.lowMv+200&&!(r.flags&~3)&&(!(r.flags&LOW_FLAG)||(r.flags&FULL_FLAG))&&r.crc==checksum(r);}
class Monitor {
 uint16_t window[8]{};uint8_t head=0,count=0;uint32_t sum=0,lastAt=0;uint8_t samples=0;
 uint16_t input=0,filtered=0;bool batchOK=true,readingOK=false;int band=-1;
public:
 Record calibration{};
 void load(const Record &r){if(validRecord(r))calibration=r;}
 void sample(uint32_t adcMv,uint32_t now){
  uint32_t mv=adcMv*2;input=mv>65535?65535:mv;
  if(adcMv>2200||mv<2600||mv>4400)batchOK=false;
  sum+=adcMv>2200?0:mv;
  if(++samples<20)return;
  uint16_t mean=sum/20;readingOK=batchOK;lastAt=now;
  if(readingOK){filtered=count?(filtered*3UL+mean+2)/4:mean;window[head]=mean;head=(head+1)%8;if(count<8)count++;}
  else {count=0;filtered=0;band=-1;}
  samples=0;sum=0;batchOK=true;
 }
 uint16_t rawMv()const{return input;}
 uint16_t mv()const{return filtered;}
 uint32_t age(uint32_t now)const{return now-lastAt;}
 uint8_t windows()const{return count;}
 uint16_t spread()const{if(!count)return 65535;uint16_t lo=65535,hi=0;for(int i=0;i<count;i++){if(window[i]<lo)lo=window[i];if(window[i]>hi)hi=window[i];}return hi-lo;}
 bool fresh(uint32_t now)const{return readingOK&&count&&age(now)<1500;}
 bool stable(uint32_t now)const{return fresh(now)&&count==8&&spread()<=80;}
 bool enabled()const{return calibration.flags&FULL_FLAG;}
 bool calibrated()const{return (calibration.flags&3)==3;}
 int reserve(uint32_t now)const{if(!enabled()||!fresh(now))return -1;int n=((int)filtered-calibration.lowMv)*100/(calibration.fullMv-calibration.lowMv);return n<0?0:n>100?100:n;}
 const char *prepare(bool full,uint32_t now,Record &next)const{
  if(!stable(now))return "ADC not stable/fresh: connect divider, release buttons, wait 4 seconds";
  if(!full&&!enabled())return "Capture FULL first";
  uint32_t mean=0;for(auto v:window)mean+=v;mean=(mean+4)/8;
  next=calibration;
  if(full){if(mean<3700||mean>4350)return "FULL outside safe 1S range";next.fullMv=mean;next.flags|=FULL_FLAG;}
  else {if(mean<3000||mean>4150)return "LOW outside safe warning range; do not deep-discharge";next.lowMv=mean;next.flags|=LOW_FLAG;}
  next.crc=checksum(next);if(!validRecord(next))return "FULL must exceed LOW by at least 200 mV";
  return nullptr;
 }
 uint32_t color(uint32_t now){
  int p=reserve(now);if(p<0)return (now/500)%2?0x080008:0; // Unknown must never look charged.
  if(filtered<=calibration.lowMv)return (now/300)%2?0x180000:0;
  if(band<0)band=p<=20?0:p<=50?1:2;
  else if(band==0&&p>23)band=p>53?2:1;
  else if(band==1){if(p<18)band=0;else if(p>53)band=2;}
  else if(band==2&&p<48)band=p<18?0:1;
  return band==2?0x001400:band==1?0x120a00:0x140000;
 }
};
// Both-button chord is swallowed until BOTH are released and debounced.
// No latent short/3s-long scene action may escape on staggered release or timeout.
class LinkDiagnostic {
 bool armed=false,blocked=false,tracking=false,fired=false,releasing=false;uint32_t heldAt=0,releaseAt=0,startedAt=0;bool showing=false;
public:
 bool active(uint32_t now){if(showing&&now-startedAt>=10000)showing=false;return showing;}
 uint32_t remaining(uint32_t now){return active(now)?10000-(now-startedAt):0;}
 bool suppress()const{return blocked;}
 bool tick(bool forward,bool back,uint32_t now,bool allowed=true){
  active(now);
  if(!allowed){armed=tracking=fired=false;blocked=forward||back;releasing=false;return false;}
  if(!forward&&!back){
   if(!releasing){releaseAt=now;releasing=true;}
   if(now-releaseAt>=30){armed=true;blocked=tracking=fired=false;}
   return false;
  }
  releasing=false;
  if(forward&&back&&armed&&!blocked){blocked=tracking=true;fired=false;heldAt=now;}
  if(blocked){
   if(!forward||!back)tracking=false;
   if(tracking&&!fired&&now-heldAt>=1000){fired=true;showing=true;startedAt=now;return true;}
  }
  return false;
 }
};
}

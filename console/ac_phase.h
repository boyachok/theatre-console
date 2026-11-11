#pragma once
#include <stdint.h>
#include <cmath>
#ifdef ESP_PLATFORM
#include <esp_attr.h>
#define MAINS_IRAM IRAM_ATTR
#else
#define MAINS_IRAM
#endif
namespace Mains {
enum Fault:uint8_t { OFF_STATE=0,WAIT_ZERO=1,OK=2,ZERO_LOST=3,TIMING=4,CONTROL_LOST=5,LINK_LOST=6,RESOURCE=7 };
struct Pulse {uint64_t zero=0,fire=0,end=0;uint32_t epoch=0;};
struct State {bool enabled=false,qualified=false,latched=false;uint8_t requested=0,paired=0;Fault fault=OFF_STATE;uint32_t good=0,rejected=0,scheduled=0,late=0,halfUs=10000;uint64_t lastZero=0;};
// Pure state machine, called under the adapter lock. No I/O, global RNG, floating point or loops in zeroCross.
class Phase {
 State s;uint16_t phase[256]{};uint8_t stable=0,pair=0,noise=0;uint32_t epoch=1;uint64_t controllerAt=0,armedAt=0;
public:
 Phase(){for(int v=1;v<256;v++){double low=0,high=3.141592653589793,power=v/255.;for(int n=0;n<24;n++){double a=(low+high)*.5,p=1-a/3.141592653589793+std::sin(2*a)/6.283185307179586;if(p>power)low=a;else high=a;}phase[v]=(uint16_t)((low+high)*.5/3.141592653589793*65535);}}
 State MAINS_IRAM state()const{return s;}
 void MAINS_IRAM trip(Fault fault){s.latched=true;s.qualified=false;s.paired=0;s.fault=fault;stable=0;++epoch;}
 void disable(){s.enabled=false;s.requested=s.paired=0;s.qualified=s.latched=false;s.fault=OFF_STATE;s.lastZero=0;stable=pair=noise=0;++epoch;}
 void arm(uint64_t now){if(s.enabled&&!s.latched)return;s.enabled=true;s.latched=false;s.qualified=false;s.lastZero=0;s.requested=s.paired=0;s.fault=WAIT_ZERO;stable=pair=noise=0;armedAt=controllerAt=now;++epoch;}
 void resourceFailure(){s.enabled=true;trip(RESOURCE);}
 void level(uint8_t v){s.requested=v;if(!v){s.paired=0;++epoch;}}
 void controller(uint64_t now,bool /*lease*/){controllerAt=now;} // Wi-Fi/master loss is not an output fault.
 void watchdog(uint64_t now){if(!s.enabled||s.latched)return;if(now-controllerAt>50000){trip(CONTROL_LOST);return;}if(now-(s.lastZero?s.lastZero:armedAt)>35000)trip(ZERO_LOST);}
 bool MAINS_IRAM zeroCross(uint64_t now,Pulse &out){
  if(!s.enabled||s.latched)return false;
  if(!s.lastZero){s.lastZero=now;return false;}uint64_t delta=now-s.lastZero;
  if(delta<8000){s.rejected++;if(++noise>=3)trip(TIMING);return false;}
  if(delta>12000||(stable&&((delta>s.halfUs?delta-s.halfUs:s.halfUs-delta)>400))){s.rejected++;if(s.qualified){trip(TIMING);return false;}s.lastZero=now;stable=0;s.qualified=false;s.paired=0;s.fault=WAIT_ZERO;return false;}
  noise=0;s.halfUs=(uint32_t)delta;s.lastZero=now;s.good++;if(stable<6)stable++;
  if(stable<6)return false;s.qualified=true;s.fault=OK;
  if(!pair)s.paired=s.requested;pair^=1; // Same level for both half-waves.
  if(!s.paired)return false;
  uint32_t delay=(s.halfUs*phase[s.paired])/65535;if(delay<300)delay=300;if(delay>s.halfUs-1000)delay=s.halfUs-1000;
  out={now,now+delay,now+s.halfUs-500,epoch};s.scheduled++;return true;
 }
 bool authorize(const Pulse &p,uint64_t now){watchdog(now);if(!s.enabled||s.latched||!s.requested||p.epoch!=epoch||p.zero!=s.lastZero)return false;
  if(now>p.fire+150||now+201>=p.end){s.late++;trip(TIMING);return false;}return true;
 }
};
inline const char *faultName(Fault f){const char *names[]={"disabled","waiting_zero","ok","zero_lost","timing_fault","controller_stalled","link_lost","resource_fault"};return f<=RESOURCE?names[f]:"invalid";}
}
#undef MAINS_IRAM

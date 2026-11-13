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
enum Fault:uint8_t {OFF_STATE=0,WAIT_ZERO=1,OK=2,ZERO_LOST=3,TIMING=4,CONTROL_LOST=5,LINK_LOST=6,RESOURCE=7};
enum Reason:uint8_t {NONE,ACQUIRED,ZC_TIMEOUT,ZC_GAP,CPU_STALL,PULSE_LATE,PULSE_END,TX_BUSY,TX_SLOW,TX_REJECT,PENDING_REPLACED,RESOURCE_ERROR};
struct Pulse {uint64_t zero=0,fire=0,end=0;uint32_t epoch=0;};
struct Event {uint64_t at=0;uint32_t id=0,intervalUs=0,halfUs=10000;Reason reason=NONE;bool hard=false;};
struct State {
 bool enabled=false,qualified=false,latched=false;uint8_t requested=0,paired=0;Fault fault=OFF_STATE;
 uint32_t good=0,rejected=0,scheduled=0,late=0,halfUs=10000,phaseDelayUs=0,lastIntervalUs=0;
 uint32_t ignoredEarly=0,duplicateEdges=0,ignoredWindow=0,syncLosses=0,recoveries=0,skippedPulses=0,staleClockSamples=0;uint64_t lastZero=0;
};
// Theatre 0.7.14, ported from the physically exercised test.3 bench.
// Every pulse still requires a fresh accepted Z-C; never free-runs.
// Filter: refractory 2ms + 8..12ms physical range + +/-750us prediction window,
// fixed sorting-network median of five accepted periods + 1/8 period IIR.
// Bad edges do not latch. Missing/gapped Z-C mutes gate and reacquires six intervals.
// CPU/resource failures remain explicit-reset hard faults. A missed RMT deadline drops that half only.
class Phase {
 State s;Event ev;uint16_t table[256]{};uint32_t history[5]{},periodQ8=10000*256,epoch=1;uint8_t head=0,stable=0,pair=0;bool haveHistory=false,everLocked=false;uint64_t controllerAt=0,armedAt=0;
 void MAINS_IRAM note(Reason reason,uint64_t now,uint32_t interval,bool hard=false);
 void MAINS_IRAM lose(Reason reason,uint64_t now,uint32_t interval);
 void MAINS_IRAM estimate(uint32_t interval,bool locked);
public:
 Phase(){for(int v=1;v<256;v++){double low=0,high=3.141592653589793,power=v/255.;for(int n=0;n<24;n++){double a=(low+high)*.5,p=1-a/3.141592653589793+std::sin(2*a)/6.283185307179586;if(p>power)low=a;else high=a;}table[v]=(uint16_t)((low+high)*.5/3.141592653589793*65535);}}
 State MAINS_IRAM state()const;Event MAINS_IRAM event()const;
 bool MAINS_IRAM outputBlocked()const;bool MAINS_IRAM hardFault()const;
 void MAINS_IRAM trip(Fault fault,uint64_t now=0,Reason reason=RESOURCE_ERROR);
 void disable();void arm(uint64_t now);void level(uint8_t v);void controller(uint64_t now,bool lease);
 void MAINS_IRAM watchdog(uint64_t now);bool MAINS_IRAM zeroCross(uint64_t now,Pulse &out);
 bool authorize(const Pulse &p,uint64_t now);
 void MAINS_IRAM skip(Reason reason,uint64_t now,uint32_t interval=0);
};
State MAINS_IRAM Phase::state()const{return s;}
Event MAINS_IRAM Phase::event()const{return ev;}
void MAINS_IRAM Phase::skip(Reason reason,uint64_t now,uint32_t interval){s.skippedPulses++;note(reason,now,interval);}
void MAINS_IRAM Phase::note(Reason reason,uint64_t now,uint32_t interval,bool hard){ev.at=now;ev.id++;ev.intervalUs=interval;ev.halfUs=s.halfUs;ev.reason=reason;ev.hard=hard;}
bool MAINS_IRAM Phase::outputBlocked()const{return !s.enabled||!s.qualified||s.latched||!s.requested;}
bool MAINS_IRAM Phase::hardFault()const{return s.latched;}
void MAINS_IRAM Phase::trip(Fault f,uint64_t now,Reason reason){if(s.latched)return;s.latched=true;s.qualified=false;s.paired=0;s.fault=f;stable=0;++epoch;note(reason,now,0,true);}
void Phase::disable(){s.enabled=false;s.requested=s.paired=0;s.qualified=s.latched=false;s.fault=OFF_STATE;s.lastZero=0;stable=pair=0;haveHistory=false;everLocked=false;++epoch;}
void Phase::arm(uint64_t now){if(s.enabled&&!s.latched)return;s.enabled=true;s.latched=false;s.qualified=false;s.lastZero=0;s.requested=s.paired=0;s.fault=WAIT_ZERO;stable=pair=0;haveHistory=false;everLocked=false;armedAt=controllerAt=now;++epoch;}
void Phase::level(uint8_t v){if(!v){s.paired=0;if(s.requested)++epoch;}s.requested=v;}
void Phase::controller(uint64_t now,bool /*lease*/){controllerAt=now;}
void MAINS_IRAM Phase::lose(Reason r,uint64_t now,uint32_t interval){if(s.qualified)s.syncLosses++;s.qualified=false;s.paired=0;s.fault=ZERO_LOST;s.lastZero=0;stable=pair=0;haveHistory=false;armedAt=now;++epoch;note(r,now,interval);}
void MAINS_IRAM Phase::watchdog(uint64_t now){
 if(!s.enabled||s.latched)return;
 // A caller can have sampled time before acquiring the lock; a newer ISR timestamp is NOT a timeout.
 if(now<controllerAt||(s.lastZero&&now<s.lastZero)||(!s.lastZero&&now<armedAt)){s.staleClockSamples++;return;}
 if(s.requested&&now-controllerAt>50000){trip(CONTROL_LOST,now,CPU_STALL);return;}
 uint64_t reference=s.lastZero?s.lastZero:armedAt;
 if(now-reference>35000&&s.fault!=ZERO_LOST)lose(ZC_TIMEOUT,now,(uint32_t)(now-reference));
}
void MAINS_IRAM Phase::estimate(uint32_t interval,bool locked){
 if(!haveHistory){history[0]=history[1]=history[2]=history[3]=history[4]=interval;head=0;haveHistory=true;}
 history[head]=interval;if(++head==5)head=0;
 uint32_t a=history[0],b=history[1],c=history[2],d=history[3],e=history[4],t;
 // Nine compare-exchanges, no dynamic sort, allocation, division or floating point in ISR.
#define SWAP_IF(x,y) if(x>y){t=x;x=y;y=t;}
 SWAP_IF(a,b) SWAP_IF(d,e) SWAP_IF(c,e) SWAP_IF(c,d) SWAP_IF(b,e) SWAP_IF(a,d) SWAP_IF(a,c) SWAP_IF(b,d) SWAP_IF(b,c)
#undef SWAP_IF
 if(!locked)periodQ8=c*256;else periodQ8+=(int32_t)(c*256-periodQ8)/8;
 s.halfUs=(periodQ8+128)>>8;
}
bool MAINS_IRAM Phase::zeroCross(uint64_t now,Pulse &out){
 if(!s.enabled||s.latched)return false;
 if(now<controllerAt||(s.lastZero&&now<s.lastZero)||(!s.lastZero&&now<armedAt)){s.staleClockSamples++;return false;}
 if(s.requested&&now-controllerAt>50000){trip(CONTROL_LOST,now,CPU_STALL);return false;}
 if(!s.lastZero){s.lastZero=now;s.fault=WAIT_ZERO;return false;}
 uint64_t wide=now-s.lastZero;uint32_t delta=wide>0xffffffffULL?0xffffffffUL:(uint32_t)wide;
 // Never advance the accepted anchor on a duplicate. Thousands of 350us double edges are harmless.
 if(delta<8000){s.rejected++;s.ignoredEarly++;if(delta<2000)s.duplicateEdges++;return false;}
 if(delta>12000){s.rejected++;lose(ZC_GAP,now,delta);s.lastZero=now;s.fault=WAIT_ZERO;return false;}
 uint32_t error=delta>s.halfUs?delta-s.halfUs:s.halfUs-delta;
 if(s.qualified&&error>750){s.rejected++;s.ignoredWindow++;return false;}
 if(!s.qualified&&haveHistory&&error>750){stable=0;haveHistory=false;}
 bool wasLocked=s.qualified;estimate(delta,wasLocked);s.lastIntervalUs=delta;s.lastZero=now;s.good++;
 if(stable<6)stable++;
 if(stable<6){s.fault=WAIT_ZERO;return false;}
 s.qualified=true;s.fault=OK;
 if(!wasLocked){if(everLocked)s.recoveries++;everLocked=true;note(ACQUIRED,now,delta);}
 if(!pair)s.paired=s.requested;pair^=1;
 if(!s.paired)return false;
 uint32_t delay=(s.halfUs*table[s.paired])/65535;if(delay<300)delay=300;if(delay>s.halfUs-1000)delay=s.halfUs-1000;
 s.phaseDelayUs=delay;out={now,now+delay,now+s.halfUs-500,epoch};s.scheduled++;return true;
}
bool Phase::authorize(const Pulse &p,uint64_t now){
 watchdog(now);if(now<p.zero){s.staleClockSamples++;return false;}if(outputBlocked()||p.epoch!=epoch||p.zero!=s.lastZero)return false;
 if(now>p.fire+150){s.late++;skip(PULSE_LATE,now,(uint32_t)(now-p.fire));return false;}
 if(now+201>=p.end){s.late++;skip(PULSE_END,now,(uint32_t)(now-p.zero));return false;}
 return true;
}
inline const char *faultName(Fault f){const char *n[]={"disabled","waiting_zero","ok","zero_lost","timing_fault","controller_stalled","link_lost","resource_fault"};return f<=RESOURCE?n[f]:"invalid";}
inline const char *reasonName(Reason r){const char *n[]={"none","acquired","zc_timeout","zc_gap","cpu_stall","pulse_late","pulse_end","rmt_busy","rmt_submit_slow","rmt_rejected","pending_replaced","resource_error"};return r<=RESOURCE_ERROR?n[r]:"invalid";}
}
#undef MAINS_IRAM

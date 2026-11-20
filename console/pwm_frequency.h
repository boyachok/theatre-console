#pragma once
#include <stdint.h>
#include <cstring>
#include "pwm_curve.h"
namespace PwmFrequency {
constexpr uint16_t MIN=2000,MAX=25000;
constexpr uint16_t maximum(uint8_t bits=THEATRE_PWM_BITS){return bits==8||bits==10?MAX:bits==12?9765:0;}
inline bool valid(uint32_t hz,uint8_t bits=THEATRE_PWM_BITS){return hz>=MIN&&hz<=maximum(bits);}
inline uint32_t ticks(uint32_t hz){return (80000000UL+hz/2)/hz;}
inline uint32_t crc(const void *data,size_t n){const auto *p=(const uint8_t*)data;uint32_t c=0xffffffffUL;while(n--){c^=*p++;for(int b=0;b<8;b++)c=(c>>1)^((c&1)?0xedb88320UL:0);}return ~c;}
struct __attribute__((packed)) Record {
 uint32_t magic=0x31514650;uint16_t hz=THEATRE_PWM_HZ;uint8_t version=1,reserved=0;uint32_t checksum=0;
 void seal(){checksum=crc(this,8);}
 bool validRecord()const{return magic==0x31514650&&version==1&&!reserved&&checksum==crc(this,8)&&valid(hz);}
};
// Optional use of the old PWM_REPORT reserved16B. Older peers ignore this extension.
struct __attribute__((packed)) Report {uint8_t tag[3]{'F','Q',1},bits=THEATRE_PWM_BITS;uint16_t active=0,saved=0;uint32_t periodTicks=0;uint16_t minimum=MIN,maximumHz=maximum();};
static_assert(sizeof(Record)==12&&sizeof(Report)==16,"PWM frequency record/report");
inline bool validReport(const Report &r){return r.tag[0]=='F'&&r.tag[1]=='Q'&&r.tag[2]==1&&valid(r.active,r.bits)&&valid(r.saved,r.bits)&&r.minimum==MIN&&r.maximumHz==maximum(r.bits)&&r.periodTicks==ticks(r.active);}
}

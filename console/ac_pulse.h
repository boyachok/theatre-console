#pragma once
#include <stdint.h>
#include <stddef.h>
namespace AcPulse {
// Software trial bounds, NOT certification of the mains module or its input ratings.
constexpr uint16_t DEFAULT_US=200,MIN_US=200,MAX_US=400;
inline bool valid(uint32_t us){return us>=MIN_US&&us<=MAX_US;}
inline uint32_t crc(const void *data,size_t n){auto p=(const uint8_t*)data;uint32_t c=0xffffffffUL;while(n--){c^=*p++;for(int b=0;b<8;b++)c=(c>>1)^((c&1)?0xedb88320UL:0);}return ~c;}
struct __attribute__((packed)) Record {
 uint32_t magic=0x31504341;uint16_t pulseUs=DEFAULT_US;uint8_t version=1,reserved=0;uint32_t checksum=0;
 void seal(){checksum=crc(this,8);}
 bool validRecord()const{return magic==0x31504341&&version==1&&!reserved&&valid(pulseUs)&&checksum==crc(this,8);}
};
struct __attribute__((packed)) Report {
 uint32_t magic=0;uint8_t protocol=0,type=0;uint16_t size=0;uint32_t session=0,deviceBoot=0,uptime=0,sequence=0;
 uint16_t pulseUs=DEFAULT_US,minUs=MIN_US,maxUs=MAX_US;
 uint8_t configFault=0,enabled=0,qualified=0,requested=0,paired=0,latched=0;
 uint32_t halfUs=10000,good=0,scheduled=0,late=0,skipped=0,syncLosses=0;uint8_t fault=0,reserved[3]{};
};
static_assert(sizeof(Record)==12&&sizeof(Report)==64,"AC pulse record/optional report");
inline bool validReport(const Report &p){return valid(p.pulseUs)&&p.minUs==MIN_US&&p.maxUs==MAX_US&&p.configFault<=1&&p.enabled<=1&&p.qualified<=1&&p.latched<=1&&p.halfUs>=8000&&p.halfUs<=12000&&p.fault<=7&&!p.reserved[0]&&!p.reserved[1]&&!p.reserved[2];}
}

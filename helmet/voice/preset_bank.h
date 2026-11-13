#ifndef RV_PRESET_BANK_H
#define RV_PRESET_BANK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "speech_chip_range.h"
#define RV_PRESET_COUNT 8
// Flags 0..3 carry power, notch, the legacy chip timbre bit and stronger low-cut.
// The current chip ignores the timbre bit when loading an existing preset.
// Bits 8..14 store the breath percentage.
// Older banks have zero in the new bits and retain the original checksum.
#define RV_CLASSIC_CHIP_FLAG (1u << 2)
#define RV_LOW_CUT_FLAG (1u << 3)
#define RV_BREATH_SHIFT 8
#define RV_BREATH_MASK (127u << RV_BREATH_SHIFT)
typedef struct {
  uint32_t flags,mode,algorithm,key,scale;
  float gain,volume,frequency,mix,drive,gateDb,wave,toneHz,formant,clarity,retuneMs,strength,transpose;
} RVSoundProfile;
typedef struct {uint32_t used;char name[64];RVSoundProfile sound;} RVPresetSlot;
typedef struct {uint32_t magic,version,generation,active;RVPresetSlot slots[RV_PRESET_COUNT];uint32_t checksum;} RVPresetBank;
typedef char RVBankSizeCheck[(sizeof(RVPresetBank)==1140)?1:-1];
static uint32_t rv_hash(const void *data,size_t size){
  const uint8_t *bytes=(const uint8_t *)data;uint32_t h=2166136261u;size_t i;
  for(i=0;i<size;++i)h=(h^bytes[i])*16777619u;return h;
}
static int rv_range(float x,float lo,float hi){return x>=lo&&x<=hi;}
static int rv_tone_valid(float hz){return hz==0||rv_range(hz,20,20000);}
static int rv_sound_valid(const RVSoundProfile *p){
  const float minFrequency=p->algorithm==4?RV_CHIP_MIN_HZ:p->algorithm==3?65:20;
  const float maxFrequency=p->algorithm==4?RV_CHIP_MAX_HZ:400;
  return (p->flags & ~(3u|RV_CLASSIC_CHIP_FLAG|RV_LOW_CUT_FLAG|RV_BREATH_MASK))==0&&((p->flags&RV_BREATH_MASK)>>RV_BREATH_SHIFT)<=100&&
    p->mode<=2&&p->algorithm<=4&&p->key<12&&p->scale<3&&
    rv_range(p->gain,.25f,64)&&rv_range(p->volume,0,1)&&rv_range(p->frequency,minFrequency,maxFrequency)&&
    rv_range(p->mix,0,1)&&rv_range(p->drive,0,12)&&rv_range(p->gateDb,-90,-25)&&
    rv_range(p->wave,0,1)&&rv_tone_valid(p->toneHz)&&rv_range(p->formant,-6,6)&&
    rv_range(p->clarity,0,1)&&rv_range(p->retuneMs,0,250)&&rv_range(p->strength,0,1)&&rv_range(p->transpose,-12,12);
}
static int rv_name_valid(const char *name){
  size_t i=0;
  if(!name[0])return 0;
  while(i<64&&name[i]){
    unsigned c=(unsigned char)name[i],n=0,j;
    if(c<32||c==127)return 0;
    if(c<128){++i;continue;}
    if(c>=0xc2&&c<=0xdf)n=2;else if(c>=0xe0&&c<=0xef)n=3;else if(c>=0xf0&&c<=0xf4)n=4;else return 0;
    if(i+n>=64)return 0;
    for(j=1;j<n;++j)if(((unsigned char)name[i+j]&0xc0)!=0x80)return 0;
    if((c==0xe0&&(unsigned char)name[i+1]<0xa0)||(c==0xed&&(unsigned char)name[i+1]>=0xa0)||
       (c==0xf0&&(unsigned char)name[i+1]<0x90)||(c==0xf4&&(unsigned char)name[i+1]>=0x90))return 0;
    i+=n;
  }
  return i<64;
}
static void rv_bank_seal(RVPresetBank *b){b->magic=0x52565053u;b->version=1;b->checksum=rv_hash(b,offsetof(RVPresetBank,checksum));}
static int rv_bank_valid(const RVPresetBank *b){
  int i;
  if(b->magic!=0x52565053u||b->version!=1||b->active>=RV_PRESET_COUNT||!b->slots[b->active].used||
     b->checksum!=rv_hash(b,offsetof(RVPresetBank,checksum)))return 0;
  for(i=0;i<RV_PRESET_COUNT;++i)if(b->slots[i].used>1||
    (b->slots[i].used&&(!rv_name_valid(b->slots[i].name)||!rv_sound_valid(&b->slots[i].sound))))return 0;
  return 1;
}
#endif

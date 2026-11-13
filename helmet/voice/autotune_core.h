#ifndef ROBOT_VOICE_AUTOTUNE_CORE_H
#define ROBOT_VOICE_AUTOTUNE_CORE_H
#include <stdint.h>
#include <string.h>
#include <math.h>

/* Monophonic YIN analysis at 8 kHz and pitch-synchronous weighted overlap/add.
 * Fixed, sample-counted 2048/48000 s DSP delay; consonants use aligned dry audio.
 * No allocation in the audio path. C-compatible for testing this exact code.
 * References: de Cheveigne & Kawahara 2002; Moulines & Charpentier 1990.
 */
#define RV_AT_SIZE 4096
#define RV_AT_MASK 4095
#define RV_AT_DELAY 2048
#define RV_AT_PI 3.14159265358979323846f
typedef struct {float b0,b1,b2,a1,a2,z1,z2;} RVATFilter;
typedef struct {uint32_t time;float period;} RVATMark;
typedef struct {
  float input[RV_AT_SIZE],epochs[RV_AT_SIZE],sum[RV_AT_SIZE],weight[RV_AT_SIZE];
  uint8_t voicedMask[RV_AT_SIZE];
  float analysis[512],yin[130],window[1025];
  RVATMark marks[64],pendingMark;
  RVATFilter low[2];
  uint32_t time,filled,lateSamples;
  int decimator,analysisHead,analysisFilled,hop,markHead,markCount;
  int voiced,synthReady,hasSmoothed,key,scale,heldNote,holdValid,grainPending,robot;
  uint32_t nextCenter;
  float fraction,pitchHz,targetHz,confidence,period,retuneMs,strength,transpose,smoothedMidi;
  float blend,dry,robotHz;
} RVAutotune;

static float rvat_clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static int32_t rvat_delta(uint32_t a,uint32_t b){return (int32_t)(a-b);}
static float rvat_filter(RVATFilter *f,float x){
  float y=f->b0*x+f->z1;f->z1=f->b1*x-f->a1*y+f->z2;f->z2=f->b2*x-f->a2*y;return y;
}
static void rvat_init(RVAutotune *s){
  int i;float w=2*RV_AT_PI*1800/48000,c=cosf(w),a=sinf(w)*.70710678f,inv=1/(1+a);
  memset(s,0,sizeof(*s));
  for(i=0;i<2;++i){s->low[i].b0=(1-c)*.5f*inv;s->low[i].b1=2*s->low[i].b0;s->low[i].b2=s->low[i].b0;s->low[i].a1=-2*c*inv;s->low[i].a2=(1-a)*inv;}
  for(i=0;i<=1024;++i)s->window[i]=.5f*(1+cosf(RV_AT_PI*i/1024));
  s->strength=1;s->heldNote=60;s->period=48000.0f/130;
}
static void rvat_configure(RVAutotune *s,int key,int scale,float retuneMs,float strength,float transpose){
  if(key!=s->key||scale!=s->scale)s->holdValid=0;
  s->key=key;s->scale=scale;s->retuneMs=retuneMs;s->strength=strength;s->transpose=transpose;
}
/* Robotization locks the voiced part to one pitch, preserving the original
 * short-term speech waveform/formants instead of substituting a carrier. */
static void rvat_robot_configure(RVAutotune *s,int enabled,float targetHz){
  if((enabled!=0)!=s->robot){s->holdValid=0;s->hasSmoothed=0;}
  s->robot=enabled!=0;s->robotHz=rvat_clamp(targetHz,65,400);
}
static int rvat_allowed(int note,int key,int scale){
  int degree=(note-key)%12;if(degree<0)degree+=12;
  if(scale==0)return 1;
  return ((scale==1?0xAB5:0x5AD)>>degree)&1; /* major / natural minor */
}
static int rvat_note(float midi,int key,int scale){
  int n,nearest=(int)floorf(midi+.5f);float best=1000;
  for(n=(int)floorf(midi)-12;n<=(int)floorf(midi)+12;++n){
    float distance=fabsf(midi-n);
    if(rvat_allowed(n,key,scale)&&distance<best){best=distance;nearest=n;}
  }
  return nearest;
}
static void rvat_analyze(RVAutotune *s){
  float x[384],energy=0,running=0,peak=1e-12f;int16_t quantized[384];int i,tau,chosen=0;
  for(i=0;i<384;++i){x[i]=s->analysis[(s->analysisHead-384+i)&511];if(i<256)energy+=x[i]*x[i];if(fabsf(x[i])>peak)peak=fabsf(x[i]);}
  /* Frame-normalized Q11 differences avoid floating arithmetic in the hot
   * correlation loop. 256*(2*2047)^2 fits uint32_t without overflow. */
  {float scale=2047/peak;for(i=0;i<384;++i)quantized[i]=(int16_t)rvat_clamp(x[i]*scale,-2047,2047);}
  s->yin[0]=1;
  for(tau=1;tau<=125;++tau){
    uint32_t difference=0;
    for(i=0;i<256;++i){int d=quantized[i]-quantized[i+tau];difference+=(uint32_t)(d*d);}
    running+=(float)difference;s->yin[tau]=running>0?(float)difference*tau/running:1;
  }
  for(tau=11;tau<124;++tau){
    if(s->yin[tau]<.15f&&s->yin[tau]<=s->yin[tau-1]&&s->yin[tau]<s->yin[tau+1]){chosen=tau;break;}
  }
  if(!chosen){
    float best=.25f;
    for(tau=11;tau<124;++tau)if(s->yin[tau]<best){best=s->yin[tau];chosen=tau;}
  }
  s->confidence=chosen?1-s->yin[chosen]:0;
  s->voiced=chosen&&energy/256>1e-8f&&s->confidence>.8f;
  s->pitchHz=0;s->targetHz=0;
  if(s->voiced){
    float left=s->yin[chosen-1],middle=s->yin[chosen],right=s->yin[chosen+1];
    float denominator=left-2*middle+right;
    float offset=fabsf(denominator)>1e-12f?.5f*(left-right)/denominator:0;
    float pitch=8000.0f/(chosen+rvat_clamp(offset,-.5f,.5f));
    s->pitchHz=pitch;s->period=rvat_clamp(48000/pitch,64,745);
    if(s->robot)s->targetHz=s->robotHz;
    else {
      float midi=69+12*logf(pitch/440)/logf(2.0f),base=midi+s->transpose,desired;
      int note=rvat_note(base,s->key,s->scale);
      if(s->holdValid&&rvat_allowed(s->heldNote,s->key,s->scale)&&fabsf(base-s->heldNote)<=fabsf(base-note)+.12f)note=s->heldNote;
      s->heldNote=note;s->holdValid=1;
      desired=base+s->strength*(note-base);
      if(!s->hasSmoothed){s->smoothedMidi=desired;s->hasSmoothed=1;}
      else {float follow=s->retuneMs<=0?1:1-expf(-10.0f/s->retuneMs);s->smoothedMidi+=follow*(desired-s->smoothedMidi);}
      s->targetHz=440*powf(2,(s->smoothedMidi-69)/12);
      s->targetHz=rvat_clamp(s->targetHz,pitch*.45f,pitch*2.25f);
    }
  } else {s->markCount=0;s->synthReady=0;s->hasSmoothed=0;s->holdValid=0;s->grainPending=0;}
  /* YIN's effective analysis centre is about 27 ms behind the newest sample.
   * Tag that source segment, so voiced/unvoiced routing matches delayed audio. */
  for(i=0;i<480;++i)s->voicedMask[(s->time-1280-(uint32_t)i)&RV_AT_MASK]=(uint8_t)s->voiced;
}
static uint32_t rvat_peak(const RVAutotune *s,uint32_t centre,int radius){
  int i;uint32_t best=centre;float maximum=-1e20f;
  for(i=-radius;i<=radius;++i){uint32_t p=centre+(uint32_t)i;float v=s->epochs[p&RV_AT_MASK];if(v>maximum){maximum=v;best=p;}}
  return best;
}
static void rvat_add_mark(RVAutotune *s,uint32_t time){
  s->marks[s->markHead].time=time;s->marks[s->markHead].period=s->period;
  s->markHead=(s->markHead+1)&63;if(s->markCount<64)++s->markCount;
}
static void rvat_marks(RVAutotune *s){
  uint32_t last;int radius=(int)(s->period*.2f),guard=0;
  if(!s->markCount){
    uint32_t first=rvat_peak(s,s->time-(uint32_t)(2*s->period),(int)(s->period*.45f));
    rvat_add_mark(s,first);
    s->nextCenter=first+RV_AT_DELAY;
    // Low voices can acquire their first mark so far in the past that the
    // left half of the first output grain would precede the current sample.
    const uint32_t earliest=s->time+(uint32_t)(s->period+1);
    if(rvat_delta(s->nextCenter,earliest)<0)s->nextCenter=earliest;
    s->fraction=0;s->synthReady=1;
  }
  last=s->marks[(s->markHead-1)&63].time;
  while(++guard<32){
    uint32_t predicted=last+(uint32_t)(s->period+.5f);
    /* Record a peak as soon as its search interval is available. The synthesis
     * stage separately waits for the selected source grain's right half. */
    if(rvat_delta(s->time,predicted)<radius+4)break;
    last=rvat_peak(s,predicted,radius);rvat_add_mark(s,last);
  }
}
static float rvat_window(const RVAutotune *s,float phase){
  float position=fabsf(phase)*1024;int index=(int)position;
  if(index>=1024)return 0;
  return s->window[index]+(position-index)*(s->window[index+1]-s->window[index]);
}
static void rvat_synthesize(RVAutotune *s){
  int guard=0;
  if(!s->synthReady||!s->markCount||s->targetHz<=0)return;
  while(++guard<32){
    RVATMark latest=s->marks[(s->markHead-1)&63],source=s->pendingMark;
    uint32_t target=s->nextCenter-RV_AT_DELAY;int k,best=0x7fffffff,extent;float windowPeriod;
    if(!s->grainPending){
      if(rvat_delta(latest.time,target)<(int)(s->period*.55f))break;
      for(k=0;k<s->markCount;++k){RVATMark candidate=s->marks[(s->markHead-1-k)&63];int distance=rvat_delta(candidate.time,target);if(distance<0)distance=-distance;if(distance<best){best=distance;source=candidate;}}
      s->pendingMark=source;s->grainPending=1;
    }
    /* Keep the source-period envelope: extending it to the target period can
     * preserve the old pitch when the two pitches are integer multiples. Robot
     * synthesis deliberately keeps a pulse envelope at the target frequency. */
    windowPeriod=source.period;
    extent=(int)(windowPeriod+1);
    if(rvat_delta(s->time,source.time)<extent+2)break;
    for(k=-extent;k<=extent;++k){
      uint32_t output=s->nextCenter+(uint32_t)k;int32_t ahead=rvat_delta(output,s->time);
      float w=rvat_window(s,(k-s->fraction)/windowPeriod);
      if(w<=0)continue;
      if(ahead<0||ahead>=RV_AT_SIZE){++s->lateSamples;continue;}
      s->sum[output&RV_AT_MASK]+=s->input[(source.time+(uint32_t)k)&RV_AT_MASK]*w;
      s->weight[output&RV_AT_MASK]+=w;
    }
    s->grainPending=0;
    {float spacing=48000/s->targetHz+s->fraction;uint32_t whole=(uint32_t)spacing;s->nextCenter+=whole;s->fraction=spacing-whole;}
  }
}
static float rvat_tick(RVAutotune *s,float input,int active){
  uint32_t index=s->time&RV_AT_MASK;float low=rvat_filter(&s->low[1],rvat_filter(&s->low[0],input));
  float output,weight,blendTarget;
  s->input[index]=input;s->epochs[index]=low;s->voicedMask[index]=0;
  if(s->filled<RV_AT_SIZE)++s->filled;
  if(++s->decimator==6){
    s->decimator=0;s->analysis[s->analysisHead]=low;s->analysisHead=(s->analysisHead+1)&511;
    if(s->analysisFilled<512)++s->analysisFilled;
    if(++s->hop>=80){s->hop=0;if(active&&s->analysisFilled>=384)rvat_analyze(s);}
  }
  if(active&&s->voiced){rvat_marks(s);rvat_synthesize(s);}
  if(!active){s->voiced=0;s->markCount=0;s->synthReady=0;s->pitchHz=s->targetHz=s->confidence=0;s->hasSmoothed=0;s->holdValid=0;s->grainPending=0;}
  s->dry=s->filled>RV_AT_DELAY?s->input[(s->time-RV_AT_DELAY)&RV_AT_MASK]:0;
  weight=s->weight[index];
  /* A low robot pitch leaves gaps between short grains. Those are silent voiced
   * gaps, not missing data: dry fallback would reintroduce the original pitch.
   * Keep the window envelope below unit overlap to avoid hard pulse edges. */
  output=s->robot?s->sum[index]/(weight>1?weight:1):(weight>.05f?s->sum[index]/weight:s->dry);
  blendTarget=active&&(s->robot||weight>.1f)&&s->voicedMask[(s->time-RV_AT_DELAY)&RV_AT_MASK]?1:0;
  s->blend+=.004f*(blendTarget-s->blend);
  s->sum[index]=s->weight[index]=0;
  if(!s->robot&&s->strength==0&&s->transpose==0)output=s->dry;
  else output=s->dry+s->blend*(output-s->dry);
  ++s->time;return output;
}
#endif

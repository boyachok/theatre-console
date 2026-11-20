#pragma once
#include <cmath>
#include "score_wire.h"
#include "voice/voice_settings.h"
#include "voice/audio_effects.h"
#include "voice/audio_protection.h"
#include "voice/voice_tone.h"
#include "voice/inmp441_decode.h"
namespace Helmet {
inline Settings soundSettings(const Show::VoiceProfile &p,bool enabled){Settings s;s.enabled=enabled;if(!p.configured)return s;s.mode=p.mode?ROBOT:DRY;s.vocoder=p.algorithm==1;s.autotune=p.algorithm==2;s.monotone=p.algorithm==3;s.speechChip=p.algorithm==4;s.gain=p.gain;s.volume=p.volume;s.frequency=p.frequency;s.mix=p.mix;s.drive=p.drive;s.gateDb=p.gateDb;s.notch=p.flags&1;s.reduceThumps=p.flags&2;s.breathReduction=p.breath;s.wave=p.wave;s.toneHz=p.toneHz;s.formant=p.formant;s.clarity=p.clarity;s.tuneKey=p.key;s.tuneScale=p.scale;s.retuneMs=p.retuneMs;s.tuneStrength=p.strength;s.transpose=p.transpose;return s;}
struct VoiceStats {bool ready=false,enabled=false;uint8_t audioCore=255;uint32_t captured=0,played=0,errors=0,late=0,limited=0,workMax=0;float inputLevel=0,outputLevel=0,pitch=0,target=0,load=0,gateEnvelopeDb=-180,gateGain=0,gateDb=-90;};
// Large DSP buffers are members of a static/global Runner, never audio task stack.
class VoiceProcessor {
 VoiceEffects effects;InputLowCut lowCut;OutputPeakLimiter limiter;VoiceToneDSP::Processor tone;
 float lp=0,nx1=0,nx2=0,ny1=0,ny2=0,envelope=0,gate=0,volume=0,gain=2,notchMix=1;
public:
 void process(const Settings &c,const uint32_t *slots,int32_t *output,size_t frames,VoiceStats &stats){
  effects.configure(c);tone.configure(c.tone);const float pi=3.14159265358979323846f,omega=2*pi*50/48000,alpha=sinf(omega)/16;
  const float b0=1/(1+alpha),b1=-2*cosf(omega)/(1+alpha),b2=b0,a1=b1,a2=(1-alpha)/(1+alpha),lpAlpha=1-expf(-2*pi*6000/48000),threshold=powf(10,c.gateDb/20);
  float power=0,outPower=0;
  for(size_t i=0;i<frames;i++){
   float input=normalizeInmp441(slots[2*i]),hp=lowCut.tick(input,c.reduceThumps),notched=b0*hp+b1*nx1+b2*nx2-a1*ny1-a2*ny2;nx2=nx1;nx1=hp;ny2=ny1;ny1=notched;
   notchMix+=.004f*((c.notch?1.f:0.f)-notchMix);lp+=lpAlpha*(hp+notchMix*(notched-hp)-lp);power+=lp*lp;
   float magnitude=fabsf(lp);envelope+=(magnitude>envelope?.004f:.00025f)*(magnitude-envelope);gate+=.004f*((c.gateDb<=-89||envelope>=threshold?1.f:0.f)-gate);gain+=.004f*(c.gain-gain);volume+=.004f*((c.enabled?c.volume:0)-volume);
   float signal=limiter.tick(tone.tick(effects.tick(lp*gain,gate),hp*gain*gate)*volume);if(limiter.limiting())stats.limited++;if(!std::isfinite(signal)){signal=0;stats.errors++;}signal=fmaxf(-.98f,fminf(.98f,signal));outPower+=signal*signal;
   output[2*i]=output[2*i+1]=(int32_t)(signal*2147483647.f);
  }
  stats.gateEnvelopeDb=20*log10f(fmaxf(envelope,1e-9f));stats.gateGain=gate;stats.gateDb=c.gateDb;
  stats.inputLevel=sqrtf(power/frames)*c.gain;stats.outputLevel=sqrtf(outPower/frames);stats.pitch=effects.tune.pitchHz;stats.target=effects.tune.targetHz;stats.enabled=c.enabled;
 }
};
}
#ifdef THEATRE_HOST_TEST
namespace Helmet {
class VoicePort {
 bool desired=false;Show::VoiceProfile profile{};Show::VoiceTone tone{};
public:
 void begin(){}bool configure(const Show::VoiceProfile &p,bool enabled,const Show::VoiceTone &t=Show::VoiceTone{}){if(!Show::validVoice(p)||!Show::validTone(t))return false;profile=p;tone=t;desired=enabled;return true;}
 Show::VoiceProfile appliedProfile()const{return profile;}
 Show::VoiceTone appliedTone()const{return tone;}
 void setEnabled(bool e){desired=e;}bool requested()const{return desired;}bool available()const{return true;}float level()const{return 0;}VoiceStats diagnostics()const{VoiceStats s;s.ready=true;s.enabled=desired;return s;}
};
}
#else
#include <driver/i2s.h>
#include <driver/gpio.h>
#include <esp_timer.h>
namespace Helmet {
class VoicePort {
 portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;Settings settings;Show::VoiceProfile profile{};VoiceStats stats;VoiceProcessor processor;TaskHandle_t task=nullptr;QueueHandle_t events=nullptr;
 static void entry(void *p){((VoicePort*)p)->work();}
 void work(){
  uint32_t input[512]{};int32_t output[512]{};VoiceStats local;local.ready=true;local.audioCore=xPortGetCoreID();
  for(;;){
   portENTER_CRITICAL(&mux);Settings control=settings;portEXIT_CRITICAL(&mux);size_t received=0;
   auto result=i2s_read(I2S_NUM_0,input,sizeof(input),&received,pdMS_TO_TICKS(100));received=result==ESP_OK?std::min(received,sizeof(input))/8*8:0;if(received!=sizeof(input)){local.errors++;memset((uint8_t*)input+received,0,sizeof(input)-received);}
   uint64_t begin=esp_timer_get_time();processor.process(control,input,output,256,local);uint32_t work=esp_timer_get_time()-begin;local.workMax=std::max(local.workMax,work);if(work>5333)local.late++;local.load=.95f*local.load+.05f*(work/53.333f);local.captured+=received/8;
   i2s_event_t event;while(xQueueReceive(events,&event,0)==pdTRUE)if(local.captured>=12000&&(event.type==I2S_EVENT_TX_Q_OVF||event.type==I2S_EVENT_RX_Q_OVF||event.type==I2S_EVENT_DMA_ERROR))local.errors++;
   size_t offset=0;while(offset<sizeof(output)){size_t sent=0;auto error=i2s_write(I2S_NUM_0,(uint8_t*)output+offset,sizeof(output)-offset,&sent,pdMS_TO_TICKS(100));offset+=sent;if(error!=ESP_OK||!sent){local.errors++;break;}}local.played+=offset/8;
   portENTER_CRITICAL(&mux);stats=local;portEXIT_CRITICAL(&mux);
  }
 }
public:
 void begin(){
  i2s_config_t config{};config.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_RX);config.sample_rate=48000;config.bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT;config.bits_per_chan=I2S_BITS_PER_CHAN_32BIT;config.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;config.communication_format=I2S_COMM_FORMAT_STAND_I2S;config.dma_buf_count=4;config.dma_buf_len=256;config.tx_desc_auto_clear=true;
  if(i2s_driver_install(I2S_NUM_0,&config,32,&events)!=ESP_OK)return;
  i2s_pin_config_t pins{};pins.mck_io_num=I2S_PIN_NO_CHANGE;pins.bck_io_num=4;pins.ws_io_num=5;pins.data_out_num=6;pins.data_in_num=7;
  if(i2s_set_pin(I2S_NUM_0,&pins)!=ESP_OK||gpio_set_pull_mode(GPIO_NUM_7,GPIO_PULLDOWN_ONLY)!=ESP_OK||i2s_zero_dma_buffer(I2S_NUM_0)!=ESP_OK){i2s_driver_uninstall(I2S_NUM_0);return;}
  if(xTaskCreatePinnedToCore(entry,"helmet-audio",16384,this,6,&task,1)!=pdPASS){i2s_driver_uninstall(I2S_NUM_0);return;}stats.ready=true;
 }
 bool configure(const Show::VoiceProfile &p,bool enabled,const Show::VoiceTone &t=Show::VoiceTone{}){if(!Show::validVoice(p)||!Show::validTone(t))return false;portENTER_CRITICAL(&mux);profile=p;settings=soundSettings(p,enabled);settings.tone=t;portEXIT_CRITICAL(&mux);return true;}
 void setEnabled(bool enabled){portENTER_CRITICAL(&mux);settings.enabled=enabled;portEXIT_CRITICAL(&mux);}
 Show::VoiceTone appliedTone(){portENTER_CRITICAL(&mux);auto p=settings.tone;portEXIT_CRITICAL(&mux);return p;}
 Show::VoiceProfile appliedProfile(){portENTER_CRITICAL(&mux);auto p=profile;portEXIT_CRITICAL(&mux);return p;}
 bool requested(){portENTER_CRITICAL(&mux);bool e=settings.enabled;portEXIT_CRITICAL(&mux);return e;}
 bool available(){portENTER_CRITICAL(&mux);bool r=stats.ready;portEXIT_CRITICAL(&mux);return r;}
 VoiceStats diagnostics(){portENTER_CRITICAL(&mux);auto s=stats;portEXIT_CRITICAL(&mux);return s;}
 float level(){return diagnostics().inputLevel;}
};
}
#endif

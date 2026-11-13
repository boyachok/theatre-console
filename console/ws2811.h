#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <esp32-hal-rmt.h>
#include <esp_heap_caps.h>
#include <cmath>
#include "hardware.h"
#include "score_wire.h"
#include "strip_animation.h"
namespace Show {
// RMT sends asynchronously. PWM fades, relay deadlines and UART never wait for a strip frame.
class Ws2811 {
 uint8_t pin; rmt_data_t *symbols=nullptr;uint16_t count=0;bool ready=false,playing=false,dirty=true,heard=false;
 uint32_t started=0,lastFrame=0,lastVoice=0;float voiceEnvelope=0;Strip effect{};StripOptions options{};Preferences settings;
 bool resize(uint16_t n){
  if(n==count)return true;if(!n||n>1024||!rmtTransmitCompleted(pin))return false;
  auto *next=(rmt_data_t*)heap_caps_malloc((n*24+1)*sizeof(rmt_data_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);if(!next)return false;
  if(settings.getUShort("stripCount",0)!=n&&settings.putUShort("stripCount",n)!=sizeof(uint16_t)){heap_caps_free(next);return false;}
  heap_caps_free(symbols);symbols=next;count=n;return true;
 }
public:
 explicit Ws2811(uint8_t dataPin=STRIP_PIN):pin(dataPin){}
 ~Ws2811(){if(symbols&&rmtTransmitCompleted(pin))heap_caps_free(symbols);}
 bool begin(){
  settings.begin("stage-io",false);digitalWrite(pin,LOW);pinMode(pin,OUTPUT);
  ready=rmtInit(pin,RMT_TX_MODE,RMT_MEM_NUM_BLOCKS_2,20000000);if(!ready)return false;
  uint16_t n=settings.getUShort("stripCount",60);if(!n||n>1024)n=60;
  if(!resize(n))return false;tick(millis());return true;
 }
 void stop(){playing=false;dirty=true;voiceEnvelope=0;lastVoice=0;heard=false;}
 bool apply(const Strip &next,uint32_t now,bool restart,const StripOptions &extra=StripOptions{}){
  if(!next.enabled||next.action==1){stop();return true;}if(next.action==0)return true;
  if(!ready||!resize(next.count))return false;
  if(!playing||restart||memcmp(&effect,&next,sizeof(next))||memcmp(&options,&extra,sizeof(extra))){effect=next;options=extra;started=now;dirty=true;}
  playing=true;return true;
 }
 void tick(uint32_t now,float voiceLevel=-1){
  if(playing&&options.reactive){float threshold=std::pow(10.f,(-60.f+options.sensitivity*.4f)/20);if(voiceLevel>=threshold){voiceEnvelope=1;lastVoice=now;heard=true;}else voiceEnvelope=heard&&options.releaseMs?std::max(0.f,1-(float)(uint32_t)(now-lastVoice)/options.releaseMs):0;}
  if(!ready||!symbols||!rmtTransmitCompleted(pin))return;
  if(!dirty&&(!playing||(!options.reactive&&(effect.mode==2||(effect.mode==1&&lastFrame-started>=effect.duration)))))return;
  if(!dirty&&now-lastFrame<33)return;
  size_t pos=0;for(uint16_t i=0;i<count;i++){
   uint8_t rgb[3]{};if(playing)stripPixel(effect,options,i,count,(uint32_t)(now-started),options.reactive?voiceEnvelope:1.f,rgb);
   if(effect.order)std::swap(rgb[0],rgb[1]);for(auto byte:rgb)for(int bit=7;bit>=0;bit--){bool one=byte&(1<<bit);auto &s=symbols[pos++];s.level0=1;s.duration0=(one?13:6)*(effect.slow?2:1);s.level1=0;s.duration1=(one?12:19)*(effect.slow?2:1);}
  }
  symbols[pos].level0=0;symbols[pos].duration0=6000;symbols[pos].level1=0;symbols[pos].duration1=6000;
  if(rmtWriteAsync(pin,symbols,pos+1)){dirty=false;lastFrame=now;}
 }
};
}

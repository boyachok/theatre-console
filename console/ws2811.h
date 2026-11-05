#pragma once
#include <esp32-hal-rmt.h>
#include <esp_heap_caps.h>
#include <cmath>
#include "hardware.h"
#include "score_wire.h"
namespace Show {
// RMT sends asynchronously. PWM fades, relay deadlines and UART never wait for a strip frame.
class Ws2811 {
 uint8_t pin; rmt_data_t *symbols=nullptr;uint16_t count=0;bool ready=false,playing=false,dirty=true;
 uint32_t started=0,lastFrame=0;Strip effect{};Preferences settings;
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
 void stop(){playing=false;dirty=true;}
 bool apply(const Strip &next,uint32_t now,bool restart){
  if(!next.enabled||next.action==1){stop();return true;}if(next.action==0)return true;
  if(!ready||!resize(next.count))return false;
  if(!playing||restart||memcmp(&effect,&next,sizeof(next))){effect=next;started=now;dirty=true;}
  playing=true;return true;
 }
 void tick(uint32_t now){
  if(!ready||!symbols||!rmtTransmitCompleted(pin))return;
  if(!dirty&&(!playing||effect.mode==2||(effect.mode==1&&lastFrame-started>=effect.duration)))return;
  if(!dirty&&now-lastFrame<33)return;
  float progress=effect.duration?(float)(uint32_t)(now-started)/effect.duration:1.f;
  size_t pos=0;for(uint16_t i=0;i<count;i++){
   float v=0;if(playing){
    if(effect.mode==2)v=1;
    else if(effect.mode==1){uint16_t index=effect.direction?count-1-i:i;v=index<std::min(1.f,progress)*count?1:0;}
    else {float center=std::fmod(progress,1.f)*count;if(effect.direction)center=count-center;float d=std::fabs(i-center);d=std::min(d,count-d);float width=std::max(1.f,count*effect.width/100.f);v=std::max(0.f,1.f-d/(width/2));}
   }
   uint8_t rgb[3]{};if(playing){rgb[0]=(effect.br+(effect.r-effect.br)*v)*effect.brightness/100;rgb[1]=(effect.bg+(effect.g-effect.bg)*v)*effect.brightness/100;rgb[2]=(effect.bb+(effect.b-effect.bb)*v)*effect.brightness/100;}
   if(effect.order)std::swap(rgb[0],rgb[1]);for(auto byte:rgb)for(int bit=7;bit>=0;bit--){bool one=byte&(1<<bit);auto &s=symbols[pos++];s.level0=1;s.duration0=(one?13:6)*(effect.slow?2:1);s.level1=0;s.duration1=(one?12:19)*(effect.slow?2:1);}
  }
  symbols[pos].level0=0;symbols[pos].duration0=6000;symbols[pos].level1=0;symbols[pos].duration1=6000;
  if(rmtWriteAsync(pin,symbols,pos+1)){dirty=false;lastFrame=now;}
 }
};
}

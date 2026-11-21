#pragma once
#include <Arduino.h>
#include "hardware.h"
#include "ac_phase.h"
#include "ac_pulse_store.h"
#if defined(THEATRE_HOST_TEST) && !defined(THEATRE_AC_BACKEND_TEST)
class AcDimmer {
 Mains::Phase phase;AcPulse::Store pulseConfig;
public:
 void begin(){digitalWrite(AC_GATE_PIN,LOW);pinMode(AC_GATE_PIN,OUTPUT);pinMode(AC_ZC_PIN,INPUT);pulseConfig.begin();phase.configurePulse(pulseConfig.pulseUs);}
 bool pulseConfigFault()const{return pulseConfig.fault;}
 Result savePulse(uint16_t us){auto s=phase.state();if(!AcPulse::valid(us))return INVALID;if(s.requested||s.paired)return NOT_READY;auto result=pulseConfig.save(us);if(result==APPLIED&&!phase.configurePulse(us))return IO_ERROR;return result;}
 bool monitor(){return configure(true,false);}
 bool configure(bool on,bool rearm){if(!on){off();return true;}if(pulseConfig.fault){phase.trip(Mains::RESOURCE);return false;}if(rearm||!phase.state().enabled)phase.arm((uint64_t)millis()*1000);return true;}
 void off(){phase.level(0);}
 void level(uint8_t v){phase.level(v);}
 void tick(bool lease){phase.controller((uint64_t)millis()*1000,lease);phase.watchdog((uint64_t)millis()*1000);}
 void stop(){phase.disable();}
 Mains::State diagnostics()const{return phase.state();}
 bool zeroForTest(uint64_t at,Mains::Pulse &pulse){return phase.zeroCross(at,pulse);}
};
#else
#ifdef THEATRE_AC_BACKEND_TEST
#include "ac_io_mock.h"
#else
#include <esp_timer.h>
#include <esp_intr_alloc.h>
#include <esp32-hal-rmt.h>
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#include <soc/gpio_struct.h>
#include <hal/gpio_ll.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif
struct AcIncident {Mains::Event event{};uint64_t lastAccepted=0,lastRaw=0;uint32_t requested=0,good=0,rejected=0,rawEdges=0,rawPeriod=0;};
struct AcTelemetry {
 uint32_t periods[64]{},incidentPeriods[64]{};uint8_t count=0,incidentCount=0;
 uint64_t lastRaw=0;uint32_t rawEdges=0,submits=0,lastSubmitLatency=0,maxSubmitLatency=0,lastSubmitDuration=0,maxSubmitDuration=0;
 uint32_t highUs=0,lowUs=0;AcIncident incident{};AcIncident events[8]{};uint8_t eventsCount=0;
};
class AcDimmer {
 Mains::Phase phase;AcPulse::Store pulseConfig;portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;TaskHandle_t task=nullptr;SemaphoreHandle_t io=nullptr;
 uint64_t rawAt=0,fallAt=0;uint32_t rawPeriods[128]{},rawEdges=0,highUs=0,lowUs=0,submits=0,lastSubmitLatency=0,maxSubmitLatency=0,lastSubmitDuration=0,maxSubmitDuration=0;uint8_t rawHead=0,rawSize=0;
 AcIncident incident{},events[8]{};uint8_t eventHead=0,eventSize=0,incidentSize=0;uint32_t incidentPeriods[64]{},seenEvent=0;
 Mains::Pulse latest{};bool pending=false,ready=false,attached=false,mapped=false,irqReady=false;uint32_t signal=SIG_GPIO_OUT_IDX;rmt_data_t symbols[2]{};
 void gateLow();void record();static void edge(void *arg);
 void fail(Mains::Fault fault,Mains::Reason reason=Mains::RESOURCE_ERROR){phase.trip(fault,esp_timer_get_time(),reason);gateLow();pending=false;record();}
 static void worker(void *arg){((AcDimmer*)arg)->work();}
 void work(){for(;;){ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(5));service();}}
 void service(){
  Mains::Pulse p;uint64_t now;bool send=false;
  portENTER_CRITICAL(&mux);now=esp_timer_get_time();phase.watchdog(now);if(phase.outputBlocked())gateLow();if(pending){p=latest;pending=false;send=phase.authorize(p,now);if(!send)gateLow();}record();portEXIT_CRITICAL(&mux);
  if(!send)return;
  xSemaphoreTake(io,portMAX_DELAY);
  portENTER_CRITICAL(&mux);now=esp_timer_get_time();send=phase.authorize(p,now);if(!send)gateLow();record();portEXIT_CRITICAL(&mux);
  if(send){
   if(!rmtTransmitCompleted(AC_GATE_PIN)){portENTER_CRITICAL(&mux);phase.skip(Mains::TX_BUSY,now,(uint32_t)(now-p.zero));gateLow();record();portEXIT_CRITICAL(&mux);}
   else{
    portENTER_CRITICAL(&mux);if(!mapped){esp_rom_gpio_connect_out_signal(AC_GATE_PIN,signal,false,false);mapped=true;}portEXIT_CRITICAL(&mux);
    now=esp_timer_get_time();uint32_t wait=p.fire>now?(uint32_t)(p.fire-now):1;
    symbols[0].level0=0;symbols[0].duration0=wait;symbols[0].level1=1;symbols[0].duration1=p.pulseUs;
    symbols[1].level0=0;symbols[1].duration0=100;symbols[1].level1=0;symbols[1].duration1=1;
    bool ok=rmtWriteAsync(AC_GATE_PIN,symbols,2);uint64_t after=esp_timer_get_time();
    portENTER_CRITICAL(&mux);
    lastSubmitDuration=(uint32_t)(after-now);if(lastSubmitDuration>maxSubmitDuration)maxSubmitDuration=lastSubmitDuration;
    if(ok){submits++;lastSubmitLatency=(uint32_t)(after-p.zero);if(lastSubmitLatency>maxSubmitLatency)maxSubmitLatency=lastSubmitLatency;}
    if(!ok)fail(Mains::RESOURCE,Mains::TX_REJECT);
    else if(after-now>150){phase.skip(Mains::TX_SLOW,after,(uint32_t)(after-now));gateLow();}
    else if(!phase.authorize(p,esp_timer_get_time()))gateLow();
    record();portEXIT_CRITICAL(&mux);
   }
  }
  xSemaphoreGive(io);
 }
public:
#ifdef THEATRE_AC_BACKEND_TEST
 void testService(){service();}
#endif
 void begin(){digitalWrite(AC_GATE_PIN,LOW);pinMode(AC_GATE_PIN,OUTPUT);pinMode(AC_ZC_PIN,INPUT);pulseConfig.begin();phase.configurePulse(pulseConfig.pulseUs);}
 bool pulseConfigFault()const{return pulseConfig.fault;}
 Result savePulse(uint16_t us){
  if(!AcPulse::valid(us))return INVALID;
  if(io)xSemaphoreTake(io,portMAX_DELAY);
  portENTER_CRITICAL(&mux);auto s=phase.state();bool quiet=!s.requested&&!s.paired;if(quiet){pending=false;gateLow();}portEXIT_CRITICAL(&mux);
  if(io)xSemaphoreGive(io);if(!quiet)return NOT_READY;
  auto result=pulseConfig.save(us);if(result!=APPLIED)return result;
  portENTER_CRITICAL(&mux);bool ok=phase.configurePulse(us);if(!ok)fail(Mains::RESOURCE);portEXIT_CRITICAL(&mux);return ok?APPLIED:IO_ERROR;
 }
 bool configure(bool on,bool rearm){
  if(!on){off();return true;}if(pulseConfig.fault){portENTER_CRITICAL(&mux);fail(Mains::RESOURCE);portEXIT_CRITICAL(&mux);return false;}if(!io)io=xSemaphoreCreateMutex();
  if(!io){portENTER_CRITICAL(&mux);fail(Mains::RESOURCE);portEXIT_CRITICAL(&mux);return false;}
  portENTER_CRITICAL(&mux);bool reconnect=rearm&&phase.state().fault==Mains::RESOURCE;portEXIT_CRITICAL(&mux);
  if(!ready||reconnect){
   xSemaphoreTake(io,portMAX_DELAY);if(ready)rmtDeinit(AC_GATE_PIN);
   ready=rmtInit(AC_GATE_PIN,RMT_TX_MODE,RMT_MEM_NUM_BLOCKS_1,1000000)&&rmtSetEOT(AC_GATE_PIN,0);
   if(ready&&!task)ready=xTaskCreatePinnedToCore(worker,"ac-phase",3072,this,20,&task,1)==pdPASS;
   portENTER_CRITICAL(&mux);if(!ready)fail(Mains::RESOURCE);else{mapped=true;signal=GPIO.func_out_sel_cfg[AC_GATE_PIN].func_sel;}portEXIT_CRITICAL(&mux);xSemaphoreGive(io);if(!ready)return false;
  }
  portENTER_CRITICAL(&mux);if(rearm||!phase.state().enabled)phase.arm(esp_timer_get_time());portEXIT_CRITICAL(&mux);
  if(!irqReady){if(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)!=ESP_OK){portENTER_CRITICAL(&mux);fail(Mains::RESOURCE);portEXIT_CRITICAL(&mux);return false;}irqReady=true;}
  if(!attached){if(gpio_set_intr_type((gpio_num_t)AC_ZC_PIN,GPIO_INTR_ANYEDGE)!=ESP_OK||gpio_isr_handler_add((gpio_num_t)AC_ZC_PIN,edge,this)!=ESP_OK){portENTER_CRITICAL(&mux);fail(Mains::RESOURCE);portEXIT_CRITICAL(&mux);return false;}attached=true;}return true;
 }
 bool monitor(){return configure(true,false);}
 void off(){level(0);}
 // Serialize cancellation against the worker's authorize->matrix-map->RMT-submit window.
 void level(uint8_t v){if(!v&&io)xSemaphoreTake(io,portMAX_DELAY);portENTER_CRITICAL(&mux);phase.level(v);if(!v){pending=false;if(mapped)gateLow();}portEXIT_CRITICAL(&mux);if(!v&&io)xSemaphoreGive(io);}
 void tick(bool lease){portENTER_CRITICAL(&mux);phase.controller(esp_timer_get_time(),lease);if(phase.outputBlocked())gateLow();record();portEXIT_CRITICAL(&mux);}
 void stop(){if(io)xSemaphoreTake(io,portMAX_DELAY);if(attached){gpio_intr_disable((gpio_num_t)AC_ZC_PIN);gpio_isr_handler_remove((gpio_num_t)AC_ZC_PIN);attached=false;}portENTER_CRITICAL(&mux);phase.disable();pending=false;gateLow();portEXIT_CRITICAL(&mux);if(io)xSemaphoreGive(io);}
 Mains::State diagnostics(){portENTER_CRITICAL(&mux);auto s=phase.state();portEXIT_CRITICAL(&mux);return s;}
 AcTelemetry telemetry(){AcTelemetry out;portENTER_CRITICAL(&mux);out.count=rawSize<64?rawSize:64;for(uint8_t i=0;i<out.count;i++)out.periods[i]=rawPeriods[(rawHead+128-out.count+i)&127];out.incidentCount=incidentSize;for(uint8_t i=0;i<incidentSize;i++)out.incidentPeriods[i]=incidentPeriods[i];out.lastRaw=rawAt;out.rawEdges=rawEdges;out.submits=submits;out.lastSubmitLatency=lastSubmitLatency;out.maxSubmitLatency=maxSubmitLatency;out.lastSubmitDuration=lastSubmitDuration;out.maxSubmitDuration=maxSubmitDuration;out.highUs=highUs;out.lowUs=lowUs;out.incident=incident;out.eventsCount=eventSize;for(uint8_t i=0;i<eventSize;i++)out.events[i]=events[(eventHead+8-eventSize+i)&7];portEXIT_CRITICAL(&mux);return out;}
};
void IRAM_ATTR AcDimmer::gateLow(){gpio_ll_set_level(&GPIO,AC_GATE_PIN,0);esp_rom_gpio_connect_out_signal(AC_GATE_PIN,SIG_GPIO_OUT_IDX,false,false);mapped=false;}
void IRAM_ATTR AcDimmer::record(){
 auto ev=phase.event();if(ev.id==seenEvent)return;seenEvent=ev.id;auto s=phase.state();AcIncident e;e.event=ev;e.lastAccepted=s.lastZero;if(ev.reason==Mains::ZC_GAP||ev.reason==Mains::ZC_TIMEOUT)e.lastAccepted=ev.at-ev.intervalUs;e.lastRaw=rawAt;e.requested=s.requested;e.good=s.good;e.rejected=s.rejected;e.rawEdges=rawEdges;e.rawPeriod=rawSize?rawPeriods[(rawHead+127)&127]:0;
 events[eventHead]=e;eventHead=(eventHead+1)&7;if(eventSize<8)eventSize++;
 if(ev.reason!=Mains::ACQUIRED){incident=e;incidentSize=rawSize<64?rawSize:64;for(uint8_t i=0;i<incidentSize;i++)incidentPeriods[i]=rawPeriods[(rawHead+128-incidentSize+i)&127];}
}
void IRAM_ATTR AcDimmer::edge(void *arg){
 auto *self=(AcDimmer*)arg;Mains::Pulse p;bool notify=false;
 portENTER_CRITICAL_ISR(&self->mux);uint64_t now=esp_timer_get_time();bool rising=gpio_ll_get_level(&GPIO,AC_ZC_PIN);
 if(rising){
  if(self->rawAt){uint64_t delta=now-self->rawAt;self->rawPeriods[self->rawHead]=(uint32_t)(delta>0xffffffffULL?0xffffffffULL:delta);self->rawHead=(self->rawHead+1)&127;if(self->rawSize<128)self->rawSize++;}self->rawAt=now;self->rawEdges++;
  if(self->fallAt)self->lowUs=(uint32_t)(now-self->fallAt);
  if(self->phase.zeroCross(now,p)){if(self->pending)self->phase.skip(Mains::PENDING_REPLACED,now,(uint32_t)(now-self->latest.zero));self->latest=p;self->pending=true;notify=true;}
 }else{self->fallAt=now;if(self->rawAt)self->highUs=(uint32_t)(now-self->rawAt);}
 if(self->phase.outputBlocked()){self->gateLow();self->pending=false;}self->record();portEXIT_CRITICAL_ISR(&self->mux);
 if(notify){BaseType_t wake=pdFALSE;vTaskNotifyGiveFromISR(self->task,&wake);if(wake)portYIELD_FROM_ISR();}
}

#endif

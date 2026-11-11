#pragma once
#include <Arduino.h>
#include "hardware.h"
#include "ac_phase.h"
#if defined(THEATRE_HOST_TEST) && !defined(THEATRE_AC_BACKEND_TEST)
class AcDimmer {
 Mains::Phase phase;
public:
 void begin(){digitalWrite(AC_GATE_PIN,LOW);pinMode(AC_GATE_PIN,OUTPUT);pinMode(AC_ZC_PIN,INPUT);}
 bool configure(bool on,bool rearm){if(!on)phase.disable();else if(rearm||!phase.state().enabled)phase.arm((uint64_t)millis()*1000);return true;}
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
// One non-looping RMT TX block: LOW phase delay -> HIGH 200us -> LOW EOT, hardware timed.
// STOP/fault disconnects the RMT GPIO matrix and drives LOW; the TRIAC itself opens only at current zero.
class AcDimmer {
 Mains::Phase phase;portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;TaskHandle_t task=nullptr;SemaphoreHandle_t io=nullptr;
 Mains::Pulse latest{};bool pending=false,ready=false,attached=false,mapped=false,irqReady=false;uint32_t signal=SIG_GPIO_OUT_IDX;rmt_data_t symbols[2]{};
 void gateLow();
 void fail(Mains::Fault fault){phase.trip(fault);gateLow();pending=false;}
 static void edge(void *arg);
 static void worker(void *arg){((AcDimmer*)arg)->work();}
 void work(){for(;;){ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(5));service();}}
 void service(){
  Mains::Pulse p;uint64_t now=esp_timer_get_time();bool send=false;
  portENTER_CRITICAL(&mux);phase.watchdog(now);if(phase.state().latched)gateLow();if(pending){p=latest;pending=false;send=phase.authorize(p,now);}portEXIT_CRITICAL(&mux);
  if(!send)return;
  xSemaphoreTake(io,portMAX_DELAY);now=esp_timer_get_time();
  portENTER_CRITICAL(&mux);send=phase.authorize(p,now);if(phase.state().latched)gateLow();portEXIT_CRITICAL(&mux);
  if(send){
   if(!rmtTransmitCompleted(AC_GATE_PIN)){portENTER_CRITICAL(&mux);fail(Mains::TIMING);portEXIT_CRITICAL(&mux);}
   else{
    portENTER_CRITICAL(&mux);if(!mapped){esp_rom_gpio_connect_out_signal(AC_GATE_PIN,signal,false,false);mapped=true;}portEXIT_CRITICAL(&mux);
    now=esp_timer_get_time();uint32_t wait=p.fire>now?(uint32_t)(p.fire-now):1;
    symbols[0].level0=0;symbols[0].duration0=wait;symbols[0].level1=1;symbols[0].duration1=200;
    symbols[1].level0=0;symbols[1].duration0=100;symbols[1].level1=0;symbols[1].duration1=1;
    bool ok=rmtWriteAsync(AC_GATE_PIN,symbols,2);uint64_t after=esp_timer_get_time();
    portENTER_CRITICAL(&mux);if(!ok)fail(Mains::RESOURCE);else if(after-now>150)fail(Mains::TIMING);else if(!phase.authorize(p,after))gateLow();portEXIT_CRITICAL(&mux);
   }
  }
  xSemaphoreGive(io);
 }
public:
#ifdef THEATRE_AC_BACKEND_TEST
 void testService(){service();}
#endif
 void begin(){digitalWrite(AC_GATE_PIN,LOW);pinMode(AC_GATE_PIN,OUTPUT);pinMode(AC_ZC_PIN,INPUT);}
 bool configure(bool on,bool rearm){
  if(!on){stop();return true;}if(!io)io=xSemaphoreCreateMutex();
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
  if(!attached){if(gpio_set_intr_type((gpio_num_t)AC_ZC_PIN,GPIO_INTR_POSEDGE)!=ESP_OK||gpio_isr_handler_add((gpio_num_t)AC_ZC_PIN,edge,this)!=ESP_OK){portENTER_CRITICAL(&mux);fail(Mains::RESOURCE);portEXIT_CRITICAL(&mux);return false;}attached=true;}return true;
 }
 void level(uint8_t v){portENTER_CRITICAL(&mux);phase.level(v);if(!v){pending=false;if(mapped)gateLow();}portEXIT_CRITICAL(&mux);}
 void tick(bool lease){portENTER_CRITICAL(&mux);phase.controller(esp_timer_get_time(),lease);if(phase.state().latched)gateLow();portEXIT_CRITICAL(&mux);}
 void stop(){if(attached){gpio_intr_disable((gpio_num_t)AC_ZC_PIN);gpio_isr_handler_remove((gpio_num_t)AC_ZC_PIN);attached=false;}portENTER_CRITICAL(&mux);phase.disable();pending=false;gateLow();portEXIT_CRITICAL(&mux);}
 Mains::State diagnostics(){portENTER_CRITICAL(&mux);auto s=phase.state();portEXIT_CRITICAL(&mux);return s;}
};
// Out-of-class strong definitions keep Xtensa IRAM literal pools ahead of l32r uses (not COMDAT/inline flash literals).
void IRAM_ATTR AcDimmer::gateLow(){gpio_ll_set_level(&GPIO,AC_GATE_PIN,0);esp_rom_gpio_connect_out_signal(AC_GATE_PIN,SIG_GPIO_OUT_IDX,false,false);mapped=false;}
void IRAM_ATTR AcDimmer::edge(void *arg){
 auto *self=(AcDimmer*)arg;Mains::Pulse p;bool notify=false;uint64_t now=esp_timer_get_time();
 portENTER_CRITICAL_ISR(&self->mux);if(self->phase.zeroCross(now,p)){if(self->pending){self->phase.trip(Mains::TIMING);self->pending=false;}else{self->latest=p;self->pending=true;notify=true;}}if(self->phase.state().latched)self->gateLow();portEXIT_CRITICAL_ISR(&self->mux);
 if(notify){BaseType_t wake=pdFALSE;vTaskNotifyGiveFromISR(self->task,&wake);if(wake)portYIELD_FROM_ISR();}
}
#endif

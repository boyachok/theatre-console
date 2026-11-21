#pragma once
#include <cassert>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#define IRAM_ATTR
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline std::function<void()> mockAcCriticalHook;
inline void portENTER_CRITICAL(int*){auto hook=std::move(mockAcCriticalHook);mockAcCriticalHook=nullptr;if(hook)hook();} inline void portEXIT_CRITICAL(int*){}
inline void portENTER_CRITICAL_ISR(int*){} inline void portEXIT_CRITICAL_ISR(int*){}
using TaskHandle_t=void*;using SemaphoreHandle_t=void*;using BaseType_t=int;using gpio_num_t=int;
constexpr int pdFALSE=0,pdTRUE=1,pdPASS=1,portMAX_DELAY=-1,GPIO_INTR_POSEDGE=1,GPIO_INTR_ANYEDGE=2,ESP_OK=0,ESP_INTR_FLAG_IRAM=1024,SIG_GPIO_OUT_IDX=256;
inline int pdMS_TO_TICKS(int n){return n;}inline int ulTaskNotifyTake(int,int){return 0;}inline void portYIELD_FROM_ISR(){}
inline uint64_t mockAcUs=1000,mockAcSubmitUs=20;inline bool mockAcRmtOK=true,mockAcWriteOK=true,mockAcBusy=false,mockAcMutexOK=true;
inline unsigned mockAcIrqFlags=0,mockAcInstalls=0,mockAcWrites=0,mockAcCreates=0;inline int mockAcCore=-1,mockAcPriority=0;
inline void (*mockAcHandler)(void*)=nullptr;inline void *mockAcHandlerArg=nullptr;
struct MockGPIO {struct Config{uint32_t func_sel=SIG_GPIO_OUT_IDX;}func_out_sel_cfg[49];}inline GPIO;
inline void gpio_ll_set_level(MockGPIO*,int pin,int value){mockLevels[pin]=value;}
inline std::function<void()> mockAcMapHook;
inline void esp_rom_gpio_connect_out_signal(int pin,uint32_t sig,bool,bool){if(sig!=SIG_GPIO_OUT_IDX&&mockAcMapHook)mockAcMapHook();GPIO.func_out_sel_cfg[pin].func_sel=sig;}
inline int gpio_install_isr_service(int flags){mockAcIrqFlags=flags;mockAcInstalls++;return ESP_OK;}
inline int gpio_ll_get_level(MockGPIO*,int pin){return mockLevels[pin];}
inline int gpio_set_intr_type(int pin,int type){assert(pin==41&&type==GPIO_INTR_ANYEDGE);return ESP_OK;}
inline int gpio_isr_handler_add(int pin,void (*handler)(void*),void *arg){assert(pin==41);mockAcHandler=handler;mockAcHandlerArg=arg;return ESP_OK;}
inline int gpio_intr_disable(int){return ESP_OK;}inline int gpio_isr_handler_remove(int){mockAcHandler=nullptr;return ESP_OK;}
inline int64_t esp_timer_get_time(){return mockAcUs;}
inline std::vector<std::unique_ptr<std::mutex>> mockAcMutexes;inline std::atomic<unsigned> mockAcTakes{0};
inline SemaphoreHandle_t xSemaphoreCreateMutex(){if(!mockAcMutexOK)return nullptr;mockAcMutexes.emplace_back(std::make_unique<std::mutex>());return mockAcMutexes.back().get();}inline int xSemaphoreTake(void *p,int){++mockAcTakes;static_cast<std::mutex*>(p)->lock();return pdPASS;}inline void xSemaphoreGive(void *p){static_cast<std::mutex*>(p)->unlock();}
inline int xTaskCreatePinnedToCore(void (*)(void*),const char*,int,void*,int priority,TaskHandle_t *handle,int core){*handle=(void*)1;mockAcCore=core;mockAcPriority=priority;mockAcCreates++;return pdPASS;}
inline void vTaskNotifyGiveFromISR(TaskHandle_t handle,BaseType_t *wake){assert(handle);*wake=pdFALSE;}
struct rmt_data_t {uint32_t duration0:15,level0:1,duration1:15,level1:1;};
constexpr int RMT_TX_MODE=0,RMT_MEM_NUM_BLOCKS_1=1;
inline uint32_t mockAcRmtHz=0;inline std::vector<rmt_data_t> mockAcFrame;
inline bool rmtInit(int pin,int mode,int blocks,uint32_t hz){assert(pin==40&&mode==RMT_TX_MODE&&blocks==1);mockAcRmtHz=hz;GPIO.func_out_sel_cfg[pin].func_sel=37;return mockAcRmtOK;}
inline bool rmtSetEOT(int pin,int value){assert(pin==40&&value==0);return true;}inline bool rmtDeinit(int){mockAcBusy=false;return true;}
inline bool rmtTransmitCompleted(int){return !mockAcBusy;}
inline bool rmtWriteAsync(int pin,rmt_data_t *p,size_t count){assert(pin==40&&count==2);mockAcFrame.assign(p,p+count);mockAcWrites++;mockAcUs+=mockAcSubmitUs;return mockAcWriteOK;}

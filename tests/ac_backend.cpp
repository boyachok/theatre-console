#define THEATRE_HOST_TEST
#define THEATRE_AC_BACKEND_TEST
#include <cassert>
#include <cstdio>
#include "../console/ac_dimmer.h"
int main(){
 AcDimmer driver;driver.begin();assert(mockLevels[40]==LOW&&mockModes[40]==OUTPUT&&mockModes[41]==INPUT&&mockAcCreates==0&&!driver.diagnostics().enabled);
 assert(driver.configure(true,true));assert(mockAcCreates==1&&mockAcCore==1&&mockAcPriority==20&&mockAcRmtHz==1000000&&mockAcInstalls==1&&mockAcIrqFlags==ESP_INTR_FLAG_IRAM&&mockAcHandler);
 auto edge=[&](uint64_t at,bool service=true){mockAcUs=at;mockNow=at/1000;driver.tick(true);assert(mockAcHandler);mockAcHandler(mockAcHandlerArg);mockAcUs+=20;if(service)driver.testService();};
 driver.level(128);for(int i=0;i<=6;i++)edge(1000+i*10000);assert(mockAcWrites==1&&driver.diagnostics().qualified);assert(mockAcFrame[0].level0==0&&mockAcFrame[0].level1==1&&mockAcFrame[0].duration0>=4900&&mockAcFrame[0].duration1==200&&mockAcFrame[1].level0==0&&mockAcFrame[1].level1==0);
 driver.level(0);assert(GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX&&mockLevels[40]==0);driver.level(200);edge(71000);edge(81000);assert(mockAcWrites==2&&GPIO.func_out_sel_cfg[40].func_sel==37); // Zero duty disconnects, but the next qualified pulse reconnects correctly.
 edge(91000,false);auto writes=mockAcWrites;driver.stop();driver.testService();assert(!driver.diagnostics().enabled&&mockAcWrites==writes&&!mockAcHandler&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);
 mockAcUs=100000;assert(driver.configure(true,true));driver.level(255);for(int i=0;i<=6;i++)edge(101000+i*10000);assert(mockAcWrites==writes+1&&mockAcCreates==1&&mockAcInstalls==1);
 mockAcSubmitUs=200;edge(171000);assert(driver.diagnostics().latched&&driver.diagnostics().fault==Mains::TIMING&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);writes=mockAcWrites;mockAcSubmitUs=20;
 assert(driver.configure(true,false));edge(181000);assert(driver.diagnostics().latched&&mockAcWrites==writes); // Reconciliation cannot clear the fault.
 mockAcUs=190000;assert(driver.configure(true,true));driver.level(128);for(int i=0;i<=6;i++)edge(191000+i*10000);assert(!driver.diagnostics().latched&&mockAcWrites==writes+1);
 mockAcUs=252000;driver.tick(false);assert(!driver.diagnostics().latched&&GPIO.func_out_sel_cfg[40].func_sel==37);writes=mockAcWrites;driver.testService();assert(mockAcWrites==writes);
 mockAcUs=260000;driver.stop();assert(driver.configure(true,true));driver.level(128);for(int i=0;i<=6;i++)edge(261000+i*10000);mockAcBusy=true;edge(331000);assert(driver.diagnostics().latched&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);mockAcBusy=false;
 mockAcUs=340000;assert(driver.configure(true,true));driver.level(128);writes=mockAcWrites;for(int i=0;i<6;i++)edge(341000+i*10000);edge(401000,false);edge(411000,false);driver.testService();assert(driver.diagnostics().fault==Mains::TIMING&&driver.diagnostics().latched&&mockAcWrites==writes&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);
 AcDimmer fail;fail.begin();mockAcMutexOK=false;assert(!fail.configure(true,true)&&fail.diagnostics().fault==Mains::RESOURCE&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);mockAcMutexOK=true;
 puts("Actual AC adapter: lazy 1MHz/one-block non-looping RMT, IRAM zero-cross registration, bounded frame, priority/core, zero/resume matrix routing, STOP pending cancellation, late/busy/resource latches, Wi-Fi-loss hold and explicit-only rearm passed (SDK/RMT mocked; no physical waveform proof)");
}

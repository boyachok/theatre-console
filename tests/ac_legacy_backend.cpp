#define THEATRE_HOST_TEST
#define THEATRE_AC_BACKEND_TEST
#define THEATRE_AC_LEGACY_082 1
#include <cassert>
#include <cstdio>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <Preferences.h>
#include "../console/ac_dimmer.h"
int main(){
 mockPrefsBytes["pulse-v1"]={0xff,0,1};auto old=mockPrefsBytes;
 AcDimmer ac;ac.begin();assert(ac.monitor()&&!ac.pulseConfigFault());assert(ac.savePulse(400)==INVALID&&mockPrefsBytes==old);
 auto rise=[&](uint64_t at,bool service=true){mockAcUs=at;mockNow=at/1000;ac.tick(true);mockLevels[41]=1;mockAcHandler(mockAcHandlerArg);mockAcUs+=20;if(service)ac.testService();};
 ac.level(77);for(int i=0;i<=7;i++)rise(1000+i*10000);assert(ac.diagnostics().qualified&&mockAcFrame[0].duration1==200);
 ac.off();ac.level(77);rise(81000,false);
 std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;std::atomic<bool> stopped{false};
 mockAcMapHook=[&]{std::unique_lock<std::mutex> l(mutex);entered=true;cv.notify_all();cv.wait(l,[&]{return release;});};
 std::thread worker([&]{ac.testService();});{std::unique_lock<std::mutex> l(mutex);assert(cv.wait_for(l,std::chrono::seconds(2),[&]{return entered;}));}
 auto takes=mockAcTakes.load();std::thread stop([&]{ac.off();stopped=true;});for(int i=0;i<100&&mockAcTakes==takes;i++)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(mockAcTakes>takes&&!stopped);
 {std::lock_guard<std::mutex> l(mutex);release=true;}cv.notify_all();worker.join();stop.join();mockAcMapHook=nullptr;
 auto count=mockAcWrites;ac.testService();assert(stopped&&ac.diagnostics().requested==0&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX&&count==mockAcWrites);assert(mockPrefsBytes==old);
 puts("AC082 actual adapter: RMT fixed200us, pulse NVS ignored/unchanged, threaded STOP/remap cancellation safety retained; no physical waveform claim");
}

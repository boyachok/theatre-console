#define THEATRE_HOST_TEST
#define THEATRE_AC_BACKEND_TEST
#include <cassert>
#include <cstdio>
#include <thread>
#include <condition_variable>
#include <chrono>
#include "../console/ac_dimmer.h"
int main(){
 for(uint32_t half:{8333U,10000U})for(int width:{200,300,400})for(int v=0;v<256;v++){
  Mains::Phase p;assert(p.configurePulse(width));p.arm(1000);p.level(v);Mains::Pulse pulse;bool sent=false;
  for(int i=0;i<=8;i++){auto t=1000ULL+i*half;p.controller(t,true);sent=p.zeroCross(t,pulse);if(sent){assert(v&&pulse.pulseUs==width&&pulse.fire>=pulse.zero+300&&pulse.fire+width+300<=pulse.end&&p.authorize(pulse,t+20));}}
  assert(sent==bool(v));if(v){assert(!p.configurePulse(200));if(width==200){double lo=0,hi=3.141592653589793;for(int n=0;n<24;n++){double a=(lo+hi)*.5,q=1-a/3.141592653589793+sin(2*a)/6.283185307179586;if(q>v/255.)lo=a;else hi=a;}uint16_t tab=(uint16_t)((lo+hi)*.5/3.141592653589793*65535);uint32_t legacy=half*tab/65535;legacy=std::max(300U,std::min(half-1000,legacy));assert(pulse.fire-pulse.zero==legacy);}auto old=pulse;p.level(0);assert(p.configurePulse(300)&&!p.authorize(old,old.zero+30));}
 }
 assert(!validControl(AC_PULSE,1,300)&&!validControl(AC_PULSE,0,199)&&!validControl(AC_PULSE,0,401)&&needsLiveLink(AC_PULSE,300)&&!safeDuringOTA(AC_PULSE,300));
 mockPrefsBytes.clear();mockPrefsWriteOK=true;mockPrefsBytes["dimConfig"]={3,4,5};auto original=mockPrefsBytes;
 AcDimmer d;d.begin();assert(d.monitor());const auto writes=mockAcWrites;assert(d.savePulse(300)==APPLIED&&d.diagnostics().pulseUs==300&&mockAcWrites==writes&&mockLevels[40]==LOW&&mockPrefsBytes["dimConfig"]==original["dimConfig"]);
 auto record=mockPrefsBytes["pulse-v1"];mockPrefsWriteOK=false;assert(d.savePulse(400)==IO_ERROR&&d.diagnostics().pulseUs==300&&mockPrefsBytes["pulse-v1"]==record);mockPrefsWriteOK=true;
 d.level(77);assert(d.savePulse(400)==NOT_READY);d.off();assert(d.savePulse(400)==APPLIED);
 auto rise=[&](AcDimmer &a,uint64_t at,bool service=true){mockAcUs=at;mockNow=at/1000;a.tick(true);mockLevels[41]=1;mockAcHandler(mockAcHandlerArg);mockAcUs+=20;if(service)a.testService();};
 for(int i=0;i<=6;i++)rise(d,1000+i*10000);assert(d.diagnostics().qualified);d.level(77);rise(d,71000);rise(d,81000);assert(!mockAcFrame.empty());assert(mockAcFrame[0].duration1==400&&mockAcFrame[1].level0==0);
 d.off();assert(d.savePulse(200)==APPLIED);d.level(77);rise(d,91000);rise(d,101000);assert(mockAcFrame[0].duration1==200);d.stop();
 AcDimmer reboot;reboot.begin();assert(reboot.diagnostics().pulseUs==200&&!reboot.pulseConfigFault());assert(reboot.savePulse(300)==APPLIED);reboot.stop();AcDimmer persisted;persisted.begin();assert(persisted.diagnostics().pulseUs==300);persisted.stop();
 mockPrefsBytes["pulse-v1"][4]^=1;AcDimmer corrupt;corrupt.begin();assert(corrupt.pulseConfigFault()&&!corrupt.monitor()&&corrupt.diagnostics().latched);assert(corrupt.savePulse(200)==APPLIED&&!corrupt.pulseConfigFault());assert(corrupt.configure(true,true));corrupt.stop();
 // Actual two-thread adapter test: STOP must wait for an already-authorized worker before it can remap+submit.
 AcDimmer race;mockAcUs=200000;race.begin();assert(race.monitor());race.level(77);for(int i=0;i<=7;i++)rise(race,201000+i*10000);race.off();race.level(77);rise(race,281000,false);
 std::mutex m;std::condition_variable cv;bool entered=false,release=false;std::atomic<bool> stopped{false};
 mockAcMapHook=[&]{std::unique_lock<std::mutex> lock(m);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});};
 std::thread worker([&]{race.testService();});{std::unique_lock<std::mutex> lock(m);assert(cv.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
 auto takes=mockAcTakes.load();std::thread stop([&]{race.off();stopped=true;});for(int i=0;i<100&&mockAcTakes==takes;i++)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(mockAcTakes>takes&&!stopped);
 {std::lock_guard<std::mutex> lock(m);release=true;}cv.notify_all();worker.join();stop.join();mockAcMapHook=nullptr;assert(stopped&&race.diagnostics().requested==0&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);auto stoppedWrites=mockAcWrites;race.testService();assert(mockAcWrites==stoppedWrites);race.stop();
 mockPrefsBeginOK=false;AcDimmer unavailable;unavailable.begin();assert(unavailable.pulseConfigFault()&&!unavailable.monitor()&&unavailable.savePulse(200)==IO_ERROR);mockPrefsBeginOK=true;
 puts("AC pulse200/300/400us: all256 levels50/60Hz, exact200us legacy timing, captured widths/epoch/300us end budget, no active writes, separate CRC/NVS/reboot/repair/NVSfailure, actual RMT duration and LOW EOT, threaded STOP-vs-remap serialization; SDK mocked, NOT mains waveform/thermal proof");
}

#define THEATRE_HOST_TEST
#define THEATRE_AC_BACKEND_TEST
#include <cassert>
#include <cstdio>
#include "../console/ac_dimmer.h"
int main(){
 AcDimmer d;d.begin();assert(mockModes[41]==INPUT&&mockModes[40]==OUTPUT&&mockLevels[40]==LOW&&mockAcCreates==0);
 assert(d.configure(true,true));assert(mockAcCreates==1&&mockAcCore==1&&mockAcPriority==20&&mockAcRmtHz==1000000&&mockAcIrqFlags==ESP_INTR_FLAG_IRAM);
 auto rise=[&](uint64_t at,bool service=true){mockAcUs=at;mockNow=at/1000;d.tick(true);mockLevels[41]=1;assert(mockAcHandler);mockAcHandler(mockAcHandlerArg);mockAcUs+=20;if(service)d.testService();};
 auto fall=[&](uint64_t at){mockAcUs=at;mockLevels[41]=0;assert(mockAcHandler);mockAcHandler(mockAcHandlerArg);};
 d.level(128);for(int i=0;i<=6;i++){rise(1000+i*10000);fall(1500+i*10000);}assert(d.diagnostics().qualified&&mockAcWrites==1&&mockAcFrame[0].duration1==200&&mockAcFrame[0].level1==1&&mockAcFrame[1].level0==0&&GPIO.func_out_sel_cfg[40].func_sel==37);
 auto tele=d.telemetry();assert(tele.highUs==500&&tele.lowUs==9500&&tele.rawEdges==7);
 rise(61350);fall(61400);rise(71000);fall(71500);assert(!d.diagnostics().latched&&d.diagnostics().duplicateEdges==1&&mockAcWrites==2);
 // Pending STOP is never resurrected, even if a worker runs after STOP.
 rise(81000,false);auto writes=mockAcWrites;d.stop();d.testService();assert(mockAcWrites==writes&&!mockAcHandler&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);
 mockAcUs=100000;assert(d.configure(true,true));d.level(128);for(int i=0;i<=6;i++)rise(101000+i*10000);assert(mockAcWrites==writes+1);writes=mockAcWrites;
 mockAcBusy=true;rise(171000);assert(mockAcWrites==writes&&!d.diagnostics().latched&&d.diagnostics().qualified&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX&&d.telemetry().incident.event.reason==Mains::TX_BUSY);mockAcBusy=false;rise(181000);assert(mockAcWrites==writes+1&&GPIO.func_out_sel_cfg[40].func_sel==37);
 mockAcSubmitUs=200;rise(191000);assert(!d.diagnostics().latched&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX&&d.telemetry().incident.event.reason==Mains::TX_SLOW&&d.telemetry().maxSubmitDuration==200);mockAcSubmitUs=20;rise(201000);assert(GPIO.func_out_sel_cfg[40].func_sel==37);
 rise(211000,false);writes=mockAcWrites;mockAcUs=217000;d.tick(true);d.testService();assert(mockAcWrites==writes&&!d.diagnostics().latched&&d.telemetry().incident.event.reason==Mains::PULSE_LATE&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);rise(221000);assert(mockAcWrites==writes+1);
 // Replace an obsolete pending half-wave; do not fire it late in the next one.
 rise(231000,false);writes=mockAcWrites;rise(241000,false);d.testService();assert(mockAcWrites==writes+1&&!d.diagnostics().latched&&d.telemetry().incident.event.reason==Mains::PENDING_REPLACED);
 // Real signal loss disconnects matrix; six fresh intervals may requalify, never free-run while absent.
 mockAcUs=277000;d.tick(true);d.testService();assert(!d.diagnostics().qualified&&!d.diagnostics().latched&&d.diagnostics().requested==128&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);writes=mockAcWrites;auto snap=d.telemetry();assert(snap.incident.event.reason==Mains::ZC_TIMEOUT&&snap.incident.lastAccepted==241000&&snap.incidentCount>0);
 for(int i=0;i<=6;i++){rise(281000+i*10000);assert(mockAcWrites==writes+(i==6?1:0));}assert(d.diagnostics().qualified&&d.diagnostics().recoveries==1&&GPIO.func_out_sel_cfg[40].func_sel==37);
 mockAcWriteOK=false;rise(351000);assert(d.diagnostics().latched&&d.diagnostics().fault==Mains::RESOURCE&&d.telemetry().incident.event.reason==Mains::TX_REJECT&&d.telemetry().incident.event.hard&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);mockAcWriteOK=true;writes=mockAcWrites;rise(361000);assert(mockAcWrites==writes);d.stop();mockAcUs=370000;assert(d.configure(true,true));assert(d.diagnostics().requested==0&&!d.diagnostics().qualified&&!d.diagnostics().latched);
 // Preemption injected precisely between a worker's clock sample and lock acquisition.
 d.stop();AcDimmer race;mockAcUs=500000;race.begin();assert(race.configure(true,true));race.level(128);for(int i=0;i<=6;i++){mockAcUs=501000+i*10000;race.tick(true);mockLevels[41]=1;mockAcHandler(mockAcHandlerArg);mockAcUs+=20;race.testService();}assert(race.diagnostics().qualified);mockAcUs=570000;race.tick(true);mockAcUs=571000;mockAcCriticalHook=[](){mockAcUs+=3;mockLevels[41]=1;mockAcHandler(mockAcHandlerArg);};race.testService();assert(race.diagnostics().qualified&&!race.diagnostics().latched&&race.diagnostics().syncLosses==0&&race.diagnostics().staleClockSamples==0);
 // Production STOP keeps the already-qualified monitor but must cancel pending output atomically.
 mockAcUs=581003;race.tick(true);mockLevels[41]=1;mockAcHandler(mockAcHandlerArg);auto beforeOff=mockAcWrites;race.off();race.testService();assert(mockAcWrites==beforeOff&&race.diagnostics().enabled&&race.diagnostics().qualified&&race.diagnostics().requested==0&&mockAcHandler&&GPIO.func_out_sel_cfg[40].func_sel==SIG_GPIO_OUT_IDX);
 mockAcUs=591003;race.tick(true);mockAcHandler(mockAcHandlerArg);race.testService();assert(mockAcWrites==beforeOff);assert(race.configure(false,false)&&race.diagnostics().qualified);assert(race.configure(true,false)&&race.diagnostics().qualified);race.level(255);mockAcUs=601003;race.tick(true);mockAcHandler(mockAcHandlerArg);mockAcUs+=20;race.testService();assert(mockAcWrites==beforeOff+1&&race.diagnostics().qualified);
 puts("PASS: always-ready monitor/STOP/pending/next-scene and exact +3us GPIO-preemption race and actual adapter with SDK mocks: ANYEDGE widths, 350us duplicate rejection, RMT pulse/EOT, STOP/epoch, transient busy/slow/late skips, pending replacement, loss gate-low, six-edge automatic reacquire, frozen incident, hard resource latch; no physical waveform proof");
}

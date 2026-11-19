#include <cassert>
#include <cstdio>
#include "../remote/battery.h"
using namespace RemoteBattery;
int main(){
 Monitor m;Record record;uint32_t t=0;
 auto feed=[&](unsigned adc,unsigned n=480){for(unsigned i=0;i<n;i++){m.sample(adc,t);t+=25;}};
 assert(!m.enabled()&&m.reserve(0)==-1&&m.prepare(false,0,record));
 feed(2100);assert(m.stable(t)&&m.mv()==4200&&m.spread()==0&&!m.prepare(true,t,record));m.load(record);
 assert(m.enabled()&&!m.calibrated()&&m.reserve(t)==100&&m.color(t)==0x001400);
 Record full=record;feed(1850);assert(!m.prepare(false,t,record));m.load(record);assert(m.calibrated()&&m.calibration.lowMv==3700&&m.reserve(t)<=1);
 feed(2000);assert(m.reserve(t)>=59&&m.reserve(t)<=61&&m.color(t)==0x001400);
 feed(1925);assert(m.reserve(t)>=29&&m.reserve(t)<=31&&m.color(t)==0x120a00);
 feed(1875);assert(m.reserve(t)>=9&&m.reserve(t)<=11&&m.color(t)==0x140000);
 feed(1800);assert(m.reserve(t)==0);assert(m.color(t)==0||m.color(t)==0x180000);
 assert(m.prepare(true,t,record)); // A discharged cell cannot become FULL.
 feed(2100);assert(m.prepare(false,t,record)); // LOW too close to FULL is rejected.
 assert(!m.fresh(t+1500)&&m.reserve(t+1500)==-1);
 feed(0);assert(!m.fresh(t)&&m.reserve(t)==-1);
 feed(2300);assert(!m.fresh(t));feed(1350);assert(m.prepare(false,t,record)); // Deep-discharge endpoint forbidden.
 Record broken=full;broken.lowMv++;Monitor restored;restored.load(broken);assert(!restored.enabled());restored.load(full);assert(restored.enabled()&&restored.calibration.fullMv==4200);
 Monitor noisy;for(int i=0;i<160;i++){noisy.sample((i/20)%2?2050:2150,t);t+=25;}assert(!noisy.stable(t)&&noisy.prepare(true,t,record));
 Monitor wrap;for(int i=0;i<160;i++){wrap.sample(2100,0xfffff000U+uint32_t(i*25));}assert(wrap.stable(uint32_t(0xfffff000U+160*25)));
 LinkDiagnostic d;assert(!d.tick(true,true,0));assert(!d.tick(true,true,1500)&&!d.active(1500)); // Held at boot not armed.
 d.tick(false,false,1600);d.tick(false,false,1630);assert(!d.tick(true,true,1700)&&d.suppress());assert(!d.tick(true,true,2699));assert(d.tick(true,true,2700)&&d.remaining(2700)==10000);
 assert(!d.tick(true,false,2800)&&d.suppress());assert(!d.tick(true,false,13000)&&!d.active(13000)&&d.suppress());
 d.tick(false,false,13100);d.tick(false,false,13130);assert(!d.suppress());d.tick(true,true,13200);d.tick(false,true,13300);assert(!d.tick(false,true,18000)&&!d.active(18000)&&d.suppress());
 d.tick(false,false,18100);d.tick(false,false,18130);d.tick(true,true,18200);assert(d.tick(true,true,19200));assert(!d.tick(true,true,40000)&&!d.active(40000));
 LinkDiagnostic dw;dw.tick(false,false,0xfffff000);dw.tick(false,false,0xfffff030);dw.tick(true,true,0xffffff00);assert(dw.tick(true,true,0x2e8)&&dw.active(0x2e8));assert(!dw.active(0x2e8+10000));
 puts("Remote battery model:2:1 divider, stable/manual FULL+LOW with CRC/range checks, conditional reserve not SOC, hysteresis/unknown/low LEDs, stale/noisy/disconnected ADC and timer wrap;1s chord/10s diagnostics swallow both releases");
}

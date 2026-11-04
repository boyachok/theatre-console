#include <cassert>
#include <cstdio>
#include "../console/remote_link.h"
#include "../console/network_led.h"
int main(){
 using namespace Remote;Buttons b;
 assert(b.tick(false,false,true,40)==NONE);assert(b.tick(true,false,true,50)==NONE);assert(b.tick(true,false,true,80)==NONE);assert(b.tick(false,false,true,120)==NONE);assert(b.tick(false,false,true,150)==FORWARD);
 assert(b.tick(false,true,true,200)==NONE);assert(b.tick(false,true,true,230)==NONE);assert(b.tick(false,true,true,3229)==NONE);assert(b.tick(false,true,true,3230)==FIRST);assert(b.tick(false,false,true,3300)==NONE);assert(b.tick(false,false,true,3330)==NONE);
 b.tick(true,false,true,3400);b.tick(true,false,true,3430);assert(b.tick(true,false,true,6430)==LAST);b.tick(false,false,true,6500);assert(b.tick(false,false,true,6530)==NONE);
 Buttons boot;boot.cancel(true,false,0);assert(boot.tick(true,false,true,4000)==NONE);boot.tick(false,false,true,4100);assert(boot.tick(false,false,true,4130)==NONE);
 boot.tick(true,false,true,4200);boot.tick(true,false,true,4230);boot.tick(true,false,false,4300);assert(boot.tick(true,false,true,9000)==NONE);boot.tick(false,false,true,9010);assert(boot.tick(false,false,true,9040)==NONE);
 Buttons both;both.tick(false,false,true,40);both.tick(true,false,true,50);both.tick(true,false,true,80);assert(both.tick(true,true,true,100)==NONE);assert(both.tick(true,true,true,5000)==NONE);both.tick(false,false,true,5100);assert(both.tick(false,false,true,5130)==NONE);
 assert(target(FORWARD,100,98,true)==99&&target(FORWARD,100,99,true)==99&&target(BACK,100,0,true)==0&&target(LAST,100,0,false)==99&&target(FORWARD,100,88,false)==0);
 Gate gate;gate.issue(99,10000);assert(gate.inspect(1,FORWARD,99,10700,10000)==Gate::NEW);gate.remember(1,FORWARD,0);assert(gate.inspect(1,FORWARD,99,10700,20000)==Gate::DUPLICATE);assert(gate.inspect(1,BACK,99,10700,10010)==Gate::REJECT);
 assert(gate.inspect(2,FORWARD,99,10700,10701)==Gate::REJECT);assert(gate.inspect(2,FORWARD,55,10700,10050)==Gate::REJECT);assert(gate.inspect(2,FORWARD,99,50000,10050)==Gate::REJECT);
 gate.issue(100,10500);gate.issue(101,11000);assert(gate.inspect(2,BACK,99,11700,11000)==Gate::NEW);gate.issue(102,11500);assert(gate.inspect(2,BACK,99,11700,11500)==Gate::REJECT);
 Gate wrap;wrap.issue(123,0xffffff00U);assert(wrap.inspect(1,LAST,123,(uint32_t)(0xffffff00U+700),0xffffff20U)==Gate::NEW);
 Outbox box;for(int i=0;i<4;i++)assert(box.push(FORWARD,55,10000,0));assert(!box.push(BACK,55,10000,0));auto *event=box.due(0);assert(event&&event->id==1&&event->deadline==10700);box.attempted(*event,0);assert(!box.due(119));assert(box.due(120)->id==1);assert(!box.acknowledge(99));assert(box.acknowledge(1));assert(box.due(121)->id==2);assert(!box.due(700)&&box.size()==0);
 assert(box.push(BACK,56,11000,1000));box.clear();assert(box.size()==0&&!box.due(1001));
 assert(networkLedColor(EXCELLENT,0)==0x14&&networkLedColor(GOOD,0)==0x1400&&networkLedColor(MARGINAL,0)==0x120a00&&networkLedColor(BAD,0)==0x140000);assert(networkLedColor(EXCELLENT,0,false,true)==0x141414);
 puts("Remote: debounce/short/3s/no-release-duplicate, boot-held/both-buttons/reconnect cancellation, navigation boundaries, expiring challenges/deadlines, cached retries, FIFO/no offline backlog, clock wrap and common LED palette passed");
}

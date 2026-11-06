#pragma once
#include <stdint.h>
namespace Remote {
// Observation only: never emits navigation. Counters retain even missed HTTP samples.
class ButtonProbe {
 struct Key {bool raw=false,down=false;uint32_t changed=0;} keys[2];
public:
 uint32_t edges[2]{},presses[2]{},releases[2]{};
 void reset(bool forward,bool back,uint32_t now){
  for(int i=0;i<2;i++){keys[i]={i?back:forward,i?back:forward,now};edges[i]=presses[i]=releases[i]=0;}
 }
 void tick(bool forward,bool back,uint32_t now){
  bool levels[2]{forward,back};
  for(int i=0;i<2;i++){
   auto &k=keys[i];if(levels[i]!=k.raw){k.raw=levels[i];k.changed=now;edges[i]++;}
   if(k.down!=k.raw&&now-k.changed>=30){k.down=k.raw;if(k.down)presses[i]++;else releases[i]++;}
  }
 }
 bool down(unsigned i)const{return i<2&&keys[i].down;}
 uint32_t color(bool pulse)const{return down(0)&&down(1)?0x606000:down(0)||down(1)||pulse?0x606060:0x200020;}
};
}

#pragma once
#include <stdint.h>
namespace Remote {
enum Action:uint8_t { NONE=0,FORWARD=1,BACK=2,FIRST=3,LAST=4 };
inline bool validAction(uint8_t a){return a>=FORWARD&&a<=LAST;}
// Pure, active-LOW button logic. No press is generated for a button held at boot/reconnect.
class Buttons {
 struct Key {bool raw=false,down=false,armed=false,fired=false;uint32_t changed=0,at=0;};Key keys[2];
public:
 void cancel(bool forward,bool back,uint32_t now){for(int i=0;i<2;i++){keys[i]=Key{};keys[i].raw=keys[i].down=i?back:forward;keys[i].changed=now;}}
 Action tick(bool forward,bool back,bool enabled,uint32_t now){
  if(!enabled||(forward&&back)){cancel(forward,back,now);return NONE;}
  bool levels[2]{forward,back};Action result=NONE;
  for(int i=0;i<2;i++){auto &k=keys[i];bool down=levels[i];if(down!=k.raw){k.raw=down;k.changed=now;}
   if(now-k.changed>=30){
    if(!down&&!k.down)k.armed=true;
    if(down!=k.down){k.down=down;if(down){k.at=now;k.fired=false;}else{if(k.armed&&!k.fired)result=i?BACK:FORWARD;k.armed=true;k.fired=false;}}
   }
   if(k.down&&k.armed&&!k.fired&&now-k.at>=3000){k.fired=true;result=i?FIRST:LAST;}
  }return result;
 }
};
inline uint16_t target(Action action,uint16_t count,uint16_t current,bool active){
 if(!count)return 0;if(action==FIRST)return 0;if(action==LAST)return count-1;
 if(!active)return 0;if(action==FORWARD)return current>=count-1?count-1:current+1;
 return current==0?0:current>=count?count-1:current-1;
}
}

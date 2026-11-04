#pragma once
#include "quality.h"
// One palette for master, slaves and the remote. White TX pulse is NOT an application ACK.
inline uint32_t networkLedColor(Health health,uint32_t now,bool identify=false,bool txPulse=false){
 if(identify)return (now/150)%2?0x141414:0;
 if(txPulse)return 0x141414;
 switch(health){case EXCELLENT:return 0x000014;case GOOD:return 0x001400;case MARGINAL:return 0x120a00;case BAD:return 0x140000;case OFFLINE:return (now/300)%2?0x180000:0;default:return (now/700)%2?0x040404:0;}
}

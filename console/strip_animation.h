#pragma once
#include <cmath>
#include <algorithm>
#include "score_wire.h"
namespace Show {
inline uint32_t stripHash(uint32_t x){x^=x>>16;x*=0x7feb352d;x^=x>>15;x*=0x846ca68b;return x^(x>>16);}
inline float stripFract(float x){return x-std::floor(x);}
inline void stripHue(float h,float &r,float &g,float &b){r=std::min(1.f,std::max(0.f,std::fabs(stripFract(h)*6-3)-1));g=std::min(1.f,std::max(0.f,2-std::fabs(stripFract(h)*6-2)));b=std::min(1.f,std::max(0.f,2-std::fabs(stripFract(h)*6-4)));}
inline bool legacyStripOptions(const StripOptions &o){StripOptions z{};return !memcmp(&o,&z,sizeof(o));}
inline void stripPixel(const Strip &e,const StripOptions &o,uint16_t i,uint16_t count,uint32_t elapsed,float gate,uint8_t (&rgb)[3]){
 float phase=e.duration?(float)elapsed/e.duration:1,part=stripFract(phase),p=count>1?(float)i/(count-1):.5f;
 float v=0,r=e.r/255.f,g=e.g/255.f,b=e.b/255.f,width=std::max(1.f,count*e.width/100.f);
 if(e.mode==2)v=1;
 else if(e.mode==1){uint16_t k=e.direction?count-1-i:i;v=k<std::min(1.f,phase)*count?1:0;}
 else if(e.mode==3){float center=(1-std::fabs(2*part-1))*(count-1);v=std::max(0.f,1-std::fabs(i-center)/std::max(1.f,width*.5f));}
 else if(e.mode==4){float h=p+phase*.2f;if(o.palette==1)h=.02f+stripFract(h)*.13f;else if(o.palette==2)h=.45f+stripFract(h)*.3f;stripHue(h,r,g,b);float chase=1-std::fabs(2*stripFract(p*(1+o.density/20.f)-phase)-1);v=.25f+.65f*chase;float chance=(stripHash(i+o.seed+(elapsed/80)*1021)&65535)/65535.f;if(chance<o.sparkles/400.f){v=1;r=g=b=1;}}
 else if(e.mode==5){r=1;g=b=0;float jitter=((stripHash((uint32_t)phase+o.seed)&255)/255.f-.5f)*o.noise/200.f;v=part<.35f+jitter?1:0;float random=(stripHash(i+o.seed+(elapsed/33)*1021)&65535)/65535.f;v*=1-(o.noise/100.f)*random;}
 else if(e.mode==6){float radial=count>1?std::abs((int)i*2-(count-1))/(float)(count-1):0,bands=1+o.density/10.f,t=stripFract(radial*bands-phase);v=std::max(0.f,1-t/std::max(.01f,e.width/100.f));}
 else{float center=part*count;if(e.direction)center=count-center;float d=std::fabs(i-center);d=std::min(d,count-d);v=std::max(0.f,1-d/(width*.5f));}
 float bg=legacyStripOptions(o)?1.f:(o.backgroundEnabled?o.backgroundBrightness/100.f:0.f),a=std::min(1.f,std::max(0.f,v*gate)),brightness=e.brightness/100.f;
 rgb[0]=(uint8_t)std::min(255.f,((e.br/255.f*bg)*(1-a)+r*a)*brightness*255);
 rgb[1]=(uint8_t)std::min(255.f,((e.bg/255.f*bg)*(1-a)+g*a)*brightness*255);
 rgb[2]=(uint8_t)std::min(255.f,((e.bb/255.f*bg)*(1-a)+b*a)*brightness*255);
}
}

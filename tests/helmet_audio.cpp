#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <cmath>
#include "../helmet/voice_engine.h"
#include "../console/strip_animation.h"
#include <Preferences.h>
#include "../console/ws2811.h"
#include "../console/slave_score_store.h"
int main(){
 static Helmet::VoiceProcessor dsp;uint32_t input[512]{};int32_t output[512]{};Helmet::VoiceStats stats;
 Show::VoiceProfile p{1,1,0,0,0,1,0,8,.15f,85,1,0,-70,0,4500,0,.5f,0,1,0};assert(Show::validVoice(p));
 for(int algorithm=0;algorithm<5;algorithm++){p.algorithm=algorithm;auto settings=Helmet::soundSettings(p,true);for(int block=0;block<200;block++){for(int i=0;i<256;i++){float v=.02f*sinf((block*256+i)*2*3.14159265f*173/48000);input[i*2]=((uint32_t)(int32_t)(v*8388607))<<8;input[i*2+1]=0;}dsp.process(settings,input,output,256,stats);for(int i=0;i<256;i++){assert(output[2*i]==output[2*i+1]);assert(std::fabs(output[2*i]/2147483647.f)<=.891f);}}assert(std::isfinite(stats.inputLevel)&&stats.inputLevel>0);}
 auto muted=Helmet::soundSettings(p,false);for(int n=0;n<100;n++)dsp.process(muted,input,output,256,stats);for(auto s:output)assert(std::abs(s)<100);assert(stats.inputLevel>0&&!stats.enabled); // Microphone still available for light while speaker is muted.
 p.gain=INFINITY;assert(!Show::validVoice(p));p.gain=8;p.algorithm=4;p.frequency=1000;assert(Show::validVoice(p));p.frequency=1001;assert(!Show::validVoice(p));
 Show::Strip e{};e.enabled=1;e.action=2;e.r=255;e.brightness=100;e.width=20;e.duration=2000;Show::StripOptions x{};x.density=50;x.sparkles=25;x.noise=30;x.seed=1;x.backgroundEnabled=1;x.backgroundBrightness=100;
 for(int mode=0;mode<=6;mode++){e.mode=mode;for(int count:{1,2,3,60,1024})for(uint32_t elapsed:{0u,500u,2000u,0xffffff00u})for(int i=0;i<count;i++){uint8_t rgb[3];Show::stripPixel(e,x,i,count,elapsed,1,rgb);if(mode==6){uint8_t mirror[3];Show::stripPixel(e,x,count-1-i,count,elapsed,1,mirror);for(int k=0;k<3;k++)assert(rgb[k]==mirror[k]);}}}
 // Reactive foreground is silent without real microphone input; background survives the release.
 Show::Ws2811 strip(47);assert(strip.begin());e.mode=2;e.count=10;e.width=100;e.bg=20;x.reactive=1;x.sensitivity=50;x.releaseMs=100;assert(strip.apply(e,0,true,x));
 auto byte=[&](int channel){uint8_t b=0;for(int bit=0;bit<8;bit++)b=(b<<1)|(mockRmtFrame[channel*8+bit].duration0==13);return b;};
 strip.tick(0,0);assert(byte(0)==0&&byte(1)==20);strip.tick(100,.1f);assert(byte(0)==255&&byte(1)==0);strip.tick(150,0);assert(byte(0)>=127&&byte(0)<=128&&byte(1)==10);strip.tick(300,0);assert(byte(0)==0&&byte(1)==20);strip.stop();strip.tick(400,.1f);assert(byte(0)==0&&byte(1)==0);
 // Score11 files retain all old bytes/settings and migrate only in RAM, never execute at boot.
 Show::Score11Cue old{};old.dimmers[0].value=17;old.inversionSet=old.invertedMask=1;old.lightDelayMs[0]=123;old.audioLoop=1;Show::FileHeader header{Show::MAGIC,77,Show::crc32(&old,sizeof(old)),1,11};auto file=LittleFS.open("/slave-score.bin","w");file.write((uint8_t*)&header,sizeof(header));file.write((uint8_t*)&old,sizeof(old));file.close();auto original=*LittleFS.files["/slave-score.bin"];Show::SlaveStore store;store.begin();assert(store.version==77&&store.cues[0].invertedMask==1&&store.cues[0].lightDelayMs[0]==123&&!store.cues[0].voice.configured&&*LittleFS.files["/slave-score.bin"]==original);
 puts("Helmet DSP: five real engines finite/bounded/stereo, silent output with active microphone, validation; all strip modes/counts and symmetric loading checked numerically, not physical audio/latency");
}

#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <cmath>
#include "../helmet/voice_engine.h"
double response(Show::VoiceTone p,float hz,bool mic=false){VoiceToneDSP::Processor fx;fx.configure(p);double energy=0;for(int i=0;i<96000;i++){float x=.05f*sinf(2*VoiceToneDSP::kPi*hz*i/48000),y=fx.tick(mic?0:x,mic?x:0);assert(std::isfinite(y));if(i>=48000)energy+=y*y;}return sqrt(energy/48000);}
int main(){
 Show::VoiceTone p;assert(Show::validTone(p));VoiceToneDSP::Processor fx;fx.configure(p);
 for(int i=0;i<10000;i++){float x=sin(i*.17f)*.6f;assert(fx.tick(x,x)==x);} // Exact legacy bypass.
 p.eqEnabled=1;p.bands[1]={1000,6,2};double boosted=response(p,1000),outside=response(p,100);assert(boosted>.068&&boosted<.074&&outside>.034&&outside<.037);
 p.bands[1].gainDb=-6;double cut=response(p,1000);assert(cut>.016&&cut<.019);
 p=Show::VoiceTone{};p.eqEnabled=1;p.highpassHz=200;assert(response(p,40)<.002);
 p.highpassHz=0;p.lowpassHz=4000;assert(response(p,16000)<.001);
 p=Show::VoiceTone{};p.presenceEnabled=1;p.presenceAmount=.5;p.presenceDrive=3;double present=response(p,2500,true),away=response(p,100,true);assert(present>.01&&away<present*.02);
 VoiceToneDSP::Processor stress;for(int block=0;block<500;block++){p.eqEnabled=block%2;p.bands[0]={float(30+block%200),12,8};p.bands[1]={float(700+block%3000),-12,.2f};p.bands[2]={16000,12,8};p.presenceAmount=block%2;p.presenceDrive=12;stress.configure(p);for(int j=0;j<256;j++){float y=stress.tick(sinf((block*256+j)*.73f)*2,sinf(j*.19f));assert(std::isfinite(y)&&fabsf(y)<1000);}}
 // The real gate must close the parallel microphone band too; output limiter stays last.
 Helmet::VoiceProcessor processor;Settings s;s.enabled=true;s.mode=DRY;s.gain=8;s.volume=1;s.gateDb=-25;s.notch=false;s.tone=p;s.tone.eqEnabled=1;s.tone.presenceEnabled=1;
 uint32_t in[512]{};int32_t out[512]{};Helmet::VoiceStats stats;
 for(int n=0;n<800;n++){for(int i=0;i<256;i++)in[2*i]=((uint32_t)(int32_t)(.002f*sinf((n*256+i)*.3f)*8388607))<<8;processor.process(s,in,out,256,stats);for(auto y:out)assert(std::abs((double)y)<=.980001*2147483647.0);}
 assert(stats.outputLevel<.0001f&&stats.inputLevel>0&&stats.errors==0);
 p.bands[0].q=0;assert(!Show::validTone(p));p=Show::VoiceTone{};p.presenceDrive=NAN;assert(!Show::validTone(p));
 puts("Real voice EQ/presence DSP: exact disabled bypass,6dB peak/cut,HP/LP rejection, selected clean-mic band, smooth parameter stress, gate closure and final limiter; no speaker/feedback/thermal claim");
}

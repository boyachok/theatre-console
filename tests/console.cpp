#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include "../console/score_runtime.h"
#include "../console/lan_wire.h"
int main(){
 assert(sizeof(Show::Cue)==314&&sizeof(Show::Packet)==358);
 const char known[]="123456789";assert(Show::crc32(known,9)==0xcbf43926);
 uint8_t wire[532]{},mac[]={0xA0,1,2,3,4,5},dest[]={0xB0,1,2,3,4,6};Show::Packet p{};p.run=55;p.transition=1;p.cue.dimmerMask=1;p.cue.dimmers[0]={255,1000,2000};p.cue.relayMask=2;p.cue.relays[1]={1,2,100,200};
 assert(Show::valid(p.cue));assert(encodeLAN(mac,dest,(uint8_t*)&p,sizeof(p),wire)==378);LanHeader h;assert(decodeLAN(wire,378,h)&&h.length==358);
 const uint8_t expectedPins[12]={5,6,7,15,16,17,18,8,9,10,11,12};for(int i=0;i<12;i++)assert(PWM_PINS[i]==expectedPins[i]);assert(RELAY_PIN==39&&RELAY2_PIN==38&&PLAYER_RX==1&&PLAYER_TX==2&&STRIP_PIN==47);
 Actors actors;actors.begin();Show::Runner runner(actors);runner.begin();assert(mockModes[RELAY2_PIN]==INPUT);assert(mockUARTRx==1&&mockUARTTx==2&&mockUARTBaud==9600);
 runner.apply(p,0);assert(mockLevels[RELAY2_PIN]==HIGH);runner.tick(50);assert(!runner.active||mockLevels[RELAY2_PIN]==HIGH);runner.tick(100);assert(mockLevels[RELAY2_PIN]==LOW);
 runner.tick(500);assert(actors.snapshot().pwm[0]>=127&&actors.snapshot().pwm[0]<=128);
 runner.apply(p,500);runner.tick(1000);assert(actors.snapshot().pwm[0]==255); // retry did not restart fade
 p.transition=2;p.cue.dimmers[0].value=0;runner.apply(p,1000);runner.tick(2000);assert(actors.snapshot().pwm[0]>=127&&actors.snapshot().pwm[0]<=128);runner.tick(3000);assert(actors.snapshot().pwm[0]==0);
 p.transition=1;p.cue.dimmers[0].value=255;runner.apply(p,3500);runner.tick(5000);assert(actors.snapshot().pwm[0]==0); // old step ignored
 p.transition=3;runner.apply(p,5000);runner.tick(7000);assert(actors.snapshot().pwm[0]==255);runner.stop();assert(actors.snapshot().pwm[0]==0&&mockLevels[RELAY2_PIN]==HIGH);runner.apply(p,8000);runner.tick(10000);assert(actors.snapshot().pwm[0]==0); // maintenance stop not undone by retry
 // Audio starts only after the boot STOP, continues for the same file, and restarts once on recovery/back.
 mockNow=0;mockUARTOut.clear();Actors audioActors;audioActors.begin();Show::Runner audio(audioActors);audio.begin();mockNow=1600;audioActors.tick(true);
 Show::Packet sound{};sound.run=99;sound.transition=1;sound.cue.audio={7,30,1,1000,300};audio.apply(sound,1600);
 auto pump=[&](uint32_t from,uint32_t end){for(mockNow=from;mockNow<=end;mockNow+=100){audioActors.tick(true);audio.tick(mockNow);}};
 auto starts=[](){int n=0;for(size_t i=1;i<mockUARTOut.size();i++)if(i>=3&&mockUARTOut[i-3]==0x7e&&mockUARTOut[i-2]==0xff&&mockUARTOut[i-1]==6&&mockUARTOut[i]==YX::PLAY_TRACK)n++;return n;};
 pump(1700,2600);assert(starts()==1);sound.transition=2;audio.apply(sound,2700);pump(2800,3600);assert(starts()==1);
 audio.apply(sound,3700,true);pump(3800,4600);assert(starts()==2);audio.apply(sound,4700);pump(4800,5600);assert(starts()==2);
 sound.transition=3;sound.reason=1;audio.apply(sound,5700);pump(5800,6600);assert(starts()==3);
 audio.stopAudio();pump(6700,7600);assert(starts()==3);sound.transition=4;sound.reason=0;audio.apply(sound,7700);pump(7800,8600);assert(starts()==4); // Explicit audio STOP cancels pending starts/ramp but does not poison the next cue.
 sound.transition=5;sound.cue.audio.track=8;audio.apply(sound,8700);audio.stopAudio();pump(8800,9600);assert(starts()==4); // Even a not-yet-transmitted PLAY cannot survive STOP.
 Actors twelve;twelve.begin();Show::Runner full(twelve);full.begin();Show::Packet all{};all.run=600;all.transition=1;all.cue.dimmerMask=4095;for(int i=0;i<12;i++)all.cue.dimmers[i].value=i*20;assert(full.apply(all,20000));for(int i=0;i<12;i++)assert(twelve.dimmerValue(i)==i*20);for(int i=8;i<12;i++)assert(mockPWMFrequency[PWM_PINS[i]]==25000);
 Show::Ws2811 output;assert(output.begin());Show::Strip fx{};fx.enabled=1;fx.action=2;fx.mode=2;fx.count=2;fx.r=255;fx.g=128;fx.brightness=100;fx.width=100;fx.duration=1000;fx.program=9;
 assert(output.apply(fx,0,false));output.tick(100);assert(mockRmtFrame.size()==49);for(int i=0;i<8;i++)assert(mockRmtFrame[i].duration0==13);assert(mockRmtFrame[8].duration0==13&&mockRmtFrame[9].duration0==6&&mockRmtFrame[48].duration0==6000);
 mockRmtBusy=true;auto resized=fx;resized.count=3;assert(!output.apply(resized,200,false));mockRmtBusy=false;fx.order=1;fx.slow=1;assert(output.apply(fx,300,false));output.tick(300);assert(mockRmtFrame[0].duration0==26&&mockRmtFrame[1].duration0==12);output.stop();output.tick(400);assert(mockRmtFrame[0].duration0==12);
 Show::SlaveStore store;Show::Cue cues[3]{};cues[1].dimmerMask=1;cues[1].dimmers[0].value=44;
 Show::Packet part{};part.version=2;part.total=3;part.checksum=Show::crc32(cues,sizeof(cues));part.step=2;part.cue=cues[2];assert(store.accept(part)&&store.nextMissing()==0);part.step=0;part.cue=cues[0];assert(store.accept(part)&&store.nextMissing()==1);part.step=1;part.cue=cues[1];assert(store.accept(part)&&store.version==2);
 Show::SlaveStore loaded;loaded.begin();assert(loaded.version==2&&loaded.cues.size()==3&&loaded.cues[1].dimmers[0].value==44);
 puts("Console: 12 PWM + WS2811 RGB/GRB, 400/800 kHz, asynchronous busy/stop; 358-byte framing, CRC, local fades/retries/stale rejection, safe second relay, stopped-state latch, out-of-order score transfer and flash reload passed");
}

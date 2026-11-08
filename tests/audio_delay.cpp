#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <vector>
#include "../console/score_runtime.h"
struct Sent {uint32_t at;uint8_t command;uint16_t value;};
struct Bench {
 Actors actor;Show::Runner runner{actor};YX::Parser parser;size_t read=0;std::vector<Sent> sent;
 explicit Bench(uint32_t at=10000){mockNow=at;mockUARTOut.clear();actor.begin();actor.player.autoPoll=false;runner.begin();pump(at,2500);sent.clear();}
 void tick(uint32_t at){mockNow=at;actor.tick(true);runner.tick(at);while(read<mockUARTOut.size())if(parser.feed(mockUARTOut[read++]))sent.push_back({at,parser.buf[3],parser.value()});}
 void pump(uint32_t at,uint32_t duration){for(uint32_t dt=0;dt<=duration;dt++)tick(at+dt);}
 void apply(Show::Packet &p,uint32_t at){mockNow=at;assert(runner.apply(p,at));}
 size_t plays()const{size_t n=0;for(auto &f:sent)if(f.command==YX::PLAY_TRACK)n++;return n;}
 Sent lastPlay()const{for(auto i=sent.rbegin();i!=sent.rend();++i)if(i->command==YX::PLAY_TRACK)return *i;assert(false);return {};}
};
Show::Packet cue(uint16_t track,uint32_t delay){Show::Packet p{};p.run=100;p.transition=1;p.cue.audio={track,30,1,1000,1000};Show::setAudioDelay(p.cue,delay);return p;}
int main(){
 Bench b;auto p=cue(7,2000);b.apply(p,13000);b.pump(13001,999);b.apply(p,14000); // duplicate cannot restart delay
 p.transition++;p.cue.dimmers[0].value=42;b.apply(p,14500);b.pump(14501,498);assert(b.plays()==0);
 b.pump(15000,300);assert(b.plays()==1&&b.lastPlay().value==7&&b.lastPlay().at>=15000&&b.lastPlay().at<15300);uint32_t playAt=b.lastPlay().at;
 b.pump(15301,369);for(const auto &f:b.sent)if(f.command==YX::VOLUME&&f.value>0)assert(f.at>=playAt+100);
 b.pump(15671,1330);assert(b.plays()==1);bool full=false;for(auto &f:b.sent)if(f.command==YX::VOLUME&&f.value==30)full=true;assert(full);
 p.transition++;p.step=1;Show::setAudioDelay(p.cue,5000);b.apply(p,18000);b.pump(18001,5999);assert(b.plays()==1); // Same already-playing file continues without another delay.
 p.transition++;p.step=2;p.cue.audio.track=8;Show::setAudioDelay(p.cue,1000);b.apply(p,25000);b.pump(25001,200);
 p.transition++;p.step=3;p.cue.audio.track=0;b.apply(p,25202);b.pump(25203,2500);assert(b.plays()==1); // Blank scene cancels pending start despite long fade-out.
 p.transition++;p.step=4;p.cue.audio.track=9;Show::setAudioDelay(p.cue,0);b.apply(p,28000);b.pump(28001,100);
 p.transition++;p.step=5;p.cue.audio.track=0;b.apply(p,28102);b.pump(28103,1800);assert(b.plays()==1); // Even an admitted but unsent PLAY is canceled by a blank cue.
 p.transition++;p.step=6;p.cue.audio.track=10;Show::setAudioDelay(p.cue,2000);b.apply(p,31000);b.pump(31001,999);
 p.transition++;Show::setAudioDelay(p.cue,4000);b.apply(p,32001);b.pump(32002,2997);assert(b.plays()==1);b.pump(35000,300);assert(b.plays()==2&&b.lastPlay().value==10&&b.lastPlay().at<35300); // LIVE delay edit stays anchored to original scene start.
 p.transition++;p.step=7;p.cue.audio.track=11;Show::setAudioDelay(p.cue,5000);b.apply(p,36000);b.pump(36001,999);
 p.transition++;Show::setAudioDelay(p.cue,500);b.apply(p,37001);b.pump(37002,300);assert(b.plays()==3&&b.lastPlay().at>=37001); // shortening past deadline starts now, never retrospectively
 p.transition++;p.step=8;p.cue.audio.track=12;Show::setAudioDelay(p.cue,1000);b.apply(p,38000);b.runner.stopAudio();b.pump(38001,2000);assert(b.plays()==3);
 p.transition++;p.step=9;p.cue.audio.track=13;b.apply(p,41000);b.runner.stop();b.pump(41001,2000);assert(b.plays()==3);b.apply(p,43500);assert(!b.runner.active);
 p.transition++;b.apply(p,44000);b.pump(44001,1400);assert(b.plays()==4); // A new explicit GO after STOP is allowed.
 // Same-file next scene while waiting restarts its own deadline, not the previous one.
 p.transition++;p.step=10;p.cue.audio.track=14;Show::setAudioDelay(p.cue,1000);b.apply(p,46000);b.pump(46001,499);
 p.transition++;p.step=11;b.apply(p,46501);b.pump(46502,998);assert(b.plays()==4);b.pump(47501,300);assert(b.plays()==5);
 assert(b.runner.apply(p,48000,true));b.pump(48001,998);assert(b.plays()==5);b.pump(49000,300);assert(b.plays()==6); // recovery restarts with delay, not immediately
 Bench wrap(0xfffff000);auto timed=cue(20,1000);wrap.apply(timed,0xffffff00);wrap.pump(0xffffff01,998);assert(wrap.plays()==0);wrap.pump(744,300);assert(wrap.plays()==1);
 // Delay belongs to CRC-protected local flash plans, without changing Cue/Packet sizes.
 Show::Cue stored{};Show::setAudioDelay(stored,3600000);assert(Show::valid(stored)&&Show::audioDelay(stored)==3600000);Show::setAudioDelay(stored,3600001);assert(!Show::valid(stored));Show::setAudioDelay(stored,1250);
 Show::SlaveStore store;Show::Packet part{};part.version=9;part.total=1;part.cue=stored;part.checksum=Show::crc32(&stored,sizeof(stored));assert(store.accept(part));Show::SlaveStore loaded;loaded.begin();assert(loaded.version==9&&Show::audioDelay(loaded.cues[0])==1250);
 auto legacy=stored;legacy.animations[0]={Show::SINE,1000,0,0,50,0};Show::setAudioDelay(legacy,0);Show::FileHeader h{Show::MAGIC,8,Show::crc32(&legacy,sizeof(legacy)),1,8};auto file=LittleFS.open("/slave-score.bin","w");file.write((uint8_t*)&h,sizeof(h));file.write((uint8_t*)&legacy,sizeof(legacy));file.close();Show::SlaveStore old;old.begin();assert(old.version==8&&Show::audioDelay(old.cues[0])==0&&old.cues[0].animations[0].mode==Show::SINE);
 puts("Audio delay actual Runner/Player: no early PLAY, fade anchored to real TX, duplicates/unrelated LIVE edits, same-file continuation, blank/queued/STOP cancellation, rescheduling/new scene/recovery-safe explicit start, wrap and CRC/score8 cache migration passed (UART mocked, no sound test)");
}

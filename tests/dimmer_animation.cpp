#define THEATRE_HOST_TEST
#include <cassert>
#include <cmath>
#include <cstdio>
#include "../console/score_runtime.h"
#include "../console/protocol.h"
#include "../console/lan_wire.h"
void near(float a,float b){assert(std::fabs(a-b)<.0001f);}
int main(){
 using namespace Show;
 DimmerOscillator wave;DimmerAnimation a{SINE,1000,0,0,50,0};
 wave.apply(a,0,true);near(wave.factor(0),0);near(wave.factor(250),.5);near(wave.factor(500),1);near(wave.factor(1000),0);
 a.phaseSeed=1800;wave.apply(a,1000,true);near(wave.factor(1000),1);
 a={SQUARE,1000,0,0,25,0};wave.apply(a,2000,true);near(wave.factor(2249),1);near(wave.factor(2250),0);near(wave.factor(3000),1);
 a={RAMP,1000,0,0,50,0};wave.apply(a,0,true);near(wave.factor(250),.5);near(wave.factor(500),1);near(wave.factor(750),.5);
 a.duty=100;wave.apply(a,0,true);near(wave.factor(250),.25);a.duty=0;wave.apply(a,0,true);near(wave.factor(250),.75);
 assert(dimmerHash(0,0)==0&&dimmerHash(1,0)==1753845952u);
 a={RANDOM,1000,42,0,50,0};wave.apply(a,0,true);float first=wave.factor(0);near(wave.factor(999),first);float next=wave.factor(1000);assert(first!=next);
 DimmerOscillator sparse;sparse.apply(a,0,true);near(sparse.factor(1000),next); // independent of tick/sampling cadence
 a.smooth=100;wave.apply(a,0,true);near(wave.factor(500),(first+next)/2);
 a={SINE,80,0,0,50,0};wave.apply(a,0xfffffffa,true);near(wave.factor(14),.5); // millis wrap
 a={SINE,1000,0,25,50,0};wave.apply(a,0,true);near(wave.factor(0),.25);near(wave.factor(500),1);
 // Same-scene parameter edits keep phase; unrelated channel edits don't retarget fades.
 wave.apply(a,0,true);near(wave.factor(250),.625);a.periodMs=2000;wave.apply(a,250,false);near(wave.factor(250),.625);near(wave.factor(750),1);
 DimmerChannel channel;channel.reset(0,0);Dimmer dim{200,0,0};a={SINE,1000,0,0,50,0};channel.apply(dim,a,true,0,true);near(channel.value(250),100);
 DimmerAnimation off{};dim.inMs=1000;channel.apply(dim,off,true,250,true);near(channel.value(250),100);near(channel.value(750),150);near(channel.value(1250),200);
 dim.value=0;dim.outMs=1000;channel.apply(dim,a,true,1250,false);near(channel.value(1750),100);near(channel.value(2250),0);
 Actors actor;actor.begin();Runner runner(actor);runner.begin();Show::Packet scene{};
 scene.run=100;scene.transition=1;scene.cue.dimmerMask=4095;
 for(int i=0;i<12;i++){scene.cue.dimmers[i].value=200;scene.cue.animations[i]={SINE,1000,(uint32_t)i*300,0,50,0};}
 scene.cue.animations[0]={SQUARE,20,0,0,50,0};assert(runner.apply(scene,1000));assert(actor.dimmerValue(0)==200&&actor.dimmerValue(3)==100);
 runner.tick(1005);assert(runner.apply(scene,1005));runner.tick(1010);assert(actor.dimmerValue(0)==0);runner.tick(1020);assert(actor.dimmerValue(0)==200);
 scene.transition=2;scene.cue.dimmers[5].value=40;assert(runner.apply(scene,1030));assert(actor.dimmerValue(0)==0);
 scene.transition=3;scene.cue.animations[0].periodMs=40;assert(runner.apply(scene,1031));assert(actor.dimmerValue(0)==0);runner.tick(1051);assert(actor.dimmerValue(0)==200);
 scene.transition=4;scene.step=1;assert(runner.apply(scene,1052));assert(actor.dimmerValue(0)==200);runner.stop();for(int i=0;i<12;i++)assert(actor.dimmerValue(i)==0);
 assert(runner.apply(scene,1060)&&!runner.active); // repeat cannot undo STOP
 scene.transition++;assert(runner.apply(scene,1061)&&runner.active);
 for(int i=8;i<12;i++)assert(mockPWMFrequency[PWM_PINS[i]]==25000);
 // Validation, the optional diagnostic116B LAN path, and large score8 transport bounds.
 Cue bad=scene.cue;bad.animations[0].periodMs=19;assert(!valid(bad));bad=scene.cue;bad.animations[0].mode=5;assert(!valid(bad));bad=scene.cue;bad.animations[0].phaseSeed=3600;assert(!valid(bad));bad.animations[0].mode=RANDOM;assert(valid(bad));
 uint8_t mac[]={0xa0,1,2,3,4,5},dest[]={0xb0,1,2,3,4,5},wire[sizeof(LanHeader)+LAN_MAX_PAYLOAD];LanHeader header{};
 auto size=encodeLAN(mac,dest,(uint8_t*)&scene,sizeof(scene),wire);assert(size==457&&decodeLAN(wire,size,header)&&header.length==437);
 for(size_t n=0;n<size;n++)assert(!decodeLAN(wire,n,header));assert(!decodeLAN(wire,size+1,header));
 uint8_t debug[116]{};assert(encodeLAN(mac,dest,debug,sizeof(debug),wire)==136&&decodeLAN(wire,136,header));assert(!encodeLAN(mac,dest,debug,115,wire));
 // Static score7 cache migrates only in RAM, preserving the old file for rollback.
 LegacyCue old[2]{};old[0].dimmerMask=1;old[0].dimmers[0]={123,400,600};FileHeader legacy{Show::MAGIC,14,crc32(old,sizeof(old)),2,0};
 auto file=LittleFS.open("/slave-score.bin","w");file.write((uint8_t*)&legacy,sizeof(legacy));file.write((uint8_t*)old,sizeof(old));file.close();
 SlaveStore store;store.begin();assert(store.version==14&&store.cues.size()==2&&store.cues[0].dimmers[0].value==123&&store.cues[0].animations[0].mode==STATIC);
 assert(LittleFS.open("/slave-score.bin","r").size()==sizeof(legacy)+sizeof(old));assert(store.checksum==crc32(store.cues.data(),store.cues.size()*sizeof(Cue)));
 store.cues[0].animations[0]={RANDOM,1000,0xffffffffu,25,50,100};Show::Packet part{};part.version=15;part.total=2;part.checksum=crc32(store.cues.data(),store.cues.size()*sizeof(Cue));auto expected=store.cues;
 part.cue=expected[1];part.step=1;assert(store.accept(part));part.cue=expected[0];part.step=0;assert(store.accept(part)&&store.version==15);SlaveStore reboot;reboot.begin();assert(reboot.version==15&&reboot.cues[0].animations[0].phaseSeed==0xffffffffu);
 RunPacket reference{};reference.type=RUN;reference.reserved=ACTIVE;reference.run=101;reference.transition=1;reference.version=15;reference.checksum=reboot.checksum;reference.total=2;Cue selected{};
 assert(reboot.resolve(reference,selected)==SlaveStore::Lookup::READY&&selected.animations[0].mode==RANDOM);auto indexed=expand(reference);indexed.cue=selected;assert(runner.apply(indexed,3000));runner.tick(3600);assert(actor.dimmerValue(0)>=30&&actor.dimmerValue(0)<=123);
 auto cache=*LittleFS.files["/slave-score.bin"];LittleFS.files["/slave-score.bin"]->push_back(0);SlaveStore trailing;trailing.begin();assert(trailing.version==0);*LittleFS.files["/slave-score.bin"]=cache;(*LittleFS.files["/slave-score.bin"])[14]=12;SlaveStore future;future.begin();assert(future.version==0);*LittleFS.files["/slave-score.bin"]=cache;LittleFS.files["/slave-score.bin"]->back()^=1;SlaveStore corrupt;corrupt.begin();assert(corrupt.version==0);
 ::Packet tag=packet(HELLO,1);setPWMFrequency(tag,25000);assert(localScoreTag(tag)&&reportedPWMFrequency(tag)==25000);tag.padding[2]=6;assert(!localScoreTag(tag));tag.padding[2]=7;assert(helmetTag(tag)&&!localScoreTag(tag));tag.padding[2]=9;assert(helmetTag(tag)&&!localScoreTag(tag));tag.padding[2]=11;assert(helmetTag(tag)&&!localScoreTag(tag));tag.padding[2]=13;assert(helmetTag(tag)&&!localScoreTag(tag));tag.padding[2]=15;assert(helmetTag(tag)&&localScoreTag(tag));
 puts("Dimmer animation: all five waves/seed/floor/duty/smoothing, continuous LIVE period, wrap, real 12-channel Runner/STOP/200ms duplicates, 25kHz carrier, score11 LAN437 + debug116, capability gate, score7 cache RAM migration + animated flash reboot passed (GPIO mocked)");
}

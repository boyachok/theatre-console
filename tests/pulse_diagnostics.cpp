#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <vector>
#include "../console/score_runtime.h"
#include "../console/protocol.h"

int main(){
 mockNow=0;Actors a;a.begin();Show::Runner runner(a);runner.begin();
 assert(a.apply(RELAY_CONFIG,0,1,true)==APPLIED);
 Show::Packet scene{};scene.run=77;scene.transition=1;scene.cue.dimmerMask=4095;scene.cue.relayMask=3;
 scene.cue.relays[0]={1,1,0,0};scene.cue.relays[1]={1,1,0,0};
 for(auto &d:scene.cue.dimmers)d.value=128;
 assert(runner.apply(scene,0));
 for(mockNow=0;mockNow<5000;mockNow++){a.tick(true);runner.tick(mockNow);}
 const auto pwm=a.pwmWriteCount(),r1=a.relayWriteCount(0),r2=a.relayWriteCount(1);
 std::vector<unsigned> queries;size_t read=mockUARTOut.size();uint32_t probes=0;bool played=false;
 for(;mockNow<=65000;mockNow++){
  if(mockNow%500==0){ // Same read-only actor operations used by the PROBE/ECHO handler.
   auto reply=packet(ECHO,55,++probes);reply.state=a.snapshot();setPWMFrequency(reply,a.pwmFrequency());
   assert(reply.state.relayOn&&reply.state.pwm[0]==128);
  }
  if(mockNow%200==0){
   if(mockNow%400==0)scene.transition++; // Another device's edits can bump the global transition.
   assert(runner.apply(scene,mockNow)); // Repeats AND new transitions with identical local cues are idempotent.
  }
  if(mockNow%PLAYER_POLL_MS==0){
   uint8_t bytes[10];YX::frame(YX::FILES,17,bytes,false);
   if((mockNow/PLAYER_POLL_MS)%2)bytes[5]=0x90; // Observed corruption before the CRC parser.
   for(auto b:bytes)mockUARTIn.push_back(b);
  }
  if(mockNow==10000){assert(a.player.startTrack(3));assert(a.player.setVolume(10));}
  a.tick(mockNow%10000<7000);runner.tick(mockNow); // Heartbeat expiry/recovery alone must hold GPIOs.
  while(read+10<=mockUARTOut.size()){
   auto b=mockUARTOut.data()+read;uint8_t cmd=b[3];YX::Parser sent;
   for(int i=0;i<10;i++)assert(sent.feed(b[i])==(i==9));
   if(cmd==YX::PLAY_TRACK){assert(sent.value()==3&&b[4]==1);played=true;assert(mockNow<=10500);}
   if(cmd==YX::STATUS||cmd==YX::QUERY_VOLUME||cmd==YX::TRACK||cmd==YX::FILES)queries.push_back(mockNow);
   read+=10;
  }
  assert(a.pwmWriteCount()==pwm&&a.relayWriteCount(0)==r1&&a.relayWriteCount(1)==r2);
  assert(mockLevels[RELAY_PIN]==HIGH&&mockLevels[RELAY2_PIN]==HIGH);
  for(int pin:PWM_PINS)assert(mockPWM[pin]==128);
 }
 assert(played&&a.player.diagnostics().checksumErrors>0&&a.player.diagnostics().validFrames>0);
 assert(queries.size()>=59000/PLAYER_POLL_MS&&queries.size()<=61000/PLAYER_POLL_MS+2);
 for(size_t i=1;i<queries.size();i++)if(queries[i]>15000)assert(queries[i]-queries[i-1]==PLAYER_POLL_MS);
 const auto before=a.pwmWriteCount();assert(a.apply(PWM,1,0,true)==APPLIED&&a.pwmWriteCount()==before+1);
 assert(a.apply(RELAY,0,0,true)==APPLIED&&a.relayWriteCount(0)==r1+1);
 assert(a.outputEventCount()>0&&a.outputEvent(a.outputEventCount()-1).kind==2);
 std::printf("Pulse regression:60s static12PWM/two relays;500ms heartbeat snapshots,200ms repeated/new-transition RUN, valid/corrupted UART at%ums and audio PLAY never rewrite outputs; RAM trace counts real software writes (not electrical waveforms)\n",(unsigned)PLAYER_POLL_MS);
}

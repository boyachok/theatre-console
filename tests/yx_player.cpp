#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <vector>
#include <array>
#include <cstring>
#include "../console/actors.h"
#include "../console/protocol.h"
void inject(uint8_t cmd,uint16_t value=0){uint8_t b[10];YX::frame(cmd,value,b,false);for(auto c:b)mockUARTIn.push_back(c);}
struct Sent {uint8_t cmd;uint16_t value;uint32_t at;};
int main(){
 assert(sizeof(Packet)==72&&sizeof(UartStats)==76&&sizeof(DebugPacket)==100&&sizeof(ExtendedDebugPacket)==116);
 const uint8_t pins[]={5,6,7,15,16,17,18,8,9,10,11,12};for(int i=0;i<12;i++)assert(PWM_PINS[i]==pins[i]);
 assert(RELAY_PIN==39&&RELAY2_PIN==38&&PLAYER_RX==1&&PLAYER_TX==2&&STRIP_PIN==47&&RGB_PIN==48);
 uint8_t b[10];YX::frame(YX::VOLUME,10,b);const uint8_t known[]={0x7e,0xff,6,6,1,0,10,0xfe,0xea,0xef};assert(!memcmp(b,known,10));
 YX::Parser parser;for(int i=0;i<10;i++)assert(parser.feed(b[i])==(i==9));assert(parser.buf[3]==YX::VOLUME&&parser.value()==10);
 b[8]^=1;for(auto c:b)assert(!parser.feed(c));assert(parser.checksumErrors==1);
 YX::frame(YX::TRACK,0x7eef,b,false);for(int i=0;i<10;i++)assert(parser.feed(b[i])==(i==9));assert(parser.value()==0x7eef); // Markers inside data are legal.
 const uint8_t malformed[]={0x7e,0xff,0,0x55,0xef};for(auto c:malformed)parser.feed(c);for(int i=0;i<10;i++)assert(parser.feed(b[i])==(i==9));assert(parser.lengthErrors>0);
 mockNow=0;Player p;p.autoPoll=false;p.begin();assert(mockUARTBaud==9600&&mockUARTConfig==SERIAL_8N1&&mockUARTRx==1&&mockUARTTx==2&&!p.bootSettled());
 std::vector<Sent> sent;size_t read=0;
 auto tick=[&](uint32_t now){mockNow=now;p.tick();while(read+10<=mockUARTOut.size()){auto q=mockUARTOut.data()+read;YX::Parser wire;for(int i=0;i<10;i++)assert(wire.feed(q[i])==(i==9));assert(q[4]==1);sent.push_back({q[3],wire.value(),now});read+=10;}};
 tick(0);assert(sent.back().cmd==YX::STOP);
 for(uint32_t t=10;t<=2200;t+=10)tick(t);
 assert(p.bootSettled()&&!p.ready()&&sent.size()==5); // STOP early, then STOP/mute/select/STOP; never PLAY on boot.
 assert(sent[1].cmd==YX::STOP&&sent[2].cmd==YX::VOLUME&&sent[2].value==0&&sent[3].cmd==YX::SELECT&&sent[3].value==2&&sent[4].cmd==YX::STOP&&sent[4].at-sent[3].at>=200);
 inject(YX::STATUS,0x0201);inject(YX::QUERY_VOLUME,12);inject(YX::TRACK,7);inject(YX::FILES,23);tick(2210);
 assert(p.ready()&&p.state==1&&p.volume==12&&p.track==7&&p.tracks==23&&p.drive==1);
 assert(p.setVolume(22));tick(2300);auto reports=p.diagnostics().replies;
 const auto echo=std::array<uint8_t,10>{mockUARTOut[read-10],mockUARTOut[read-9],mockUARTOut[read-8],mockUARTOut[read-7],mockUARTOut[read-6],mockUARTOut[read-5],mockUARTOut[read-4],mockUARTOut[read-3],mockUARTOut[read-2],mockUARTOut[read-1]};
 for(auto c:echo)mockUARTIn.push_back(c);tick(2301);assert(p.diagnostics().loopbacks==1&&p.diagnostics().replies==reports&&p.volume==12); // TX is not reported as applied volume.
 assert(p.startTrack(9));assert(p.freeSlots()==5);assert(p.setVolume(7)&&p.setVolume(8)&&p.freeSlots()==4);inject(YX::FILE_END,7);tick(2302);assert(p.freeSlots()==4); // Late old EOF cannot cancel the new queued file.
 auto beforePlay=p.playSerial;for(uint32_t t=2400;t<=2600;t+=100)tick(t);
 assert(sent[sent.size()-3].cmd==YX::VOLUME&&sent[sent.size()-3].value==0&&sent[sent.size()-2].cmd==YX::REPEAT&&sent[sent.size()-2].value==1&&sent.back().cmd==YX::PLAY_TRACK&&sent.back().value==9);
 assert(p.playSerial==beforePlay+1&&p.playSentAt==sent.back().at);
 inject(YX::ACK);inject(YX::ERROR,5);tick(2610);assert(p.debug().acks==1&&p.debug().errors==1&&p.debug().lastError==5);
 inject(YX::FILE_END,9);tick(2700);assert(sent.back().cmd==YX::STOP&&p.state==0);size_t stopped=sent.size();for(uint32_t t=2800;t<=3200;t+=100)tick(t);assert(sent.size()==stopped); // No silent next-track/restart command after EOF.
 assert(p.startTrack(8,true));for(uint32_t t=3300;t<=3500;t+=100)tick(t);inject(YX::FILE_END,8);tick(3510);assert(p.freeSlots()==8&&sent.back().cmd==YX::PLAY_TRACK); // Explicit repeat mode does not issue STOP on EOF.
 assert(p.setVolume(4)&&p.setVolume(8)&&p.freeSlots()==7);p.stop();tick(3580);
 for(int i=0;i<8;i++)assert(p.setEq(i%5));assert(p.freeSlots()==0&&!p.startTrack(4));p.stop();assert(p.freeSlots()==7);tick(3600);assert(sent.back().cmd==YX::STOP);
 // A complete 10B TX is reconstructed without changing legacy wire layouts.
 auto stats=p.diagnostics();auto extra=p.debug();uint8_t full[10];memcpy(full,stats.lastTx,8);memcpy(full+8,extra.txTail,2);YX::Parser reconstructed;for(int i=0;i<10;i++)assert(reconstructed.feed(full[i])==(i==9));
 ExtendedDebugPacket ext{};ext.legacy.magic=MAGIC;ext.legacy.protocol=PROTOCOL;ext.legacy.type=DEBUG_REPORT;ext.legacy.size=sizeof(ext);ext.legacy.uart=stats;ext.player=extra;
 DebugPacket decoded{};PlayerDebug detail{};assert(decodeDebugReport((uint8_t*)&ext,sizeof(ext),decoded,detail)&&detail.family==1&&detail.errors==1&&decoded.uart.txLen==10);
 DebugPacket old=ext.legacy;old.size=sizeof(old);assert(decodeDebugReport((uint8_t*)&old,sizeof(old),decoded,detail)&&detail.family==0);ext.legacy.size--;assert(!decodeDebugReport((uint8_t*)&ext,sizeof(ext),decoded,detail));
 // Partial RX timeout/checksum corruption cannot refresh the player; partial TX is counted.
 YX::frame(YX::STATUS,0,b,false);for(int i=0;i<5;i++)mockUARTIn.push_back(b[i]);tick(3610);tick(3720);assert(p.diagnostics().frameTimeouts==1);
 auto bad=b[8];b[8]^=1;for(auto c:b)mockUARTIn.push_back(c);tick(3730);assert(p.diagnostics().checksumErrors==1);b[8]=bad;
 mockWriteLimit=4;assert(p.setVolume(10));mockNow=3800;p.tick();assert(p.diagnostics().txFailures==1);mockWriteLimit=1000;
 p.clearStats();assert(p.diagnostics().txFrames==0&&p.debug().errors==0&&p.volume==12&&p.track==7);
 // An unsolicited reboot reinitializes safely; no automatic PLAY transaction is emitted.
 mockUARTOut.clear();read=0;sent.clear();inject(YX::INIT,2);tick(3900);assert(!p.bootSettled()&&p.state==255&&p.volume==255&&p.track==0);for(uint32_t t=4000;t<=6000;t+=100)tick(t);assert(p.bootSettled());for(auto f:sent)assert(f.cmd!=YX::PLAY_TRACK&&f.cmd!=YX::RESUME);
 // Echo-only and checksumless traffic cannot invent player presence.
 mockNow=7000;mockUARTOut.clear();Player silent;silent.autoPoll=false;silent.begin();for(uint32_t t=7000;t<=9300;t+=10){mockNow=t;silent.tick();}
 auto count=mockUARTOut.size();for(size_t i=0;i<10;i++)mockUARTIn.push_back(mockUARTOut[count-10+i]);silent.tick();assert(!silent.ready()&&silent.diagnostics().loopbacks==1&&silent.diagnostics().replies==0);
 // Actual Actors keep logical NVS presets/inversions while moving physical pins, and translate service ops.
 PwmSettings stored;stored.level[0]=91;stored.inverted=1;auto bytes=(const uint8_t*)&stored;mockPrefsBytes["pwmConfig"]=std::vector<uint8_t>(bytes,bytes+sizeof(stored));
 mockNow=10000;mockUARTOut.clear();Actors actor;actor.begin();actor.player.autoPoll=false;
 assert(actor.pwmStatus().level[0]==91&&(actor.pwmStatus().inverted&1)&&actor.dimmerValue(0)==0&&mockPWM[5]==255);
 for(int pin:{4,13,14,21})assert(mockModes[pin]==INPUT);
 assert(actor.apply(AUDIO_TRACK,0,4,true)==PLAYER_OFFLINE);for(mockNow=10000;mockNow<=12500;mockNow+=10)actor.tick(true);
 assert(!actor.player.ready()&&actor.player.bootSettled());assert(actor.apply(AUDIO_VOLUME,0,10,true)==APPLIED&&actor.snapshot().audioVolume==255);assert(actor.apply(AUDIO_TRACK,0,4,true)==APPLIED);
 assert(actor.apply(AUDIO_MODE,0,0,true)==INVALID&&actor.apply(AUDIO_DRIVE,0,0,true)==INVALID);
 for(;mockNow<=13500;mockNow+=10)actor.tick(true);YX::Parser commands;bool played=false,volume10=false;
 for(auto c:mockUARTOut)if(commands.feed(c)){if(commands.buf[3]==YX::PLAY_TRACK&&commands.value()==4)played=true;if(commands.buf[3]==YX::VOLUME&&commands.value()==10)volume10=true;}
 assert(played&&volume10&&actor.snapshot().audioVolume==255); // One-way TX allowed; unknown reply state stays unknown.
 assert(actor.apply(RELAY_CONFIG,0,1,true)==APPLIED&&mockModes[39]==OUTPUT&&mockLevels[39]==LOW);assert(actor.apply(RELAY,0,1,true)==APPLIED&&mockLevels[39]==HIGH);actor.off();assert(mockLevels[39]==LOW);
 // An offline actor calls ALL_OFF every tick: this must not restart/starve the safe bootstrap.
 mockNow=15000;mockUARTOut.clear();Actors disconnected;disconnected.begin();disconnected.player.autoPoll=false;
 assert(!disconnected.player.resume()&&!disconnected.player.next()&&!disconnected.player.previous()&&!disconnected.player.selectTF());
 for(mockNow=15000;mockNow<=17500;mockNow+=10)disconnected.tick(false);
 assert(disconnected.player.bootSettled()&&!disconnected.player.ready());YX::Parser offline;bool muted=false,selected=false;
 for(auto c:mockUARTOut)if(offline.feed(c)){assert(offline.buf[3]!=YX::PLAY_TRACK&&offline.buf[3]!=YX::RESUME&&offline.buf[3]!=YX::NEXT);if(offline.buf[3]==YX::VOLUME&&offline.value()==0)muted=true;if(offline.buf[3]==YX::SELECT&&offline.value()==2)selected=true;}
 assert(muted&&selected);
 // An EOF arriving before a queued loop-enable TX must still stop the single file, not pretend the mode has applied.
 mockNow=18000;mockUARTOut.clear();Player race;race.autoPoll=false;race.begin();for(mockNow=18000;mockNow<=20500;mockNow+=10)race.tick();assert(race.startTrack(7));for(mockNow=21000;mockNow<=21500;mockNow+=10)race.tick();assert(race.setRepeat(true));inject(YX::FILE_END,7);mockNow=21501;race.tick();assert(race.state==0&&race.diagnostics().lastTx[3]==YX::STOP&&race.freeSlots()==8);
 assert(race.startTrack(8,true));for(mockNow=22000;mockNow<=22500;mockNow+=10)race.tick();assert(race.setRepeat(false));inject(YX::FILE_END,8);mockNow=22501;race.tick();assert(race.diagnostics().lastTx[3]==YX::REPEAT);inject(YX::FILE_END,8);mockNow=22600;race.tick();assert(race.state==0&&race.diagnostics().lastTx[3]==YX::STOP);
 puts("YX6300 actual driver: RX1/TX2 + complete slave pin map, 10B checksum/parser/resync, safe muted TF initialization/200ms settle, real replies vs echoes, atomic start/actual TX timestamp, volume telemetry, stop/EOF/repeat/reboot, queue limits, RX/TX failures, legacy100B + extended116B diagnostics passed (UART mocked; no physical sound proof)");
}

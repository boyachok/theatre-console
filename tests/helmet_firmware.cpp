#include <cassert>
#include <cstdio>
#include "../helmet/helmet.ino"
const uint8_t MASTER[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};
template<class T>void inject(const T &p){uint8_t wire[sizeof(LanHeader)+LAN_MAX_PAYLOAD];auto n=encodeLAN(MASTER,network.mac,(const uint8_t*)&p,sizeof(p),wire);assert(n);mockUDPIn.push_back({IPAddress(192,168,50,119),LAN_PORT,std::vector<uint8_t>(wire,wire+n)});}
void tick(uint32_t at){mockNow=at;loop();}
Packet last(Type type){for(auto i=mockUDPOut.rbegin();i!=mockUDPOut.rend();++i){if(i->data.size()!=sizeof(Packet)+sizeof(LanHeader))continue;Packet p;memcpy(&p,i->data.data()+sizeof(LanHeader),sizeof(p));if(p.type==type)return p;}assert(false);return {};}
Show::Packet scene(uint32_t sequence){Show::Packet p{};p.magic=Show::MAGIC;p.protocol=Show::PROTOCOL;p.size=sizeof(p);p.type=Show::PREVIEW;p.session=77;p.deviceBoot=selfBoot;p.run=200;p.transition=sequence;p.reserved=1;p.step=sequence-1;p.cue.strip={1,2,0,255,0,0,0,0,0,20,20,0,0,0,60,1000,1};return p;}
int main(){
 // Same filesystem safeguard as the master: format only genuinely erased flash.
 LittleFS.mountOK=false;mockPartitionData[42]=1;assert(!mountShowFS()&&LittleFS.formats==0);
 mockPartitionData.fill(255);mockPartitionReadOK=false;assert(!mountShowFS()&&LittleFS.formats==0);
 mockPartitionReadOK=true;assert(mountShowFS()&&LittleFS.formats==1);
 setup();assert(HELMET_STRIP_PIN==47&&mockRmtPin==47&&runner.ready&&storageReady&&!runner.active);
 assert(WiFi.ssid=="YOUR_WIFI_SSID"&&WiFi.password=="YOUR_WIFI_PASSWORD"&&WiFi.host.find("stage-helmet-")==0);
 assert(!runner.voice.available()&&!runner.voice.requested()&&mockUARTOut.empty());
 WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,50,65);tick(1000);assert(MDNS.services==0);
 inject(packet(DISCOVER,77));tick(1001);tick(1500);assert(helmetTag(last(HELLO))&&last(HELLO).deviceBoot==selfBoot);
 Packet probe=packet(PROBE,77,1);probe.nodeId=9;probe.health=EXCELLENT;inject(probe);tick(1501);assert(helmetTag(last(ECHO))&&last(ECHO).nodeId==9&&linkFresh());
 auto p=scene(1);p.cue.audio.enabled=1;inject(p);tick(1510);assert(runner.active&&runner.sequence==1&&runner.voice.requested()&&!runner.voice.available());
 auto ack=last(SCORE_ACK);assert(ack.result==APPLIED&&(ack.reserved&1)&&ack.seq==1&&ack.state.audioEnabled==0&&ack.state.audioState==255);
 int frames=mockRmtWrites;inject(p);tick(1511);assert(runner.sequence==1&&mockRmtWrites==frames); // No restart on duplicate.
 auto unsupported=scene(2).cue;unsupported.audioLoop=1;assert(!Helmet::supported(unsupported));unsupported.audioLoop=0;unsupported.lightDelayMs[0]=500;assert(!Helmet::supported(unsupported));unsupported.lightDelayMs[0]=0;unsupported.ac.delayMs=500;assert(!Helmet::supported(unsupported));
 auto bad=scene(2);bad.cue.dimmerMask=1;bad.cue.dimmers[0].value=255;inject(bad);tick(1512);assert(last(SCORE_ACK).result==NOT_READY&&runner.sequence==1);
 bad=scene(2);bad.cue.audio.track=1;inject(bad);tick(1513);assert(last(SCORE_ACK).result==NOT_READY); // Not an XY player.
 bad=scene(2);bad.deviceBoot++;inject(bad);tick(1514);assert(runner.sequence==1);
 bad=scene(2);bad.session++;inject(bad);tick(1515);assert(runner.sequence==1);
 auto resize=scene(2);resize.cue.strip.count=127;mockRmtBusy=true;inject(resize);tick(1516);assert(last(SCORE_ACK).result==NOT_READY&&runner.sequence==1);
 mockRmtBusy=false;inject(resize);tick(1517);assert(last(SCORE_ACK).result==APPLIED&&mockRmtFrame.size()==127*24+1);
 inject(p);tick(1518);assert(runner.sequence==2); // Older transition cannot roll back.
 Show::Cue cues[2]={p.cue,resize.cue};Show::Packet store=p;store.type=Show::STORE;store.version=3;store.total=2;store.checksum=Show::crc32(cues,sizeof(cues));
 store.step=1;store.cue=cues[1];inject(store);tick(1520);assert(scoreStore.version==0&&last(SCORE_ACK).value==0);
 store.step=0;store.cue=cues[0];inject(store);tick(1521);assert(scoreStore.version==3&&(last(SCORE_ACK).reserved&2));
 Show::SlaveStore reboot;reboot.begin();assert(reboot.version==3&&reboot.cues.size()==2); // Persistence does not execute at boot.
 auto indexed=Show::reference(scene(3));indexed.type=Show::RUN;indexed.version=3;indexed.checksum=store.checksum;indexed.total=2;indexed.step=0;
 inject(indexed);tick(1522);assert(last(SCORE_ACK).result==APPLIED&&runner.sequence==3&&runner.voice.requested());
 indexed.transition=4;indexed.checksum++;inject(indexed);tick(1523);assert(last(SCORE_ACK).result==NOT_READY&&(last(SCORE_ACK).reserved&Show::MISSING_PLAN)&&runner.sequence==3);
 WiFi.state=0;tick(1600);assert(!linkFresh()&&runner.active);int before=mockRmtWrites;tick(1700);assert(mockRmtWrites>before);
 WiFi.state=WL_CONNECTED;tick(2000);inject(packet(DISCOVER,77));tick(2001);tick(2400);
 auto off=scene(4);off.cue.strip.action=1;off.cue.audio.enabled=0;inject(off);tick(2410);assert(runner.sequence==4&&!runner.voice.requested());
 Packet stop=packet(COMMAND,77);stop.deviceBoot=selfBoot;stop.commandId=10;stop.op=ALL_OFF;inject(stop);tick(2420);assert(!runner.active&&last(COMMAND_ACK).result==APPLIED);
 auto next=scene(5);inject(next);tick(2421);assert(runner.active);inject(stop);tick(2422);assert(runner.active); // Cached OFF must not stop a later scene.
 for(auto v:mockPWMFrequency)assert(v==0);assert(mockLEDCClock==0&&mockUARTOut.empty());
 for(int pin=0;pin<49;pin++)if(pin!=47&&pin!=48)assert(mockModes[pin]==INPUT);
 puts("Actual helmet firmware: GPIO47 WS2811/configurable count, no PWM/relay/UART/HTTP, voice unavailable (desired only), master binding/ACK/dedup/stale protection, local animation/recovery, CRC score persistence and non-erasing FS guard passed");
}

#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include "../remote/remote.ino"
const uint8_t MASTER[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};
void inject(const Packet &p){uint8_t wire[300];auto n=encodeLAN(MASTER,network.mac,(const uint8_t*)&p,sizeof(p),wire);assert(n);mockUDPIn.push_back({IPAddress(192,168,50,119),LAN_PORT,std::vector<uint8_t>(wire,wire+n)});}
void tick(uint32_t at){mockNow=at;loop();}
void heartbeat(uint32_t at){static uint32_t seq=0;auto p=packet(REMOTE_PING,77,++seq);p.deviceBoot=selfBoot;p.commandId=900+seq;p.padding[0]='R';p.padding[1]=1;p.health=EXCELLENT;inject(p);tick(at);}
void auth(const char *enabled){char mac[18];remoteMAC(mac);web.args={{"token",otaToken},{"target",mac},{"role","remote"},{"enabled",enabled}};web.headers.clear();}
void request(){web.request("/api/remote/buttons-test",HTTP_POST);}
void ack(){auto p=packet(REMOTE_ACK,77);p.deviceBoot=selfBoot;p.result=STALE;p.padding[0]='R';p.padding[1]=1;inject(p);tick(mockNow+1);}
int main(){
 mockLevels[13]=mockLevels[14]=HIGH;mockNow=100;setup();assert(buttonTest);assert(setButtonTest(false));WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,50,88);tick(1000);inject(packet(DISCOVER,77));tick(1001);tick(1500);heartbeat(1501);assert(!buttonTest&&canOperate(mockNow));
 auth("bad");request();assert(web.code==400&&!buttonTest);
 auth("1");web.args["token"]="bad";request();assert(web.code==403&&!buttonTest);
 auth("1");mockPrefsWriteOK=false;request();assert(web.code==500&&!buttonTest);mockPrefsWriteOK=true;
 request();assert(web.code==200&&buttonTest&&mockPrefsBytes["buttonTest"][0]==1&&web.response.find("\"buttonsEnabled\":false")!=std::string::npos);
 tick(1510);assert(mockRgbColor==0x200020);
 mockLevels[13]=LOW;tick(1600);assert(buttonProbe.edges[0]==1&&buttonProbe.presses[0]==0);tick(1630);assert(buttonProbe.presses[0]==1&&mockRgbColor==0x606060);
 for(uint32_t t=2000;t<5500;t+=500)heartbeat(t);assert(sentCommands==0&&outbox.size()==0); // Holds never navigate.
 mockLevels[13]=HIGH;tick(5510);tick(5540);assert(buttonProbe.releases[0]==1&&diagSent==1&&diagPending);ack();assert(diagAcks==1&&diagResult==STALE);
 heartbeat(5600);mockLevels[14]=LOW;tick(5610);tick(5640);assert(buttonProbe.presses[1]==1&&mockRgbColor==0x606060);
 mockLevels[13]=LOW;tick(5650);tick(5680);assert(mockRgbColor==0x606000);
 mockLevels[13]=mockLevels[14]=HIGH;tick(5690);tick(5720);assert(diagSent==2);ack();tick(5810);assert(mockRgbColor==0x200020);
 for(const auto &w:mockUDPOut){Packet p;if(w.data.size()!=sizeof(LanHeader)+sizeof(p))continue;memcpy(&p,w.data.data()+sizeof(LanHeader),sizeof(p));if(p.type==REMOTE_EVENT)assert(p.commandId==0&&p.op==Remote::NONE&&p.seq==0&&p.uptime==0);}
 // Reboot reloads the NVS lock, rather than silently re-enabling navigation.
 buttonTest=false;setup();assert(buttonTest);
 mockLevels[13]=LOW;tick(5900);tick(5930);auth("0");mockPrefsWriteOK=false;request();assert(web.code==500&&buttonTest);mockPrefsWriteOK=true;request();assert(web.code==200&&!buttonTest&&mockPrefsBytes["buttonTest"][0]==0);
 for(uint32_t t=6000;t<10000;t+=500)heartbeat(t);assert(sentCommands==0);mockLevels[13]=HIGH;tick(10010);tick(10040);assert(sentCommands==0&&outbox.size()==0); // Held exit/release cannot replay.
 Remote::ButtonProbe probe;probe.reset(false,false,0xfffffff0);probe.tick(true,false,0xfffffff5);probe.tick(true,false,20);assert(probe.presses[0]==1);probe.tick(false,false,21);probe.tick(true,false,22);probe.tick(true,false,60);assert(probe.edges[0]==3&&probe.presses[0]==1&&probe.releases[0]==0);
 puts("Actual remote button diagnostic: authorized persistent NVS navigation lock, write failures, reboot restore, raw/debounced counters and wrap/bounce, held bright white/both yellow/idle purple, only zero-ID UDP events, no 3s navigation or held-exit replay passed");
}

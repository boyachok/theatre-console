#include <cassert>
#include <set>
#include <cstdio>
#include "../remote/remote.ino"
const uint8_t MASTER[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};
void inject(const Packet &p){uint8_t wire[256];auto n=encodeLAN(MASTER,network.mac,(const uint8_t*)&p,sizeof(p),wire);assert(n);mockUDPIn.push_back({IPAddress(192,168,50,119),LAN_PORT,std::vector<uint8_t>(wire,wire+n)});}
void tick(uint32_t at){mockNow=at;loop();}
Packet last(Type type){for(auto i=mockUDPOut.rbegin();i!=mockUDPOut.rend();++i){Packet p;assert(i->data.size()>=sizeof(p)+sizeof(LanHeader));memcpy(&p,i->data.data()+sizeof(LanHeader),sizeof(p));if(p.type==type)return p;}assert(false);return {};}
uint32_t pingSeq=0;
void heartbeat(uint32_t at){Packet p=packet(REMOTE_PING,77,++pingSeq);p.deviceBoot=selfBoot;p.commandId=900+pingSeq;p.uptime=at;p.health=EXCELLENT;p.padding[0]='R';p.padding[1]=1;inject(p);tick(at);}
void acknowledge(uint32_t id,uint32_t at){Packet p=packet(REMOTE_ACK,77,id);p.deviceBoot=selfBoot;p.commandId=id;p.result=APPLIED;p.value=1;p.padding[0]='R';p.padding[1]=1;inject(p);tick(at);}
int main(){
 mockPrefsBytes["buttonTest"]={0}; // Already commissioned: normal navigation fixture.
 mockLevels[13]=mockLevels[14]=HIGH;mockNow=100;setup();assert(mockModes[13]==INPUT_PULLUP&&mockModes[14]==INPUT_PULLUP);assert(WiFi.ssid=="YOUR_WIFI_SSID"&&WiFi.password=="YOUR_WIFI_PASSWORD"&&WiFi.host.find("stage-remote-")==0);
 WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,50,88);tick(1000);assert(MDNS.services==1&&httpReady&&!masterKnown);
 inject(packet(DISCOVER,77));tick(1001);tick(1500);auto hello=last(REMOTE_HELLO);assert(remoteTag(hello)&&hello.deviceBoot==selfBoot);
 heartbeat(1501);assert(last(REMOTE_PONG).seq==pingSeq);tick(1535);assert(canOperate(mockNow)&&mockRgbColor==0x14);
 mockLevels[13]=LOW;tick(1600);tick(1640);mockLevels[13]=HIGH;tick(1700);tick(1740);auto forward=last(REMOTE_EVENT);assert(forward.op==Remote::FORWARD&&forward.commandId==1&&mockRgbColor==0x141414);
 tick(1830);assert(mockRgbColor==0x14);tick(1900);assert(last(REMOTE_EVENT).commandId==1&&mockRgbColor==0x14);acknowledge(1,1901);assert(outbox.size()==0&&ackedCommands==1&&lastResult==APPLIED);
 mockLevels[14]=LOW;heartbeat(2000);tick(2040);for(uint32_t t=2500;t<=5000;t+=500)heartbeat(t);tick(5040);auto first=last(REMOTE_EVENT);assert(first.op==Remote::FIRST&&first.commandId==2&&mockRgbColor==0x141414);
 mockLevels[14]=HIGH;tick(5100);acknowledge(2,5110);acknowledge(2,5111);tick(5140);assert(outbox.size()==0&&longConfirmations==1);
 tick(5230);assert(mockRgbColor==0x606060);tick(5350);assert(mockRgbColor==0x14);tick(5470);assert(mockRgbColor==0x606060);tick(5590);assert(mockRgbColor==0x14); // Double bright confirmation only after matched successful long ACK.
 WiFi.state=0;tick(5600);mockLevels[13]=LOW;tick(5900);assert(!canOperate(mockNow)&&outbox.size()==0);
 WiFi.state=WL_CONNECTED;tick(6000);inject(packet(DISCOVER,77));tick(6001);tick(6400);heartbeat(6500);for(uint32_t t=7000;t<=10000;t+=500)heartbeat(t);mockLevels[13]=HIGH;tick(10100);tick(10140);assert(outbox.size()==0);
 mockLevels[13]=LOW;tick(10200);tick(10240);for(uint32_t t=10500;t<=13000;t+=500)heartbeat(t);tick(13240);auto rejected=last(REMOTE_EVENT);assert(rejected.op==Remote::LAST&&rejected.commandId==3);
 Packet negative=packet(REMOTE_ACK,77,3);negative.deviceBoot=selfBoot;negative.commandId=3;negative.result=NOT_READY;negative.padding[0]='R';negative.padding[1]=1;inject(negative);tick(13241);assert(outbox.size()==0&&longConfirmations==1&&lastResult==NOT_READY);tick(13370);assert(mockRgbColor==0x14); // A matched negative long ACK must not produce a confirmation.
 std::set<uint32_t> ids;for(auto &wire:mockUDPOut){Packet p;memcpy(&p,wire.data.data()+sizeof(LanHeader),sizeof(p));if(p.type==REMOTE_EVENT)ids.insert(p.commandId);}assert(ids==std::set<uint32_t>({1,2,3}));for(auto value:mockPWMFrequency)assert(value==0);
 puts("Actual remote firmware: dedicated input-only GPIO13/14, same WLAN, maintenance HTTP/OTA advertisement, discovery/boot-bound handshake, ping echo, forward/3s-first, one white pulse (not retry/ACK), no offline/held-reconnect phantom command, no PWM outputs passed");
}

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "../remote/remote.ino"
const uint8_t MASTER[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};
void inject(const Packet &p){uint8_t wire[300];auto n=encodeLAN(MASTER,network.mac,(const uint8_t*)&p,sizeof(p),wire);assert(n);mockUDPIn.push_back({IPAddress(192,168,50,119),LAN_PORT,std::vector<uint8_t>(wire,wire+n)});}
void tick(uint32_t at){mockNow=at;loop();}
void heartbeat(uint32_t at){static uint32_t seq=0;auto p=packet(REMOTE_PING,77,++seq);p.deviceBoot=selfBoot;p.commandId=900+seq;p.padding[0]='R';p.padding[1]=1;p.health=EXCELLENT;inject(p);tick(at);}
void pump(uint32_t at,uint32_t duration){for(uint32_t dt=0;dt<=duration;dt+=25){if(dt%500==0)heartbeat(at+dt);else tick(at+dt);}}
void auth(const char *point){char mac[18];remoteMAC(mac);web.args={{"token",otaToken},{"target",mac},{"role","remote"},{"point",point}};web.headers.clear();}
void capture(){web.request("/api/remote/battery/calibrate",HTTP_POST);}
int main(){
 mockPrefsBytes["buttonTest"]={0};mockLevels[13]=mockLevels[14]=HIGH;mockAnalogMv[4]=2100;mockNow=100;setup();
 assert(mockModes[4]==INPUT&&mockAnalogResolution==12&&mockAnalogAttenuation[4]==ADC_11db);
 WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,50,88);tick(1000);inject(packet(DISCOVER,77));tick(1001);tick(1500);heartbeat(1501);
 auto original=mockPrefsBytes;pump(1600,4500);assert(battery.stable(mockNow)&&!battery.enabled()&&mockPrefsBytes==original&&sentCommands==0);
 web.request("/battery",HTTP_GET);assert(web.code==200&&web.response.find("</script>")!=std::string::npos&&web.response.find("capture('full')")!=std::string::npos); // HTML must not truncate UTF-8/page tail.
 if(const char *path=std::getenv("BATTERY_HTML_DUMP")){std::ofstream file(path);file<<web.response;}
 web.request("/",HTTP_GET);assert(web.response.find("/battery")!=std::string::npos&&web.response.find("</form>")!=std::string::npos);
 assert(!web.routes.count(std::string("/api/remote/battery/calibrate")+std::to_string(HTTP_GET)));
 auth("full");web.args["role"]="master";capture();assert(web.code==403);
 auth("full");web.args["token"]="wrong";capture();assert(web.code==403&&!battery.enabled());
 auth("full");web.args["target"]="A0:85:E3:E2:BC:D4";capture();assert(web.code==403);
 auth("full");web.headers["Origin"]="http://evil.invalid";capture();assert(web.code==403);
 auth("wat");capture();assert(web.code==400);
 auth("full");mockPrefsWriteOK=false;capture();assert(web.code==500&&!battery.enabled());mockPrefsWriteOK=true;capture();assert(web.code==200&&battery.enabled()&&battery.calibration.fullMv==4200&&mockPrefsBytes["buttonTest"][0]==0);
 auto saved=mockPrefsBytes;pump(6200,1000);assert(mockPrefsBytes==saved&&mockRgbColor==0x001400);
 // Chord cannot emit forward/back/3s navigation, including staggered release and timeout while held.
 mockLevels[13]=mockLevels[14]=LOW;tick(7300);heartbeat(7800);tick(8299);assert(!linkDiagnostic.active(mockNow));heartbeat(8300);assert(linkDiagnostic.active(mockNow)&&outbox.size()==0);
 pump(8350,3650);assert(sentCommands==0);mockLevels[13]=HIGH;heartbeat(12500);
 pump(12525,5975);assert(!linkDiagnostic.active(mockNow)&&sentCommands==0);
 mockLevels[14]=HIGH;tick(18600);tick(18630);tick(18660);assert(sentCommands==0);
 mockLevels[13]=LOW;tick(18700);tick(18740);mockLevels[13]=HIGH;tick(18800);tick(18840);assert(sentCommands==1&&mockRgbColor==0x141414);
 auto ack=packet(REMOTE_ACK,77,1);ack.deviceBoot=selfBoot;ack.commandId=1;ack.result=APPLIED;ack.padding[0]='R';ack.padding[1]=1;inject(ack);tick(18850);tick(18930);assert(mockRgbColor==0x001400&&outbox.size()==0);
 mockAnalogMv[4]=1850;pump(19000,12000);auth("low");capture();assert(web.code==200&&battery.calibrated()&&battery.calibration.lowMv==3700);saved=mockPrefsBytes;
 auto stored=battery.calibration;battery=RemoteBattery::Monitor{};batteryBegin();assert(battery.calibrated()&&!memcmp(&stored,&battery.calibration,sizeof(stored)));assert(!battery.fresh(mockNow));
 mockAnalogMv[4]=0;pump(31500,5000);auth("full");capture();assert(web.code==409&&mockPrefsBytes==saved);
 assert(remoteStatusJSON().c_str()!=nullptr);web.request("/api/status",HTTP_GET);assert(web.response.find("\"reservePct\":null")!=std::string::npos&&web.response.find("\"ledMode\":\"battery\"")!=std::string::npos);
 for(auto v:mockPWMFrequency)assert(v==0);assert(mockUARTOut.empty());
 puts("Actual remote battery/chord firmware: ADC1 config/filter; auth/Origin/range/write failure; explicit NVS FULL/LOW and reload; no auto writes; complete calibration HTML;1s chord/10s diagnostic no navigation or held-release replay; normal navigation/white TX restored; no PWM/UART");
}

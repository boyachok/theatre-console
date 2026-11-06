#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include "../remote/remote.ino"
const uint8_t MASTER[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};
void inject(const Packet &p){uint8_t wire[300];auto n=encodeLAN(MASTER,network.mac,(const uint8_t*)&p,sizeof(p),wire);assert(n);mockUDPIn.push_back({IPAddress(192,168,50,119),LAN_PORT,std::vector<uint8_t>(wire,wire+n)});}
void tick(uint32_t at){mockNow=at;loop();}
void heartbeat(uint32_t at){static uint32_t seq=0;auto p=packet(REMOTE_PING,77,++seq);p.deviceBoot=selfBoot;p.commandId=900+seq;p.padding[0]='R';p.padding[1]=1;p.health=EXCELLENT;inject(p);tick(at);}
void auth(){char mac[18];remoteMAC(mac);web.args={{"token",otaToken},{"target",mac},{"role","remote"}};web.headers.clear();web.data.name=String("firmware");}
void finish(){web.request("/update",HTTP_POST);}
std::vector<uint8_t> image(bool marker=true){std::vector<uint8_t> data(512,0x11);data[0]=0xe9;data[1]=1;if(marker)memcpy(data.data()+240,RemoteOta::ROLE_MARKER,sizeof(RemoteOta::ROLE_MARKER)-1);return data;}
void feed(const std::vector<uint8_t> &v){web.uploadEvent(UPLOAD_FILE_WRITE,v.data(),251);web.uploadEvent(UPLOAD_FILE_WRITE,v.data()+251,v.size()-251);}
int main(){
 mockPrefsBytes["buttonTest"]={0}; // Already commissioned: normal navigation fixture.
 mockLevels[13]=mockLevels[14]=HIGH;mockNow=100;setup();WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,50,88);tick(1000);inject(packet(DISCOVER,77));tick(1001);tick(1500);heartbeat(1501);assert(httpReady&&canOperate(mockNow));
 auth();web.request("/api/status",HTTP_GET);assert(web.code==200&&web.response.find("\"otaSupported\":true")!=std::string::npos&&web.response.find(RemoteOta::ROLE_MARKER)!=std::string::npos);
 web.args["token"]="wrong";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&Update.begins==0&&!otaLocked);
 auth();web.args["target"]="A0:85:E3:E2:BC:D4";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&Update.begins==0);
 auth();web.args["role"]="master";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&Update.begins==0);
 auth();web.headers["Origin"]="http://evil.invalid";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&Update.begins==0);
 auth();web.data.name=String("wrong");web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==400&&Update.begins==0);
 // Real code path for the safe TX pulse: deliberately invalid R/1 event, never navigation.
 auth();web.request("/api/remote/tx-test",HTTP_POST);assert(web.code==202&&diagSent==1&&diagPending&&whitePulses==1&&sentCommands==0);
 Packet diag;memcpy(&diag,mockUDPOut.back().data.data()+sizeof(LanHeader),sizeof(diag));assert(diag.type==REMOTE_EVENT&&diag.commandId==0&&diag.op==Remote::NONE&&diag.seq==0&&diag.uptime==0);
 tick(1502);assert(mockRgbColor==0x141414);auto ack=packet(REMOTE_ACK,77);ack.deviceBoot=selfBoot;ack.commandId=0;ack.result=STALE;ack.padding[0]='R';ack.padding[1]=1;inject(ack);tick(1503);assert(diagAcks==1&&diagResult==STALE&&!diagPending&&ackedCommands==0&&outbox.size()==0);
 tick(1582);assert(mockRgbColor==0x14); // Exactly one 80ms pulse, ACK does not repeat it.
 auto good=image(),wrong=image(false);
 auth();web.uploadEvent(UPLOAD_FILE_START);assert(otaLocked&&otaWriting);feed(wrong);auto ends=Update.ends;web.uploadEvent(UPLOAD_FILE_END);finish();assert(web.code==400&&Update.ends==ends&&!otaLocked&&!otaWriting);
 // A queued/held button is cancelled during OTA, including recovery from failed upload.
 auth();web.uploadEvent(UPLOAD_FILE_START);mockLevels[13]=LOW;tick(1600);tick(1640);assert(outbox.size()==0);web.uploadEvent(UPLOAD_FILE_ABORTED);finish();mockLevels[13]=HIGH;tick(1650);tick(1690);assert(outbox.size()==0&&sentCommands==0);
 auth();Update.writeOK=false;web.uploadEvent(UPLOAD_FILE_START);feed(good);finish();assert(web.code==400&&!otaLocked);Update.writeOK=true;
 auth();Update.endOK=false;web.uploadEvent(UPLOAD_FILE_START);feed(good);web.uploadEvent(UPLOAD_FILE_END);finish();assert(web.code==400&&!otaLocked&&!restartAt);Update.endOK=true;
 auth();web.uploadEvent(UPLOAD_FILE_START);feed(good);web.uploadEvent(UPLOAD_FILE_END);ends=Update.ends;assert(otaWriting&&otaImageComplete&&Update.ends==ends);web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==400&&Update.ends==ends&&!restartAt); // No commit before all multipart files parsed.
 auth();web.uploadEvent(UPLOAD_FILE_START);mockNow=lastUploadAt+15000;remoteWebTick();finish();assert(!otaLocked&&!otaWriting&&web.code==400);
 tick(mockNow);inject(packet(DISCOVER,77));tick(mockNow+1);tick(mockNow+400);heartbeat(mockNow+1);assert(canOperate(mockNow));
 auth();web.uploadEvent(UPLOAD_FILE_START);feed(good);web.uploadEvent(UPLOAD_FILE_END);ends=Update.ends;assert(Update.ends==ends&&otaWriting);finish();assert(web.code==200&&Update.ends==ends+1&&otaLocked&&otaOK&&restartAt);
 assert(Update.image==good);tick(restartAt+1);assert(mockRestartCalls==1&&outbox.size()==0&&sentCommands==0);
 // Marker matching must work at every boundary; malformed/oversize images are rejected.
 for(size_t split=1;split<sizeof(RemoteOta::ROLE_MARKER);split++){
  RemoteOta::ImageGuard guard;size_t at=240+split;assert(guard.feed(good.data(),at)&&guard.feed(good.data()+at,good.size()-at)&&guard.acceptable());
 }
 RemoteOta::ImageGuard guard;auto broken=good;broken[0]=0;assert(!guard.feed(broken.data(),broken.size()));guard.reset();assert(!guard.feed(good.data(),RemoteOta::MAX_IMAGE+1));
 for(auto hz:mockPWMFrequency)assert(hz==0);assert(mockUARTOut.empty());
 puts("Actual remote OTA handlers: token/MAC/role/Origin, wrong image/role, multipart commit ordering, abort/write/end/timeout recovery, OTA input cancellation, reboot, safe zero-ID TX with single 80ms white pulse and no navigation passed (HTTP/Update boundaries mocked)");
}

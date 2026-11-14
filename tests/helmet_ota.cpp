#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include "../helmet/helmet.ino"
const uint8_t MASTER[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};
template<class T>void inject(const T &p){uint8_t wire[sizeof(LanHeader)+LAN_MAX_PAYLOAD];auto n=encodeLAN(MASTER,network.mac,(const uint8_t*)&p,sizeof(p),wire);assert(n);mockUDPIn.push_back({IPAddress(192,168,50,119),LAN_PORT,std::vector<uint8_t>(wire,wire+n)});}
void tick(uint32_t at){mockNow=at;loop();}
Show::Packet scene(uint32_t seq){Show::Packet p{};p.magic=Show::MAGIC;p.protocol=Show::PROTOCOL;p.size=sizeof(p);p.type=Show::PREVIEW;p.session=77;p.deviceBoot=selfBoot;p.run=200;p.transition=seq;p.reserved=1;p.step=0;p.cue.audio.enabled=1;p.cue.voice=runner.defaults.settings().voice;p.cue.strip=runner.defaults.settings().strip;p.cue.stripOptions=runner.defaults.settings().options;return p;}
void auth(){char mac[18];helmetMAC(mac);web.args={{"token",otaToken},{"target",mac},{"role","helmet"}};web.headers.clear();web.data.name=String("firmware");}
void finish(){web.request("/update",HTTP_POST);}
std::vector<uint8_t> image(bool marker=true){std::vector<uint8_t> data(512,0x11);data[0]=0xe9;data[1]=1;data[12]=9;data[13]=0;if(marker)memcpy(data.data()+240,HelmetOta::ROLE_MARKER,sizeof(HelmetOta::ROLE_MARKER)-1);return data;}
void feed(const std::vector<uint8_t>&v){web.uploadEvent(UPLOAD_FILE_WRITE,v.data(),251);web.uploadEvent(UPLOAD_FILE_WRITE,v.data()+251,v.size()-251);}
int main(){
 RVPresetBank legacy{};legacy.slots[0].used=1;strcpy(legacy.slots[0].name,"working chip");legacy.slots[0].sound={3u|RV_LOW_CUT_FLAG|(39u<<RV_BREATH_SHIFT),1,4,0,0,.8f,1,154,.42f,11.64f,-42,.98f,16300,-6,1,0,1,6};rv_bank_seal(&legacy);assert(rv_bank_valid(&legacy));Preferences old;old.begin("robot-voice",false);old.putBytes("bank_v1",&legacy,sizeof(legacy));old.putBytes("bank_backup",&legacy,sizeof(legacy));
 mockNow=100;setup();assert(runner.active&&runner.standalone&&runner.defaults.saved);WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,50,21);tick(1000);inject(packet(DISCOVER,77));tick(1001);tick(1500);auto p=scene(1);inject(p);tick(1501);assert(runner.active&&runner.voice.requested());auto bank=mockPrefsBytes;auto defaults=mockPrefsBytes["defaults"];
 auth();web.request("/api/status",HTTP_GET);assert(web.code==200&&web.response.find("\"role\":\"helmet\"")!=std::string::npos&&web.response.find("\"otaSupported\":true")!=std::string::npos&&web.response.find(HelmetOta::ROLE_MARKER)!=std::string::npos);web.request("/",HTTP_GET);assert(web.code==200&&web.response.find("role=helmet")!=std::string::npos);
 auth();web.args["token"]="wrong";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&!otaLocked&&runner.active&&Update.begins==0);
 auth();web.args["target"]="A0:85:E3:E2:BC:D4";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&Update.begins==0);
 auth();web.args["role"]="slave";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&Update.begins==0);
 auth();web.headers["Origin"]="http://evil.invalid";web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==403&&Update.begins==0);
 auth();web.data.name=String("wrong");web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==400&&runner.active&&Update.begins==0);
 mockNextPartitionPresent=false;auth();web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==400&&runner.active&&Update.begins==0);mockNextPartitionPresent=true;
 auto good=image(),wrong=image(false);auth();web.uploadEvent(UPLOAD_FILE_START);assert(otaLocked&&otaWriting&&!runner.active&&!runner.voice.requested());
 auto next=scene(2);inject(next);tick(1502);assert(!runner.active&&!runner.voice.requested()); // New RUN/PREVIEW cannot defeat maintenance.
 auto store=next;store.type=Show::STORE;store.version=88;store.total=1;store.checksum=Show::crc32(&store.cue,sizeof(store.cue));inject(store);tick(1503);assert(scoreStore.version==0); // No filesystem writes during image upload.
 feed(wrong);auto ends=Update.ends;web.uploadEvent(UPLOAD_FILE_END);finish();assert(web.code==400&&Update.ends==ends&&!otaLocked&&!runner.active);
 inject(p);tick(1504);assert(!runner.active&&!runner.voice.requested());inject(next);tick(1505);assert(runner.active); // Old accepted cue cannot resurrect STOP, a fresh command may.
 auth();Update.beginOK=false;web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==400&&!otaLocked&&!runner.active);Update.beginOK=true;
 auth();Update.writeOK=false;web.uploadEvent(UPLOAD_FILE_START);feed(good);finish();assert(web.code==400&&!otaLocked);Update.writeOK=true;
 auth();Update.endOK=false;web.uploadEvent(UPLOAD_FILE_START);feed(good);web.uploadEvent(UPLOAD_FILE_END);finish();assert(web.code==400&&!otaLocked&&!restartAt);Update.endOK=true;
 auth();web.uploadEvent(UPLOAD_FILE_START);feed(good);web.uploadEvent(UPLOAD_FILE_END);ends=Update.ends;assert(otaWriting&&otaImageComplete&&ends==Update.ends);web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==400&&Update.ends==ends&&!restartAt);
 auth();web.uploadEvent(UPLOAD_FILE_START);web.uploadEvent(UPLOAD_FILE_ABORTED);finish();assert(web.code==400&&!otaWriting&&!otaLocked&&!runner.active);
 auth();mockNextPartition.size=300;web.uploadEvent(UPLOAD_FILE_START);feed(good);finish();assert(web.code==400&&!otaWriting&&!restartAt);mockNextPartition.size=0x140000;
 auth();web.uploadEvent(UPLOAD_FILE_START);mockNow=lastUploadAt+15000;helmetWebTick();finish();assert(!otaWriting&&!otaLocked&&web.code==400);
 assert(mockPrefsBytes["defaults"]==defaults&&mockPrefsBytes["bank_v1"]==bank["bank_v1"]&&mockPrefsBytes["bank_backup"]==bank["bank_backup"]);
 auto changed=scene(3).cue;changed.voice.frequency=155;runner.defaults.remember(changed,mockNow);mockPrefsWriteOK=false;auth();web.uploadEvent(UPLOAD_FILE_START);finish();assert(web.code==400&&!otaLocked&&!runner.active);mockPrefsWriteOK=true; // Keep the unsaved155Hz working change; the next accepted OTA must flush it.
 auth();web.uploadEvent(UPLOAD_FILE_START);feed(good);web.uploadEvent(UPLOAD_FILE_END);ends=Update.ends;assert(Update.ends==ends&&otaWriting);finish();assert(web.code==200&&Update.ends==ends+1&&otaLocked&&otaOK&&restartAt&&Update.image==good);assert(!runner.active&&!runner.voice.requested());assert(runner.defaults.saved&&runner.defaults.settings().voice.frequency==155);tick(restartAt+1);assert(mockRestartCalls==1);
 // Role markers can straddle any network chunk; S3 header and actual OTA-slot cap are checked.
 for(size_t split=1;split<sizeof(HelmetOta::ROLE_MARKER);split++){HelmetOta::ImageGuard g;size_t at=240+split;assert(g.feed(good.data(),at)&&g.feed(good.data()+at,good.size()-at)&&g.acceptable());}
 HelmetOta::ImageGuard g;auto bad=good;bad[12]=0;assert(!g.feed(bad.data(),bad.size()));g.reset();bad=good;bad[0]=0;assert(!g.feed(bad.data(),bad.size()));g.reset(256);assert(!g.feed(good.data(),good.size()));
 for(auto hz:mockPWMFrequency)assert(hz==0);assert(mockUARTOut.empty());assert(mockPrefsBytes["bank_v1"]==bank["bank_v1"]&&mockPrefsBytes["bank_backup"]==bank["bank_backup"]);Helmet::Defaults reboot;reboot.begin();assert(reboot.settings().voice.frequency==155&&reboot.saved);
 puts("Helmet OTA: auth/MAC/role/origin, S3/role marker and slot bounds, delayed multipart commit, failed-write/end/timeout/abort recovery, saved startup settings, STOP hold and rejection of scenes/STORE while updating, no PWM/UART (hardware mocked)");
}

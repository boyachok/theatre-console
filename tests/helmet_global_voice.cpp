#define THEATRE_HOST_TEST
#include <cassert>
#include <cstdio>
#include "../helmet/helmet.ino"
const uint8_t MASTER[6]={0xa0,0x85,0xe3,0xe2,0xbc,0xd4};
template<class T>void inject(const T &p){uint8_t wire[sizeof(LanHeader)+LAN_MAX_PAYLOAD];auto n=encodeLAN(MASTER,network.mac,(const uint8_t*)&p,sizeof(p),wire);assert(n);mockUDPIn.push_back({IPAddress(192,168,50,119),LAN_PORT,std::vector<uint8_t>(wire,wire+n)});}
void tick(uint32_t at){mockNow=at;loop();}
Packet ack(){for(auto i=mockUDPOut.rbegin();i!=mockUDPOut.rend();++i)if(i->data.size()==sizeof(Packet)+sizeof(LanHeader)){Packet p;memcpy(&p,i->data.data()+sizeof(LanHeader),sizeof(p));if(p.type==VOICE_CONFIG_ACK)return p;}assert(false);return {};}
int main(){
 setup();WiFi.state=WL_CONNECTED;WiFi.address=IPAddress(192,168,50,21);tick(1000);inject(packet(DISCOVER,77));tick(1001);tick(1400);
 auto probe=packet(PROBE,77,1);inject(probe);tick(1410);
 HelmetSound::Packet p;p.session=77;p.deviceBoot=selfBoot;p.revision=1;p.profile.voice=runner.defaults.settings().voice;p.profile.voice.frequency=249;p.profile.tone.eqEnabled=1;p.profile.tone.bands[1].gainDb=6;p.checksum=HelmetSound::hash(p.profile);assert(HelmetSound::valid(p));
 bool enabled=runner.voice.requested();uint32_t seq=runner.sequence;int stripWrites=mockRmtWrites;inject(p);tick(1420);assert(ack().result==APPLIED&&runner.voice.appliedProfile().frequency==249&&runner.voice.appliedTone().bands[1].gainDb==6&&runner.voice.requested()==enabled&&runner.sequence==seq&&mockRmtWrites==stripWrites);
 auto stop=packet(COMMAND,77);stop.deviceBoot=selfBoot;stop.commandId=1;stop.op=ALL_OFF;inject(stop);tick(1430);assert(!runner.active&&!runner.voice.requested());
 p.revision=2;p.profile.voice.gain=2;p.checksum=HelmetSound::hash(p.profile);inject(p);tick(1440);assert(ack().result==APPLIED&&!runner.active&&!runner.voice.requested());
 Show::Packet scene{};scene.magic=Show::MAGIC;scene.protocol=Show::PROTOCOL;scene.size=sizeof(scene);scene.type=Show::PREVIEW;scene.session=77;scene.deviceBoot=selfBoot;scene.run=9;scene.transition=1;scene.reserved=Show::ACTIVE;scene.cue.voice=runner.defaults.settings().voice;scene.cue.voice.frequency=85;scene.cue.audio.enabled=1;inject(scene);tick(1450);assert(runner.active&&runner.voice.requested()&&runner.voice.appliedProfile().frequency==249&&runner.voice.appliedTone().eqEnabled);
 p.revision=1;p.profile.voice.frequency=200;p.checksum=HelmetSound::hash(p.profile);inject(p);tick(1460);assert(ack().result==STALE&&runner.voice.appliedProfile().frequency==249);
 p.revision=3;p.profile.voice.frequency=300;p.checksum=HelmetSound::hash(p.profile);p.deviceBoot++;inject(p);tick(1470);assert(runner.voice.appliedProfile().frequency==249);p.deviceBoot=selfBoot;p.checksum++;inject(p);tick(1480);assert(runner.voice.appliedProfile().frequency==249);
 tick(3600);assert(runner.global.saved&&mockPrefsBytes["sound-v1"].size()>0);Helmet::GlobalVoice reloaded;reloaded.begin();assert(reloaded.active&&reloaded.profile.voice.frequency==249&&reloaded.profile.tone.bands[1].gainDb==6);
 assert(sizeof(Show::Packet)==512&&sizeof(Helmet::BootRecord)==111);for(auto v:mockPWMFrequency)assert(v==0);assert(mockUARTOut.empty());
 puts("Global helmet voice:145B lossless independent profile+EQ, boot/session/revision/CRC checks, immediate config without scene/strip/enable changes, old cue cannot overwrite selected preset, STOP remains authoritative, separate NVS persisted; old512B/111B formats unchanged");
}

#include <cassert>
#include <cstdio>
#include "../console/protocol.h"
#include "../console/lan_wire.h"
int main(){
 auto discovery=packet(DISCOVER,77);assert(!firmwareRequested(discovery));requestFirmware(discovery);assert(firmwareRequested(discovery));auto v=firmwareReport(77,901,FW_SLAVE);assert(sizeof(v)==64&&validFirmware(v)&&!strcmp(v.version,THEATRE_VERSION)&&v.session==77&&v.deviceBoot==901);
 uint8_t mac[]={0xa0,1,2,3,4,5},dest[]={0xb0,1,2,3,4,5},wire[532];LanHeader h;assert(encodeLAN(mac,dest,(uint8_t*)&v,sizeof(v),wire)==84&&decodeLAN(wire,84,h));
 auto bad=v;bad.version[0]='<';assert(!validFirmware(bad));bad=v;memset(bad.version,'1',sizeof(bad.version));assert(!validFirmware(bad));bad=v;bad.role=0;assert(!validFirmware(bad));bad=v;bad.deviceBoot=0;assert(!validFirmware(bad));bad=v;bad.size--;assert(!validFirmware(bad));assert(!remoteType(FIRMWARE_REPORT));
 auto old=packet(HELLO,77);old.padding[0]='P';old.padding[2]=12;assert(!localScoreTag(old));setPWMFrequency(old,25000);assert(localScoreTag(old)&&reportedPWMFrequency(old)==25000);old.padding[2]=15;assert(helmetTag(old)&&!localScoreTag(old));old.padding[2]=17;assert(helmetTag(old)&&localScoreTag(old));
 puts("Firmware metadata: opt-in discovery, bounded real-version report/role/boot/ASCII validation, unchanged 64B service envelope/R1 exclusion and score12 capability gate passed");
}

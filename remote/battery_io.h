#pragma once
#include "battery.h"
RemoteBattery::Monitor battery;
RemoteBattery::LinkDiagnostic linkDiagnostic;
Preferences batteryPrefs;uint32_t batterySampleAt=0;bool batterySampling=false;
void batteryBegin(){
 pinMode(RemoteBattery::PIN,INPUT);analogReadResolution(12);analogSetPinAttenuation(RemoteBattery::PIN,ADC_11db);
 batteryPrefs.begin("remote-batt",false);RemoteBattery::Record saved;
 if(batteryPrefs.getBytesLength("cal-v1")==sizeof(saved)&&batteryPrefs.getBytes("cal-v1",&saved,sizeof(saved))==sizeof(saved))battery.load(saved);
}
void batteryTick(uint32_t now){
 if(batterySampling&&now-batterySampleAt<25)return;
 batterySampling=true;batterySampleAt=now;battery.sample(analogReadMilliVolts(RemoteBattery::PIN),now);
}
const char *batteryCalibrate(bool full,uint32_t now){
 RemoteBattery::Record next;const char *error=battery.prepare(full,now,next);if(error)return error;
 if(memcmp(&next,&battery.calibration,sizeof(next))&&batteryPrefs.putBytes("cal-v1",&next,sizeof(next))!=sizeof(next))return "Cannot persist battery calibration";
 battery.load(next);return nullptr;
}
String batteryJSON(){
 uint32_t now=millis();char body[720],reserve[16];int pct=battery.reserve(now);
 if(pct<0)strcpy(reserve,"null");else snprintf(reserve,sizeof(reserve),"%d",pct);
 snprintf(body,sizeof(body),"{\"pin\":4,\"divider\":2,\"rawBatteryMv\":%u,\"batteryMv\":%u,\"fresh\":%s,\"stable\":%s,\"spreadMv\":%u,\"windowCount\":%u,\"sampleAgeMs\":%lu,\"enabled\":%s,\"fullCaptured\":%s,\"lowCaptured\":%s,\"fullMv\":%u,\"lowMv\":%u,\"reservePct\":%s,\"reserveIsNotStateOfCharge\":true,\"warning\":%s,\"critical\":%s}",
 battery.rawMv(),battery.mv(),battery.fresh(now)?"true":"false",battery.stable(now)?"true":"false",battery.spread(),battery.windows(),(unsigned long)battery.age(now),battery.enabled()?"true":"false",battery.enabled()?"true":"false",battery.calibrated()?"true":"false",battery.calibration.fullMv,battery.calibration.lowMv,reserve,pct>=0&&pct<=20?"true":"false",pct>=0&&battery.mv()<=battery.calibration.lowMv?"true":"false");return String(body);
}
uint32_t diagnosticLinkColor(Health health,uint32_t now){
 if((now/300)%2)return 0;
 if(health==OFFLINE)return 0x180000;if(health==WARMUP)return 0x040404;
 return networkLedColor(health,now);
}

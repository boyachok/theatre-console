#include <Arduino.h>
#include <WiFi.h>
#include "wifi_transport.h"
#include "protocol.h"
#include "network_led.h"
#include "slave_score_store.h"
#include "show_filesystem.h"
#include "runtime.h"
#include "control_web.h"
#include "ota_guard.h"
// Dedicated helmet: local I2S DSP + RMT strip; HTTP is maintenance/OTA only.
ControlWebServer web(80);
bool httpReady=false,otaLocked=false,otaWriting=false,otaOK=false,uploadSeen=false,otaImageComplete=false;
uint32_t firmwareBoot=1,restartAt=0,lastUploadAt=0;char otaToken[33]{};const char *otaError="none";
HelmetOta::ImageGuard otaImage;
void helmetWebBegin();void helmetWebTick();String helmetOtaStatusJSON();
uint32_t voiceReportAt=0;
// Published RUN is a 44B local-plan reference; draft PREVIEW remains explicit.
WifiTransport network;Helmet::Runner runner;Show::SlaveStore scoreStore;
uint8_t masterMAC[6]{};bool masterKnown=false,hasHeartbeat=false,recovery=false,storageReady=false,masterWantsFirmware=false;
uint32_t selfBoot=1,masterSession=0,generation=0,masterSeen=0,lastHeartbeat=0,lastProbeSeq=0,helloAt=0,identifyUntil=0;
uint8_t assignedId=0;Health grade=WARMUP;
uint32_t lastControlId=0;Result lastControlResult=APPLIED;
uint32_t randomId(){uint32_t n=esp_random();return n?n:1;}
bool linkFresh(){return network.ready()&&hasHeartbeat&&millis()-lastHeartbeat<OFFLINE_MS;}
Packet message(Type type,uint32_t seq=0){
 Packet p=packet(type,masterSession,seq);p.deviceBoot=selfBoot;p.nodeId=assignedId;p.rssi=(int8_t)WiFi.RSSI();p.state=runner.snapshot(linkFresh());
 // P/17: score12 indexed helmet, local voice and strip, no PWM. Same 72-byte HELLO/ECHO envelope.
 if(type==HELLO||type==ECHO){p.padding[0]='P';p.padding[2]=17;}
 return p;
}
void sendFirmwareReport(){if(masterKnown&&masterWantsFirmware){auto p=firmwareReport(masterSession,selfBoot,FW_HELMET);network.send(masterMAC,(const uint8_t*)&p,sizeof(p));}}
void sendVoiceReport(){if(!masterKnown)return;auto s=runner.voice.diagnostics();VoiceReport p{};p.magic=MAGIC;p.protocol=PROTOCOL;p.type=VOICE_REPORT;p.size=sizeof(p);p.session=masterSession;p.deviceBoot=selfBoot;p.uptime=millis();p.captured=s.captured;p.played=s.played;p.errors=s.errors;p.late=s.late;p.limited=s.limited;p.workMax=s.workMax;p.inputLevel=s.inputLevel;p.outputLevel=s.outputLevel;p.load=s.load;p.ready=s.ready;p.enabled=s.enabled;p.audioCore=s.audioCore;
#ifdef THEATRE_HOST_TEST
p.controlCore=0;
#else
p.controlCore=xPortGetCoreID();
#endif
network.send(masterMAC,(uint8_t*)&p,sizeof(p));}
void resetLink(bool forget){
 selfBoot=randomId();hasHeartbeat=false;lastProbeSeq=0;lastControlId=0;lastControlResult=APPLIED;if(runner.active)recovery=true;
 if(forget){masterKnown=false;masterSession=0;helloAt=0;assignedId=0;}
 else if(masterKnown)helloAt=millis()+20+esp_random()%100;
}
bool fromMaster(const LanMessage &wire,uint32_t session){return masterKnown&&!memcmp(masterMAC,wire.mac,6)&&session==masterSession;}
void receiveHelmet(){
 LanMessage wire;int budget=32;
 while(budget--&&network.receive(wire)){
  uint32_t now=millis();
  if(wire.length==sizeof(Show::Packet)||wire.length==sizeof(Show::RunPacket)){
   Show::Packet p{};memcpy(&p,wire.payload,wire.length);bool indexed=wire.length==sizeof(Show::RunPacket);
   if(p.magic!=Show::MAGIC||p.protocol!=Show::PROTOCOL||p.size!=wire.length||!fromMaster(wire,p.session)||p.deviceBoot!=selfBoot){network.rxInvalid++;continue;}
   if((indexed&&p.type!=Show::RUN)||(!indexed&&p.type!=Show::STORE&&p.type!=Show::PREVIEW)){network.rxInvalid++;continue;}
   masterSeen=lastHeartbeat=now;hasHeartbeat=true;grade=p.health<=OFFLINE?(Health)p.health:BAD;
   bool accepted=false,missing=false;
   if(p.type==Show::STORE)accepted=!otaLocked&&storageReady&&Helmet::supported(p.cue)&&scoreStore.accept(p);
   else if(!otaLocked&&p.reserved==Show::ACTIVE&&p.run&&p.transition){
    if(indexed){auto lookup=scoreStore.resolve(Show::reference(p),p.cue);missing=lookup==Show::SlaveStore::Lookup::MISSING;
     if(lookup==Show::SlaveStore::Lookup::READY)accepted=runner.apply(p,now,recovery&&runner.active);
    }else accepted=runner.apply(p,now,recovery&&runner.active);
    accepted=accepted&&runner.active&&runner.run==p.run&&runner.sequence==p.transition&&runner.step==p.step;
    if(accepted)recovery=false;
   }else accepted=p.reserved==0;
   Packet ack=message(SCORE_ACK);ack.channel=p.type==Show::STORE?0:1;ack.result=accepted?APPLIED:NOT_READY;
   ack.op=p.sentAt&255;ack.padding[0]=(p.sentAt>>8)&255;ack.padding[1]=(p.sentAt>>16)&255;ack.padding[2]=(p.sentAt>>24)&255;
   if(p.type==Show::STORE){ack.commandId=scoreStore.version;ack.seq=p.checksum;ack.value=scoreStore.nextMissing();if(scoreStore.version==p.version&&scoreStore.checksum==p.checksum)ack.reserved|=Show::LOADED;}
   else {ack.commandId=p.run;ack.seq=p.transition;ack.value=runner.step;if(accepted&&p.reserved==Show::ACTIVE&&runner.active)ack.reserved|=Show::ACTIVE;if(missing)ack.reserved|=Show::MISSING_PLAN;}
   network.send(masterMAC,(const uint8_t*)&ack,sizeof(ack));continue;
  }
  if(wire.length!=sizeof(Packet))continue;
  Packet p;memcpy(&p,wire.payload,sizeof(p));
  if(p.magic!=MAGIC||p.protocol!=PROTOCOL||p.size!=sizeof(p))continue;
  if(p.type==DISCOVER&&p.session){
   if(masterKnown&&memcmp(masterMAC,wire.mac,6)&&now-masterSeen<15000)continue;
   if(!masterKnown||memcmp(masterMAC,wire.mac,6)||masterSession!=p.session){resetLink(true);memcpy(masterMAC,wire.mac,6);masterKnown=true;masterSession=p.session;grade=WARMUP;}
   masterSeen=now;masterWantsFirmware=firmwareRequested(p);helloAt=now+20+esp_random()%350;continue;
  }
  if(!fromMaster(wire,p.session))continue;
  if(p.type==PROBE){
   if(lastProbeSeq&&(int32_t)(p.seq-lastProbeSeq)<=0)continue;
   lastProbeSeq=p.seq;masterSeen=lastHeartbeat=now;hasHeartbeat=true;assignedId=p.nodeId;grade=p.health<=OFFLINE?(Health)p.health:BAD;
   Packet reply=message(ECHO,p.seq);network.send(masterMAC,(const uint8_t*)&reply,sizeof(reply));sendFirmwareReport();
  }else if(p.type==MAINTENANCE)runner.stop();
  else if(p.type==IDENTIFY)identifyUntil=now+5000;
  else if(p.type==COMMAND&&p.deviceBoot==selfBoot&&p.commandId){
   Packet ack=message(COMMAND_ACK);ack.commandId=p.commandId;ack.result=STALE;
   if(p.commandId==lastControlId)ack.result=lastControlResult;
   else if(newSequence(p.commandId,lastControlId)){
    Result result=INVALID;
    if(p.op==ALL_OFF&&p.channel==0&&p.value==0){runner.stop();result=APPLIED;}
    lastControlId=p.commandId;lastControlResult=result;ack.result=result;
   }
   ack.state=runner.snapshot(linkFresh());
   network.send(masterMAC,(const uint8_t*)&ack,sizeof(ack));
  }
 }
}
void legacyStatus(){
 Serial.printf("{\"role\":\"helmet\",\"version\":\"%s\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"ip\":\"%s\",\"connected\":%s,\"masterReady\":%s,\"stripPin\":%u,\"stripReady\":%s,\"storageReady\":%s,\"scoreVersion\":%lu,\"step\":%u,\"active\":%s,\"voiceRequested\":%s,\"voiceAvailable\":%s,\"voiceApplied\":%s}\n",RADIO_VERSION,network.mac[0],network.mac[1],network.mac[2],network.mac[3],network.mac[4],network.mac[5],WiFi.localIP().toString().c_str(),network.ready()?"true":"false",linkFresh()?"true":"false",HELMET_STRIP_PIN,runner.ready?"true":"false",storageReady?"true":"false",(unsigned long)scoreStore.version,runner.step+1,runner.active?"true":"false",runner.voice.requested()?"true":"false",runner.voice.available()?"true":"false",runner.voice.available()?(runner.voice.diagnostics().enabled?"true":"false"):"null");
}
String helmetStatusJSON(){
 auto a=runner.voice.diagnostics();auto live=runner.voice.appliedProfile();const auto &b=runner.defaults.settings();char mac[18];snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",network.mac[0],network.mac[1],network.mac[2],network.mac[3],network.mac[4],network.mac[5]);String j;
 j="{\"role\":\"helmet\",\"version\":\""+String(RADIO_VERSION)+"\",\"mac\":\""+String(mac)+"\",\"boot\":"+String(selfBoot)+",\"ip\":\""+WiFi.localIP().toString()+"\",\"connected\":"+String(network.ready()?"true":"false")+",\"masterReady\":"+String(linkFresh()?"true":"false")+",\"standalone\":"+String(runner.standalone?"true":"false")+",\"active\":"+String(runner.active?"true":"false")+",\"step\":"+(runner.active&&!runner.standalone?String(runner.step+1):String("null"));
 j+=",\"voiceRequested\":"+String(runner.voice.requested()?"true":"false")+",\"voiceAvailable\":"+String(a.ready?"true":"false")+",\"voiceApplied\":"+String(a.enabled?"true":"false")+",\"storageReady\":"+String(storageReady?"true":"false")+",\"scoreVersion\":"+String(scoreStore.version)+",\"defaultsSaved\":"+String(runner.defaults.saved?"true":"false")+",\"defaultsSource\":\""+String(runner.defaults.source)+"\",\"defaultsWrites\":"+String(runner.defaults.writes);
 j+=",\"sound\":{\"algorithm\":"+String(live.algorithm)+",\"mode\":"+String(live.mode)+",\"frequency\":"+String(live.frequency,2)+",\"gain\":"+String(live.gain,2)+",\"volume\":"+String(live.volume,3)+",\"gateDb\":"+String(live.gateDb,1)+"},\"diagnostic\":"+String(runner.diagnostic?"true":"false")+",\"savedSound\":{\"frequency\":"+String(b.voice.frequency,2)+",\"gain\":"+String(b.voice.gain,2)+",\"volume\":"+String(b.voice.volume,3)+",\"gateDb\":"+String(b.voice.gateDb,1)+"},\"strip\":{\"mode\":"+String(b.strip.mode)+",\"count\":"+String(b.strip.count)+",\"brightness\":"+String(b.strip.brightness)+",\"pin\":47},\"audio\":{\"captured\":"+String(a.captured)+",\"played\":"+String(a.played)+",\"errors\":"+String(a.errors)+",\"late\":"+String(a.late)+",\"load\":"+String(a.load,2)+",\"inputLevel\":"+String(a.inputLevel,6)+",\"outputLevel\":"+String(a.outputLevel,6)+",\"core\":"+String(a.audioCore)+",\"gateDb\":"+String(a.gateDb,1)+",\"gateEnvelopeDb\":"+String(a.gateEnvelopeDb,2)+",\"gateGain\":"+String(a.gateGain,5)+"},"+helmetOtaStatusJSON()+"}";return j;
}
void status(){Serial.println(helmetStatusJSON());}
void serviceHelmet();
#ifndef THEATRE_HOST_TEST
void controlTask(void*){for(;;)serviceHelmet();}
#endif
void setup(){
 Serial.begin(115200);selfBoot=firmwareBoot=randomId();snprintf(otaToken,sizeof(otaToken),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());const char *fsLabel=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"storage")?"storage":"spiffs";storageReady=mountShowFS(fsLabel);if(storageReady)scoreStore.begin();runner.begin();
 network.begin(false,"stage-helmet-",true);generation=network.generation;helmetWebBegin();
 Serial.printf("\nTheatre console %s role=helmet DATA=GPIO%u I2S4/5/6/7; i/j=status\n",RADIO_VERSION,HELMET_STRIP_PIN);
#ifndef THEATRE_HOST_TEST
 if(xTaskCreatePinnedToCore(controlTask,"helmet-control",12288,nullptr,1,nullptr,0)!=pdPASS){runner.stop();Serial.println("Helmet control task failed");}
#endif
}
void serviceHelmet(){
 network.tick();uint32_t now=millis();
 if(generation!=network.generation){generation=network.generation;resetLink(true);}
 if(hasHeartbeat&&now-lastHeartbeat>=OFFLINE_MS)resetLink(false);
 receiveHelmet();now=millis();
 if(network.ready()&&masterKnown&&helloAt&&(int32_t)(now-helloAt)>=0){helloAt=0;Packet p=message(HELLO);network.send(masterMAC,(const uint8_t*)&p,sizeof(p));sendFirmwareReport();}
 helmetWebTick();now=millis();runner.tick(now);if(linkFresh()&&now-voiceReportAt>=250){voiceReportAt=now;sendVoiceReport();}
 bool identify=identifyUntil&&(int32_t)(identifyUntil-now)>0;
 uint32_t color=networkLedColor(linkFresh()?grade:OFFLINE,now,identify,false);static uint32_t previous=0xffffffff;
 if(color!=previous){rgbLedWrite(HELMET_STATUS_PIN,(color>>16)&255,(color>>8)&255,color&255);previous=color;}
 while(Serial.available()){char c=Serial.read();if(c=='i'||c=='j')status();else if(c=='!')runner.stop();else if(c=='d'&&!otaLocked)runner.dryTest();else if(c=='b'&&!otaLocked)runner.endTest();else if(c=='L'&&!otaLocked){bool ok=runner.importLegacy();Serial.println(ok?"Legacy voice imported and saved":"Legacy import rejected");}}
 if(restartAt&&(int32_t)(millis()-restartAt)>=0)ESP.restart();
 delay(1);
}
void loop(){
#ifdef THEATRE_HOST_TEST
 serviceHelmet();
#else
 delay(1000);
#endif
}
#include "helmet_web.h"

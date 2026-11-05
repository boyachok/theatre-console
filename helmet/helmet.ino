#include <Arduino.h>
#include <WiFi.h>
#include "wifi_transport.h"
#include "protocol.h"
#include "network_led.h"
#include "slave_score_store.h"
#include "show_filesystem.h"
#include "runtime.h"
// Dedicated actuator role. No Actors/Player, PWM, relay, HTTP or OTA driver.
// Existing protocol sizes and master send/retry scheduling are unchanged.
WifiTransport network;Helmet::Runner runner;Show::SlaveStore scoreStore;
uint8_t masterMAC[6]{};bool masterKnown=false,hasHeartbeat=false,recovery=false,storageReady=false;
uint32_t selfBoot=1,masterSession=0,generation=0,masterSeen=0,lastHeartbeat=0,lastProbeSeq=0,helloAt=0,identifyUntil=0;
uint8_t assignedId=0;Health grade=WARMUP;
uint32_t lastControlId=0;Result lastControlResult=APPLIED;
uint32_t randomId(){uint32_t n=esp_random();return n?n:1;}
bool linkFresh(){return network.ready()&&hasHeartbeat&&millis()-lastHeartbeat<OFFLINE_MS;}
Packet message(Type type,uint32_t seq=0){
 Packet p=packet(type,masterSession,seq);p.deviceBoot=selfBoot;p.nodeId=assignedId;p.rssi=(int8_t)WiFi.RSSI();p.state=runner.snapshot(linkFresh());
 // P/5: score-capable helmet, no PWM. Same 72-byte HELLO/ECHO envelope.
 if(type==HELLO||type==ECHO){p.padding[0]='P';p.padding[2]=5;}
 return p;
}
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
  if(wire.length==sizeof(Show::Packet)){
   Show::Packet p;memcpy(&p,wire.payload,sizeof(p));
   if(p.magic!=Show::MAGIC||p.protocol!=Show::PROTOCOL||p.size!=sizeof(p)||!fromMaster(wire,p.session)||p.deviceBoot!=selfBoot){network.rxInvalid++;continue;}
   if(p.type!=Show::STORE&&p.type!=Show::RUN)continue;
   masterSeen=lastHeartbeat=now;hasHeartbeat=true;grade=p.health<=OFFLINE?(Health)p.health:BAD;
   bool accepted=false;
   if(p.type==Show::STORE)accepted=storageReady&&Helmet::supported(p.cue)&&scoreStore.accept(p);
   else if(p.reserved&1){accepted=runner.apply(p,now,recovery&&runner.active);if(accepted)recovery=false;}
   else accepted=true;
   Packet ack=message(SCORE_ACK);ack.channel=p.type==Show::STORE?0:1;ack.result=accepted?APPLIED:NOT_READY;
   ack.op=p.sentAt&255;ack.padding[0]=(p.sentAt>>8)&255;ack.padding[1]=(p.sentAt>>16)&255;ack.padding[2]=(p.sentAt>>24)&255;
   if(p.type==Show::STORE){ack.commandId=scoreStore.version;ack.value=scoreStore.nextMissing();if(scoreStore.version==p.version&&scoreStore.checksum==p.checksum)ack.reserved|=2;}
   else {ack.commandId=runner.run;ack.seq=runner.sequence;ack.value=runner.step;if(runner.active)ack.reserved|=1;}
   network.send(masterMAC,(const uint8_t*)&ack,sizeof(ack));continue;
  }
  if(wire.length!=sizeof(Packet))continue;
  Packet p;memcpy(&p,wire.payload,sizeof(p));
  if(p.magic!=MAGIC||p.protocol!=PROTOCOL||p.size!=sizeof(p))continue;
  if(p.type==DISCOVER&&p.session){
   if(masterKnown&&memcmp(masterMAC,wire.mac,6)&&now-masterSeen<15000)continue;
   if(!masterKnown||memcmp(masterMAC,wire.mac,6)||masterSession!=p.session){resetLink(true);memcpy(masterMAC,wire.mac,6);masterKnown=true;masterSession=p.session;grade=WARMUP;}
   masterSeen=now;helloAt=now+20+esp_random()%350;continue;
  }
  if(!fromMaster(wire,p.session))continue;
  if(p.type==PROBE){
   if(lastProbeSeq&&(int32_t)(p.seq-lastProbeSeq)<=0)continue;
   lastProbeSeq=p.seq;masterSeen=lastHeartbeat=now;hasHeartbeat=true;assignedId=p.nodeId;grade=p.health<=OFFLINE?(Health)p.health:BAD;
   Packet reply=message(ECHO,p.seq);network.send(masterMAC,(const uint8_t*)&reply,sizeof(reply));
  }else if(p.type==IDENTIFY)identifyUntil=now+5000;
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
void status(){
 Serial.printf("{\"role\":\"helmet\",\"version\":\"%s\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"ip\":\"%s\",\"connected\":%s,\"masterReady\":%s,\"stripPin\":%u,\"stripReady\":%s,\"storageReady\":%s,\"scoreVersion\":%lu,\"step\":%u,\"active\":%s,\"voiceRequested\":%s,\"voiceAvailable\":false,\"voiceApplied\":null}\n",RADIO_VERSION,network.mac[0],network.mac[1],network.mac[2],network.mac[3],network.mac[4],network.mac[5],WiFi.localIP().toString().c_str(),network.ready()?"true":"false",linkFresh()?"true":"false",HELMET_STRIP_PIN,runner.ready?"true":"false",storageReady?"true":"false",(unsigned long)scoreStore.version,runner.step+1,runner.active?"true":"false",runner.voice.requested()?"true":"false");
}
void setup(){
 Serial.begin(115200);selfBoot=randomId();storageReady=mountShowFS();if(storageReady)scoreStore.begin();runner.begin();
 network.begin(false,"stage-helmet-",false);generation=network.generation;
 Serial.printf("\nTheatre console %s role=helmet WS2811 DATA=GPIO%u; voice backend NOT installed; i/j=status\n",RADIO_VERSION,HELMET_STRIP_PIN);
}
void loop(){
 network.tick();uint32_t now=millis();
 if(generation!=network.generation){generation=network.generation;resetLink(true);}
 if(hasHeartbeat&&now-lastHeartbeat>=OFFLINE_MS)resetLink(false);
 receiveHelmet();now=millis();
 if(network.ready()&&masterKnown&&helloAt&&(int32_t)(now-helloAt)>=0){helloAt=0;Packet p=message(HELLO);network.send(masterMAC,(const uint8_t*)&p,sizeof(p));}
 runner.tick(now);
 bool identify=identifyUntil&&(int32_t)(identifyUntil-now)>0;
 uint32_t color=networkLedColor(linkFresh()?grade:OFFLINE,now,identify,false);static uint32_t previous=0xffffffff;
 if(color!=previous){rgbLedWrite(HELMET_STATUS_PIN,(color>>16)&255,(color>>8)&255,color&255);previous=color;}
 while(Serial.available()){char c=Serial.read();if(c=='i'||c=='j')status();else if(c=='!')runner.stop();}
 delay(1);
}

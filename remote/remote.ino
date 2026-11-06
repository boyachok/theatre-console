#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "button_probe.h"
#include "wifi_transport.h"
#include "protocol.h"
#include "remote_link.h"
#include "network_led.h"
#include "control_web.h"
#include "ota_guard.h"
// Dedicated remote: maintenance HTTP/OTA only. No filesystem/PWM/relays/audio.
constexpr uint8_t FORWARD_PIN=13,BACK_PIN=14,STATUS_PIN=48;
WifiTransport network;Remote::Buttons buttons;Remote::Outbox outbox;ControlWebServer web(80);
uint8_t masterMAC[6]{};bool masterKnown=false,hasPing=false;
uint32_t selfBoot=1,firmwareBoot=1,masterSession=0,generation=0,masterSeen=0,lastPing=0,lastPingSeq=0,helloAt=0;
uint32_t token=0,clockMaster=0,clockAt=0,whiteUntil=0,identifyUntil=0,longAckAt=0,longConfirmations=0;
uint32_t sentCommands=0,ackedCommands=0,lastAck=0,whitePulses=0,diagSent=0,diagAcks=0,diagAt=0;bool diagPending=false;
uint16_t currentStep=0;uint8_t lastResult=NOT_READY,diagResult=NOT_READY;Health grade=WARMUP;
bool httpReady=false,otaLocked=false,otaWriting=false,otaOK=false,uploadSeen=false,otaImageComplete=false;
uint32_t restartAt=0,lastUploadAt=0;char otaToken[33]{};const char *otaError="none";
RemoteOta::ImageGuard otaImage;
Preferences diagnosticPrefs;Remote::ButtonProbe buttonProbe;bool buttonTest=false;
bool setButtonTest(bool enabled){
 uint8_t value=enabled?1:0;
 if(diagnosticPrefs.putBytes("buttonTest",&value,1)!=1)return false;
 buttonTest=enabled;outbox.clear();buttons.cancel(digitalRead(FORWARD_PIN)==LOW,digitalRead(BACK_PIN)==LOW,millis());
 buttonProbe.reset(digitalRead(FORWARD_PIN)==LOW,digitalRead(BACK_PIN)==LOW,millis());
 identifyUntil=whiteUntil=longAckAt=0;return true;
}
String remoteStatusJSON();void remoteWebBegin();void remoteWebTick();
uint32_t randomId(){uint32_t n=esp_random();return n?n:1;}
Packet message(Type type,uint32_t seq=0){Packet p=packet(type,masterSession,seq);p.deviceBoot=selfBoot;p.padding[0]='R';p.padding[1]=1;p.rssi=(int8_t)WiFi.RSSI();return p;}
void cancelInputs(){outbox.clear();buttons.cancel(digitalRead(FORWARD_PIN)==LOW,digitalRead(BACK_PIN)==LOW,millis());}
void cancelLink(bool forget){
 selfBoot=randomId();hasPing=false;lastPingSeq=token=0;diagPending=false;longAckAt=0;cancelInputs();
 if(forget){masterKnown=false;masterSession=0;helloAt=0;}else if(masterKnown)helloAt=millis()+20+esp_random()%100;
}
bool canOperate(uint32_t now){return network.ready()&&masterKnown&&hasPing&&token&&now-clockAt<=1000;}
void whitePulse(uint32_t now){whiteUntil=now+80;whitePulses++;}
// Reserved invalid navigation event: id=0/action=NONE is ALWAYS rejected by R/1.
// Exercises real UDP TX -> master STALE ACK without moving the show counter.
bool diagnosticTX(){
 uint32_t now=millis();if(otaLocked||!canOperate(now)||outbox.size()||diagPending)return false;
 Packet p=message(REMOTE_EVENT);p.commandId=0;p.op=Remote::NONE;p.seq=0;p.uptime=0;
 if(!network.send(masterMAC,(const uint8_t*)&p,sizeof(p)))return false;
 diagSent++;diagPending=true;diagAt=now;diagResult=NOT_READY;whitePulse(now);return true;
}
void receiveRemote(){
 LanMessage wire;int budget=32;uint32_t now=millis();while(budget--&&network.receive(wire)){
  if(wire.length!=sizeof(Packet))continue;Packet p;memcpy(&p,wire.payload,sizeof(p));
  if(p.magic!=MAGIC||p.protocol!=PROTOCOL||p.size!=sizeof(p))continue;
  if(p.type==DISCOVER&&p.session){
   if(masterKnown&&memcmp(masterMAC,wire.mac,6)&&now-masterSeen<15000)continue;
   if(!masterKnown||memcmp(masterMAC,wire.mac,6)||masterSession!=p.session){cancelLink(true);memcpy(masterMAC,wire.mac,6);masterKnown=true;masterSession=p.session;grade=WARMUP;}
   masterSeen=now;helloAt=now+20+esp_random()%350;continue;
  }
  if(!masterKnown||memcmp(masterMAC,wire.mac,6)||p.session!=masterSession||p.deviceBoot!=selfBoot)continue;
  if(p.type==IDENTIFY){identifyUntil=now+5000;continue;}if(!remoteTag(p))continue;
  if(p.type==REMOTE_PING&&p.commandId&&p.seq){
   if(lastPingSeq&&(int32_t)(p.seq-lastPingSeq)<=0)continue;
   lastPingSeq=p.seq;masterSeen=lastPing=clockAt=now;hasPing=true;clockMaster=p.uptime;token=p.commandId;currentStep=p.value;grade=p.health<=OFFLINE?(Health)p.health:BAD;
   Packet pong=message(REMOTE_PONG,p.seq);pong.commandId=p.commandId;pong.value=p.value;network.send(masterMAC,(const uint8_t*)&pong,sizeof(pong));
  }else if(p.type==REMOTE_ACK){
   if(p.commandId==0&&diagPending&&now-diagAt<1500){diagPending=false;diagAcks++;diagResult=p.result;masterSeen=now;}
   else {Remote::Action action=Remote::NONE;if(outbox.acknowledge(p.commandId,&action)){
    lastAck=p.commandId;lastResult=p.result;ackedCommands++;currentStep=p.value;masterSeen=now;
    if(p.result==APPLIED&&(action==Remote::FIRST||action==Remote::LAST)){longAckAt=now+120;longConfirmations++;}
   }}
  }
 }
}
void status(){Serial.printf("%s\n",remoteStatusJSON().c_str());}
void setup(){
 Serial.begin(115200);pinMode(FORWARD_PIN,INPUT_PULLUP);pinMode(BACK_PIN,INPUT_PULLUP);selfBoot=randomId();firmwareBoot=randomId();cancelInputs();
 diagnosticPrefs.begin("remote-diag",false);uint8_t testFlag=1; // First boot/read failure stays safely in commissioning mode.
 if(diagnosticPrefs.getBytesLength("buttonTest")==1)diagnosticPrefs.getBytes("buttonTest",&testFlag,1);
 buttonTest=testFlag!=0;buttonProbe.reset(digitalRead(FORWARD_PIN)==LOW,digitalRead(BACK_PIN)==LOW,millis());
 snprintf(otaToken,sizeof(otaToken),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
 network.begin(false,"stage-remote-",true);generation=network.generation;remoteWebBegin();
 Serial.printf("\nTheatre console %s role=remote; GPIO13 forward/GPIO14 back; maintenance HTTP/OTA; i/j=status, l=white LED, t=safe TX test\n",RADIO_VERSION);
}
void loop(){
 network.tick();uint32_t now=millis();if(generation!=network.generation){generation=network.generation;cancelLink(true);}
 if(hasPing&&now-lastPing>Remote::LEASE_MS)cancelLink(false);
 receiveRemote();now=millis();
 if(network.ready()&&masterKnown&&helloAt&&(int32_t)(now-helloAt)>=0){helloAt=0;Packet hello=message(REMOTE_HELLO);network.send(masterMAC,(const uint8_t*)&hello,sizeof(hello));}
 if(diagPending&&now-diagAt>=1500)diagPending=false;
 bool live=canOperate(now),forward=digitalRead(FORWARD_PIN)==LOW,back=digitalRead(BACK_PIN)==LOW;
 uint32_t released=buttonProbe.releases[0]+buttonProbe.releases[1];buttonProbe.tick(forward,back,now);
 if(buttonTest&&released!=buttonProbe.releases[0]+buttonProbe.releases[1])diagnosticTX();
 auto action=buttons.tick(forward,back,live&&!otaLocked&&!buttonTest,now);
 if(!live||otaLocked||buttonTest)outbox.clear();
 else {
  if(action!=Remote::NONE)outbox.push(action,token,clockMaster+(uint32_t)(now-clockAt),now);
  if(auto *event=outbox.due(now)){
   Packet command=message(REMOTE_EVENT,event->token);command.commandId=event->id;command.op=event->action;command.uptime=event->deadline;
   bool sent=network.send(masterMAC,(const uint8_t*)&command,sizeof(command));outbox.attempted(*event,now);
   if(sent){sentCommands++;if(!event->sent){event->sent=true;whitePulse(now);}}
  }
 }
 remoteWebTick();receiveRemote();now=millis();live=canOperate(now);
 bool identify=identifyUntil&&(int32_t)(identifyUntil-now)>0,pulse=whiteUntil&&(int32_t)(whiteUntil-now)>0;
 bool longWhite=false;if(longAckAt&&(int32_t)(now-longAckAt)>=0){uint32_t phase=now-longAckAt;if(phase>=360)longAckAt=0;else longWhite=phase<120||phase>=240;}
 uint32_t color=buttonTest&&!identify?buttonProbe.color(pulse):!identify&&longWhite?0x606060:networkLedColor(live?grade:OFFLINE,now,identify,pulse);static uint32_t previous=0xffffffff;
 if(color!=previous){rgbLedWrite(STATUS_PIN,(color>>16)&255,(color>>8)&255,color&255);previous=color;}
 while(Serial.available()){char c=Serial.read();if(c=='i'||c=='j')status();else if(c=='l')whitePulse(millis());else if(c=='t')diagnosticTX();}
 if(restartAt&&(int32_t)(now-restartAt)>=0)ESP.restart();
 delay(1);
}
#include "remote_web.h"

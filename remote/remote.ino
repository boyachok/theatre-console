#include <Arduino.h>
#include <WiFi.h>
#include "wifi_transport.h"
#include "protocol.h"
#include "remote_link.h"
#include "network_led.h"
// Dedicated remote firmware: no web server, filesystem, PWM, relays or audio drivers.
constexpr uint8_t FORWARD_PIN=13,BACK_PIN=14,STATUS_PIN=48;
WifiTransport network;Remote::Buttons buttons;Remote::Outbox outbox;
uint8_t masterMAC[6]{};bool masterKnown=false,hasPing=false;
uint32_t selfBoot=1,masterSession=0,generation=0,masterSeen=0,lastPing=0,lastPingSeq=0,helloAt=0;
uint32_t token=0,clockMaster=0,clockAt=0,whiteUntil=0,identifyUntil=0;
uint32_t sentCommands=0,ackedCommands=0,lastAck=0;uint16_t currentStep=0;uint8_t lastResult=NOT_READY;Health grade=WARMUP;
uint32_t randomId(){uint32_t n=esp_random();return n?n:1;}
Packet message(Type type,uint32_t seq=0){Packet p=packet(type,masterSession,seq);p.deviceBoot=selfBoot;p.padding[0]='R';p.padding[1]=1;p.rssi=(int8_t)WiFi.RSSI();return p;}
void cancelLink(bool forget){
 selfBoot=randomId();hasPing=false;lastPingSeq=token=0;outbox.clear();buttons.cancel(digitalRead(FORWARD_PIN)==LOW,digitalRead(BACK_PIN)==LOW,millis());
 if(forget){masterKnown=false;masterSession=0;helloAt=0;}else if(masterKnown)helloAt=millis()+20+esp_random()%100;
}
bool canOperate(uint32_t now){return network.ready()&&masterKnown&&hasPing&&token&&now-clockAt<=1000;}
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
  }else if(p.type==REMOTE_ACK&&outbox.acknowledge(p.commandId)){lastAck=p.commandId;lastResult=p.result;ackedCommands++;currentStep=p.value;masterSeen=now;}
 }
}
void status(){
 Serial.printf("{\"role\":\"remote\",\"version\":\"%s\",\"name\":\"remote\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"ip\":\"%s\",\"hostname\":\"%s\",\"wifi\":%s,\"masterReady\":%s,\"rssi\":%d,\"pending\":%u,\"sent\":%lu,\"acks\":%lu,\"lastAck\":%lu,\"lastResult\":%u,\"step\":%u}\n",RADIO_VERSION,network.mac[0],network.mac[1],network.mac[2],network.mac[3],network.mac[4],network.mac[5],WiFi.localIP().toString().c_str(),network.hostname.c_str(),network.ready()?"true":"false",canOperate(millis())?"true":"false",WiFi.RSSI(),outbox.size(),(unsigned long)sentCommands,(unsigned long)ackedCommands,(unsigned long)lastAck,lastResult,currentStep);
}
void setup(){
 Serial.begin(115200);pinMode(FORWARD_PIN,INPUT_PULLUP);pinMode(BACK_PIN,INPUT_PULLUP);selfBoot=randomId();buttons.cancel(digitalRead(FORWARD_PIN)==LOW,digitalRead(BACK_PIN)==LOW,millis());
 network.begin(false,"stage-remote-",false);generation=network.generation;Serial.printf("\nTheatre console %s role=remote; forward=GPIO13 back=GPIO14, active LOW; i/j=status\n",RADIO_VERSION);
}
void loop(){
 network.tick();uint32_t now=millis();if(generation!=network.generation){generation=network.generation;cancelLink(true);}
 if(hasPing&&now-lastPing>Remote::LEASE_MS)cancelLink(false);
 receiveRemote();now=millis();
 if(network.ready()&&masterKnown&&helloAt&&(int32_t)(now-helloAt)>=0){helloAt=0;Packet hello=message(REMOTE_HELLO);network.send(masterMAC,(const uint8_t*)&hello,sizeof(hello));}
 bool live=canOperate(now);auto action=buttons.tick(digitalRead(FORWARD_PIN)==LOW,digitalRead(BACK_PIN)==LOW,live,now);
 if(!live)outbox.clear();
 else {
  if(action!=Remote::NONE)outbox.push(action,token,clockMaster+(uint32_t)(now-clockAt),now);
  if(auto *event=outbox.due(now)){
   Packet command=message(REMOTE_EVENT,event->token);command.commandId=event->id;command.op=event->action;command.uptime=event->deadline;
   bool sent=network.send(masterMAC,(const uint8_t*)&command,sizeof(command));outbox.attempted(*event,now);
   if(sent){sentCommands++;if(!event->sent){event->sent=true;whiteUntil=now+80;}}
  }
 }
 bool identify=identifyUntil&&(int32_t)(identifyUntil-now)>0,pulse=whiteUntil&&(int32_t)(whiteUntil-now)>0;
 uint32_t color=networkLedColor(live?grade:OFFLINE,now,identify,pulse);static uint32_t previous=0xffffffff;
 if(color!=previous){rgbLedWrite(STATUS_PIN,(color>>16)&255,(color>>8)&255,color&255);previous=color;}
 while(Serial.available()){char c=Serial.read();if(c=='i'||c=='j')status();}
 delay(1);
}

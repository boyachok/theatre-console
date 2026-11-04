#pragma once
bool mountShowFS(){
 if(LittleFS.begin(false,"/littlefs",8,"storage"))return true;
 const auto *partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"storage");if(!partition)return false;
 uint8_t block[512];for(size_t at=0;at<partition->size;at+=sizeof(block)){if(esp_partition_read(partition,at,block,sizeof(block))!=ESP_OK)return false;for(auto byte:block)if(byte!=255){Serial.println("Storage is not blank: refusing automatic erase");return false;}}
 return LittleFS.format()&&LittleFS.begin(false,"/littlefs",8,"storage");
}
void handleShowAck(const Rx &r){
 int i=findNode(r.mac,false);if(i<0)return;Node &n=nodes[i];if(n.deviceBoot!=r.p.deviceBoot)return;
 n.scoreSupported=true;n.hasReply=true;n.lastReply=millis();n.lastMasterRssi=r.rssi;n.lastSlaveRssi=r.p.rssi;
 if(r.p.channel==0){if(r.p.reserved&2)n.scoreLoaded=r.p.commandId;n.scoreNext=r.p.value;return;}
 n.scoreReceived++;uint32_t sentAt=r.p.op|((uint32_t)r.p.padding[0]<<8)|((uint32_t)r.p.padding[1]<<16)|((uint32_t)r.p.padding[2]<<24);
 n.scoreRtt=(uint32_t)(millis()-sentAt)<60000?(uint32_t)(millis()-sentAt):-1;
 if(r.p.commandId==showMaster.run&&(!n.scoreSeq||Show::newer(r.p.seq,n.scoreSeq)||r.p.seq==n.scoreSeq)){n.scoreSeq=r.p.seq;if((r.p.reserved&1)&&r.p.result==APPLIED){n.scoreAt=millis();n.scoreStep=r.p.value;}else n.scoreAt=0;}
 if(!n.reportAt||(int32_t)(r.p.uptime-n.reportUptime)>=0){n.reported=r.p.state;n.reportAt=millis();n.reportUptime=r.p.uptime;}
}
bool handleShowMessage(const LanMessage &message){
 if(message.length!=sizeof(Show::Packet))return false;
 Show::Packet p;memcpy(&p,message.payload,sizeof(p));
 if(RADIO_MASTER||p.magic!=Show::MAGIC||p.protocol!=Show::PROTOCOL||p.size!=sizeof(p)||!masterKnown||memcmp(message.mac,masterMAC,6)||p.session!=masterSession||p.deviceBoot!=bootSession){network.rxInvalid++;return true;}
 if(p.type!=Show::STORE&&p.type!=Show::RUN)return true;
 masterSeen=lastProbe=millis();probeReceivedAt=millis();slaveHealth=p.health<=OFFLINE?(Health)p.health:BAD;
 bool accepted=false;
 if(!otaSafetyLock){if(p.type==Show::STORE)accepted=showStore.accept(p);else if(p.reserved&1){accepted=showRunner.apply(p,millis(),showRecovery&&showRunner.active);if(accepted)showRecovery=false;}else accepted=true;}
 Packet reply=packet(SCORE_ACK,masterSession);reply.deviceBoot=bootSession;reply.channel=p.type==Show::STORE?0:1;reply.result=accepted?APPLIED:NOT_READY;reply.rssi=message.rssi;reply.state=actors.snapshot();
 reply.op=p.sentAt&255;reply.padding[0]=(p.sentAt>>8)&255;reply.padding[1]=(p.sentAt>>16)&255;reply.padding[2]=(p.sentAt>>24)&255;
 if(p.type==Show::STORE){reply.commandId=showStore.version;reply.value=showStore.nextMissing();if(showStore.version==p.version&&showStore.checksum==p.checksum)reply.reserved|=2;}
 else{reply.commandId=showRunner.run;reply.seq=showRunner.sequence;reply.value=showRunner.step;if(showRunner.active)reply.reserved|=1;}
 sendPacket(masterMAC,reply);return true;
}
void showMasterTick(){
 if(!RADIO_MASTER||!network.ready()||networkResetActive)return;
 static uint32_t slot=0;uint32_t now=millis();if((int32_t)(now-slot)<0)return;
 static int cursor=0;
 for(int k=0;k<MAX_NODES;k++){
  int i=cursor;cursor=(cursor+1)%MAX_NODES;Node &n=nodes[i];if(!n.used||!n.deviceBoot||!n.scoreSupported||(uint32_t)network.peerIP(n.mac)==0)continue;
  Show::Packet p{};p.magic=Show::MAGIC;p.protocol=Show::PROTOCOL;p.size=sizeof(p);p.session=bootSession;p.deviceBoot=n.deviceBoot;p.run=showMaster.run;p.transition=showMaster.sequence;p.sentAt=now;p.health=metrics(n).health;
  if((int32_t)(now-n.nextScore)>=0){
   n.nextScore=now+500;p.type=Show::RUN;
   auto plan=showMaster.find(n.mac,showMaster.isPreview);
   if(showMaster.active&&plan){uint16_t index=showMaster.isPreview?0:showMaster.step;if(index<plan->cues.size()){p.reserved=1;p.cue=plan->cues[index];p.version=showMaster.isPreview?0:showMaster.version;p.step=showMaster.step;p.total=showMaster.isPreview?showMaster.previewTotal:showMaster.stepIds.size();p.reason=showMaster.reason;}}
   network.send(n.mac,(const uint8_t*)&p,sizeof(p));n.scoreSent++;slot=now+10;return;
  }
  auto plan=showMaster.find(n.mac);
  if(plan&&n.scoreLoaded!=showMaster.version&&(int32_t)(now-n.nextStore)>=0){
   n.nextStore=now+100;p.type=Show::STORE;p.version=showMaster.version;p.checksum=plan->checksum;p.total=plan->cues.size();p.step=n.scoreNext<p.total?n.scoreNext:0;p.cue=plan->cues[p.step];network.send(n.mac,(const uint8_t*)&p,sizeof(p));slot=now+10;return;
  }
 }
}
void invalidateShowDelivery(){for(auto &n:nodes){n.nextScore=n.nextStore=0;n.scoreLoaded=n.scoreNext=0;}}
void consoleButtons(){
 if(!RADIO_MASTER)return;
 struct Key{uint8_t pin;bool raw=false,pressed=false,longFired=false;uint32_t changed=0,at=0;};static Key keys[2]{{BUTTON_FORWARD_PIN},{BUTTON_BACK_PIN}};
 uint32_t now=millis();for(int i=0;i<2;i++){auto &key=keys[i];bool down=digitalRead(key.pin)==LOW;
  if(down!=key.raw){key.raw=down;key.changed=now;}
  if(now-key.changed>=30&&down!=key.pressed){key.pressed=down;if(down){key.at=now;key.longFired=false;}else if(!key.longFired&&showMaster.stepIds.size()){
   uint16_t next=!showMaster.active?0:i==0?std::min((int)showMaster.stepIds.size()-1,(int)showMaster.step+1):std::max(0,(int)showMaster.step-1);
   if(!showMaster.active||showMaster.isPreview||next!=showMaster.step){showMaster.go(next,i==1?1:0);for(auto &n:nodes)n.nextScore=0;}
  }}
  if(key.pressed&&!key.longFired&&now-key.at>=3000&&showMaster.stepIds.size()){key.longFired=true;uint16_t next=i==0?showMaster.stepIds.size()-1:0;if(!showMaster.active||showMaster.isPreview||next!=showMaster.step){showMaster.go(next,i==1?1:0);for(auto &n:nodes)n.nextScore=0;}}
 }
}
bool consoleForm(){if(!RADIO_MASTER){web.send(403,"text/plain","Master only");return false;}return validForm();}
bool guardShowRequest(){
 static String client;static uint32_t sequence=0;uint32_t deadline,seq;
 if(!unsignedArg("deadline",0xffffffffUL,deadline)||!freshDeadline(millis(),deadline)||!unsignedArg("seq",0x7fffffff,seq)){web.send(408,"text/plain","Устаревший запрос");return false;}
 String next=web.arg("client");if(!next.length()||next.length()>40||(client==next&&seq<=sequence)){web.send(409,"text/plain","Повтор запроса");return false;}client=next;sequence=seq;return true;
}
void consoleRoutes(){
 web.on("/api/console/status",HTTP_GET,[]{web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",statusJSON(false));});
 web.on("/api/console/workspace",HTTP_GET,[]{if(!RADIO_MASTER){web.send(403);return;}if(!showFSReady||showMaster.workspaceFault){web.send(503,"text/plain","Не удалось прочитать память партитуры: восстановите JSON-бэкап");return;}web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",showMaster.workspace.length()?showMaster.workspace:String("null"));});
 web.on("/api/console/workspace",HTTP_POST,[]{
  if(!consoleForm())return;if(!showFSReady){web.send(503);return;}String body=web.takePlain();if(body.length()>2200000){web.send(413);return;}JsonDocument filter,doc(&Show::jsonRam);filter["format"]=true;filter["schemaVersion"]=true;filter["published"]["version"]=true;
  if(deserializeJson(doc,body,DeserializationOption::Filter(filter))||doc["format"]!="theatre-score"||doc["schemaVersion"]!=1){web.send(400);return;}
  if((doc["published"]["version"]|0U)<showMaster.version){web.send(409,"text/plain","Версия уже изменилась");return;}
  if(!Show::atomicText("/workspace.json",body)){web.send(507);return;}showMaster.workspace=std::move(body);showMaster.workspaceFault=false;web.send(200,"text/plain","OK");
 });
 web.on("/api/console/publish",HTTP_POST,[]{
  if(!consoleForm())return;if(!showFSReady){web.send(503);return;}String body=web.takePlain(),error;if(body.length()>3000000){web.send(413);return;}
  if(!showMaster.loadPayload(body,true,error)){web.send(400,"text/plain",error);return;}invalidateShowDelivery();web.send(200,"application/json","{\"version\":"+String(showMaster.version)+"}");
 });
 web.on("/api/console/preview",HTTP_POST,[]{
  if(!consoleForm()||!guardShowRequest())return;String body=web.takePlain(),error;if(body.length()>60000){web.send(413);return;}
  if(!showMaster.previewPayload(body,error)){web.send(400,"text/plain",error);return;}for(auto &n:nodes)n.nextScore=0;
  web.send(200,"application/json","{\"transition\":"+String(showMaster.sequence)+"}");
 });
 web.on("/api/console/measure",HTTP_POST,[]{if(!consoleForm())return;uint32_t enabled;if(!unsignedArg("enabled",1,enabled)){web.send(400);return;}setMeasurements(enabled);web.send(200,"text/plain","OK");});
 web.on("/api/console/network/reset",HTTP_POST,[]{if(!consoleForm())return;beginNetworkReset();web.send(202,"text/plain","OK");});
 web.on("/api/console/identify",HTTP_POST,[]{if(!consoleForm())return;String mac=web.arg("mac");if(mac.equalsIgnoreCase(macString(network.mac))){identifyUntil=millis()+5000;web.send(200);return;}uint8_t target[6];if(!Show::parseMAC(mac.c_str(),target)){web.send(400);return;}int i=findNode(target,false);if(i<0){web.send(404);return;}bool ok=sendPacket(target,packet(IDENTIFY,bootSession));web.send(ok?200:503);});
}

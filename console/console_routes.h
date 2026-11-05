#pragma once
#include "show_filesystem.h"
#include "scene_delivery.h"
void consoleButtons(){
 if(!RADIO_MASTER)return;
 static Remote::Buttons keys;auto action=keys.tick(digitalRead(BUTTON_FORWARD_PIN)==LOW,digitalRead(BUTTON_BACK_PIN)==LOW,!otaSafetyLock&&!networkResetActive&&!showMaster.stepIds.empty(),millis());
 if(action!=Remote::NONE)navigatePublished(action);
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
 web.on("/api/console/go",HTTP_POST,[]{
  if(!consoleForm()||!guardShowRequest())return;uint32_t target,version,reason;
  if(!unsignedArg("step",Show::MAX_STEPS-1,target)||!unsignedArg("version",0xffffffffUL,version)||!unsignedArg("reason",1,reason)){web.send(400);return;}
  if(version!=showMaster.version){web.send(409,"text/plain","Опубликованная версия изменилась: обновите партитуру");return;}
  if(!goPublished(target,reason)){web.send(409,"text/plain","Показ не готов: дождитесь загрузки партитур, проверьте версии контроллеров в Сети");return;}
  web.send(200,"application/json","{\"transition\":"+String(showMaster.sequence)+",\"step\":"+String(showMaster.step+1)+"}");
 });
 web.on("/api/console/preview",HTTP_POST,[]{
  if(!consoleForm()||!guardShowRequest())return;String body=web.takePlain(),error;if(body.length()>60000){web.send(413);return;}
  if(!showMaster.previewPayload(body,error)){web.send(400,"text/plain",error);return;}for(auto &n:nodes)n.nextScore=0;
  web.send(200,"application/json","{\"transition\":"+String(showMaster.sequence)+"}");
 });
 web.on("/api/console/measure",HTTP_POST,[]{if(!consoleForm())return;uint32_t enabled;if(!unsignedArg("enabled",1,enabled)){web.send(400);return;}setMeasurements(enabled);web.send(200,"text/plain","OK");});
 web.on("/api/console/network/reset",HTTP_POST,[]{if(!consoleForm())return;beginNetworkReset();web.send(202,"text/plain","OK");});
 web.on("/api/console/identify",HTTP_POST,[]{if(!consoleForm())return;String mac=web.arg("mac");if(mac.equalsIgnoreCase(macString(network.mac))){identifyUntil=millis()+5000;web.send(200);return;}uint8_t target[6];if(!Show::parseMAC(mac.c_str(),target)){web.send(400);return;}if(identifyRemote(target)){web.send(200);return;}int i=findNode(target,false);if(i<0){web.send(404);return;}bool ok=sendPacket(target,packet(IDENTIFY,bootSession));web.send(ok?200:503);});
}

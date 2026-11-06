#pragma once
#include <Update.h>
#include <esp_ota_ops.h>
#include <strings.h>
// Callbacks run on loopTask via ControlWebServer; parsing/socket waits use its worker.
void remoteMAC(char (&out)[18]){snprintf(out,sizeof(out),"%02X:%02X:%02X:%02X:%02X:%02X",network.mac[0],network.mac[1],network.mac[2],network.mac[3],network.mac[4],network.mac[5]);}
bool remoteAuthorized(){
 char mac[18];remoteMAC(mac);
 if(strcmp(web.arg("token").c_str(),otaToken)||!otaToken[0])return false;
 if(strcasecmp(web.arg("target").c_str(),mac)||strcmp(web.arg("role").c_str(),"remote"))return false;
 String origin=web.header("Origin");
 if(strlen(origin.c_str())){char expected[160];snprintf(expected,sizeof(expected),"http://%s",web.hostHeader().c_str());if(strcmp(origin.c_str(),expected))return false;}
 return true;
}
void failRemoteUpdate(const char *error){
 bool wasLocked=otaLocked||otaWriting;
 if(otaWriting)Update.abort();otaWriting=false;otaOK=otaImageComplete=false;otaLocked=false;otaError=error;
 if(wasLocked)cancelInputs(); // A held button cannot turn into a command after failure.
}
String remoteStatusJSON(){
 char mac[18],age[24],body[1800];remoteMAC(mac);
 const auto *partition=esp_ota_get_running_partition();
 if(hasPing)snprintf(age,sizeof(age),"%lu",(unsigned long)(millis()-lastPing));else strcpy(age,"null");
 snprintf(body,sizeof(body),"{\"role\":\"remote\",\"version\":\"%s\",\"firmwareRole\":\"%s\",\"mac\":\"%s\",\"ip\":\"%s\",\"hostname\":\"%s\",\"token\":\"%s\",\"serverMs\":%lu,\"bootId\":%lu,\"wifi\":%s,\"connected\":%s,\"masterReady\":%s,\"buttonsEnabled\":%s,\"pingAgeMs\":%s,\"rssi\":%d,\"forwardDown\":%s,\"backDown\":%s,\"pending\":%u,\"sent\":%lu,\"acks\":%lu,\"lastAck\":%lu,\"lastResult\":%u,\"step\":%u,\"whitePulses\":%lu,\"diagSent\":%lu,\"diagAcks\":%lu,\"diagResult\":%u,\"otaSupported\":%s,\"otaBusy\":%s,\"otaBytes\":%u,\"otaError\":\"%s\",\"appAddress\":%lu,\"otaSlot\":\"%s\",\"buttonTest\":%s,\"statusPin\":%u,\"forwardRawEdges\":%lu,\"backRawEdges\":%lu,\"forwardPresses\":%lu,\"backPresses\":%lu,\"forwardReleases\":%lu,\"backReleases\":%lu}",
 RADIO_VERSION,RemoteOta::ROLE_MARKER,mac,WiFi.localIP().toString().c_str(),network.hostname.c_str(),otaToken,(unsigned long)millis(),(unsigned long)firmwareBoot,
 network.ready()?"true":"false",network.ready()?"true":"false",canOperate(millis())?"true":"false",canOperate(millis())&&!otaLocked&&!buttonTest?"true":"false",age,WiFi.RSSI(),
 digitalRead(FORWARD_PIN)==LOW?"true":"false",digitalRead(BACK_PIN)==LOW?"true":"false",outbox.size(),(unsigned long)sentCommands,(unsigned long)ackedCommands,(unsigned long)lastAck,lastResult,currentStep,
 (unsigned long)whitePulses,(unsigned long)diagSent,(unsigned long)diagAcks,diagResult,httpReady?"true":"false",otaLocked?"true":"false",(unsigned)otaImage.size(),otaError,(unsigned long)(partition?partition->address:0),partition?partition->label:"unknown",buttonTest?"true":"false",STATUS_PIN,(unsigned long)buttonProbe.edges[0],(unsigned long)buttonProbe.edges[1],(unsigned long)buttonProbe.presses[0],(unsigned long)buttonProbe.presses[1],(unsigned long)buttonProbe.releases[0],(unsigned long)buttonProbe.releases[1]);
 return String(body);
}
void remoteWebBegin(){
 const char *headers[]={"Origin"};web.collectHeaders(headers,1);
 auto statusRoute=[]{web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",remoteStatusJSON());};
 web.on("/api/status",HTTP_GET,statusRoute);web.on("/api/remote/status",HTTP_GET,statusRoute);
 web.on("/",HTTP_GET,[]{
  char mac[18],body[2300];remoteMAC(mac);
  snprintf(body,sizeof(body),"<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Theatre remote</title><h2>Theatre remote %s</h2><p>MAC %s · GPIO13 вперёд / GPIO14 назад</p><p>Только обслуживание пульта, не редактор спектакля.</p><form method='post' action='/api/remote/identify?token=%s&target=%s&role=remote'><button>Мигать белым 5 секунд</button></form><form method='post' action='/api/remote/tx-test?token=%s&target=%s&role=remote'><button>Белая TX-вспышка без переключения сцены</button></form><p>TX-тест: нулевой ID/действие, мастер должен отвергнуть его как STALE.</p><a href='/buttons'>Проверка кнопок без сцен</a> · <a href='/api/status'>Статус</a><h3>OTA · только remote .bin</h3><form method='post' action='/update?token=%s&target=%s&role=remote' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><button>Обновить пульт</button></form><p>Доверенная LAN, без TLS/подписи образа. Не нажимать кнопки во время обновления.</p>",RADIO_VERSION,mac,otaToken,mac,otaToken,mac,otaToken,mac);
  web.send(200,"text/html; charset=utf-8",String(body));
 });
 web.on("/buttons",HTTP_GET,[]{
  char mac[18],body[1900];remoteMAC(mac);
  snprintf(body,sizeof(body),"<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><h2>Проверка кнопок: %s</h2><p>В тесте навигация заблокирована, режим сохраняется при перезагрузке. Фиолетовый: отпущены. Белый: одна нажата. Жёлтый: обе нажаты. Отпускание отправляет только безопасный нулевой TX-тест.</p><form method='post' action='/api/remote/buttons-test?token=%s&target=%s&role=remote&enabled=1'><button>Включить безопасную проверку</button></form><form method='post' action='/api/remote/buttons-test?token=%s&target=%s&role=remote&enabled=0'><button>Завершить тест и разрешить навигацию</button></form><a href='/api/status'>GPIO и счётчики</a>",buttonTest?"ВКЛЮЧЕНА":"выключена",otaToken,mac,otaToken,mac);
  web.send(200,"text/html; charset=utf-8",String(body));
 });
 web.on("/api/remote/buttons-test",HTTP_POST,[]{
  if(!remoteAuthorized()){web.send(403,"text/plain","Wrong token/target/role/origin");return;}
  if(otaLocked||restartAt){web.send(409,"text/plain","OTA/reboot active");return;}
  String value=web.arg("enabled");
  if(strcmp(value.c_str(),"1")&&strcmp(value.c_str(),"0")){web.send(400,"text/plain","enabled must be 1 or 0");return;}
  if(!setButtonTest(!strcmp(value.c_str(),"1"))){web.send(500,"text/plain","Cannot persist test lock");return;}
  web.send(200,"application/json",remoteStatusJSON());
 });
 web.on("/api/remote/identify",HTTP_POST,[]{
  if(!remoteAuthorized()){web.send(403,"text/plain","Wrong token/target/role/origin");return;}
  identifyUntil=millis()+5000;web.send(200,"text/plain","LED identification only; no scene command");
 });
 web.on("/api/remote/tx-test",HTTP_POST,[]{
  if(!remoteAuthorized()){web.send(403,"text/plain","Wrong token/target/role/origin");return;}
  bool sent=diagnosticTX();web.send(sent?202:409,"text/plain",sent?"Diagnostic TX sent; expect STALE ACK, no scene":"No fresh link, busy or OTA active");
 });
 web.on("/update",HTTP_POST,[]{
  bool authorized=remoteAuthorized();
  if(authorized&&otaWriting&&otaImageComplete){
   if(Update.end(true)){otaWriting=false;otaOK=true;}else failRemoteUpdate("invalid-image");
  }
  web.send(!authorized?403:otaOK?200:400,"text/plain",!authorized?"Wrong token/target/role/origin":otaOK?"Remote update OK. Rebooting.":otaError);
  if(authorized&&otaOK)restartAt=millis()+700;
  else if(uploadSeen)failRemoteUpdate(otaError);
  uploadSeen=false;
 },[]{
  HTTPUpload &u=web.upload();
  if(u.status==UPLOAD_FILE_START){
   if(uploadSeen){failRemoteUpdate("multiple-files");return;}
   uploadSeen=true;otaOK=otaImageComplete=false;otaError="none";otaImage.reset();
   if(!remoteAuthorized()){otaError="unauthorized";return;}
   if(restartAt){otaError="reboot-pending";return;}
   if(strcmp(u.name.c_str(),"firmware")){otaError="wrong-field";return;}
   otaLocked=true;diagPending=false;cancelInputs();lastUploadAt=millis();
   otaWriting=Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);
   if(!otaWriting)failRemoteUpdate("begin-failed");
  }else if(u.status==UPLOAD_FILE_WRITE&&otaWriting){
   lastUploadAt=millis();
   if(!otaImage.feed(u.buf,u.currentSize)){failRemoteUpdate("bad-header-or-oversize");return;}
   if(Update.write(u.buf,u.currentSize)!=u.currentSize)failRemoteUpdate("write-failed");
  }else if(u.status==UPLOAD_FILE_END&&otaWriting){
   if(!otaImage.acceptable()){failRemoteUpdate("not-a-remote-image");return;}
   otaImageComplete=true; // Commit only in the final POST handler, after all multipart files.
  }else if(u.status==UPLOAD_FILE_ABORTED){failRemoteUpdate("upload-aborted");uploadSeen=false;}
 });
 web.onNotFound([]{web.send(404,"text/plain","Remote maintenance only");});
 web.begin();httpReady=web.startWorker();
}
void remoteWebTick(){
 web.service();
 if(otaWriting&&millis()-lastUploadAt>=15000)failRemoteUpdate("upload-timeout");
}

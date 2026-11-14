#pragma once
#include <Update.h>
#include <esp_ota_ops.h>
#include <strings.h>
// All state/Update callbacks execute on helmet-control; sockets/parser use stage-http.
void helmetMAC(char (&out)[18]){snprintf(out,sizeof(out),"%02X:%02X:%02X:%02X:%02X:%02X",network.mac[0],network.mac[1],network.mac[2],network.mac[3],network.mac[4],network.mac[5]);}
const esp_partition_t *helmetUpdatePartition(){const auto *next=esp_ota_get_next_update_partition(nullptr);const auto *running=esp_ota_get_running_partition();return next&&running&&next->address!=running->address&&next->size>=256?next:nullptr;}
bool helmetAuthorized(){
 char mac[18];helmetMAC(mac);
 if(!otaToken[0]||strcmp(web.arg("token").c_str(),otaToken)||strcasecmp(web.arg("target").c_str(),mac)||strcmp(web.arg("role").c_str(),"helmet"))return false;
 String origin=web.header("Origin");if(strlen(origin.c_str())){String expected=String("http://")+web.hostHeader();if(strcmp(origin.c_str(),expected.c_str()))return false;}return true;
}
void failHelmetUpdate(const char *error){
 bool locked=otaLocked||otaWriting;if(otaWriting)Update.abort();otaWriting=otaLocked=otaOK=otaImageComplete=false;otaError=error;
 if(locked)runner.stop(); // No automatic sound/light restoration after an aborted maintenance update.
}
String helmetOtaStatusJSON(){
 const auto *running=esp_ota_get_running_partition(),*next=helmetUpdatePartition();char buf[700];
 snprintf(buf,sizeof(buf),"\"token\":\"%s\",\"bootId\":%lu,\"firmwareRole\":\"%s\",\"hostname\":\"%s\",\"otaSupported\":%s,\"otaBusy\":%s,\"otaBytes\":%u,\"otaError\":\"%s\",\"appAddress\":%lu,\"otaSlot\":\"%s\",\"otaNextAddress\":%lu,\"otaSlotBytes\":%u",otaToken,(unsigned long)firmwareBoot,HelmetOta::ROLE_MARKER,network.hostname.c_str(),httpReady&&next?"true":"false",otaLocked?"true":"false",(unsigned)otaImage.size(),otaError,(unsigned long)(running?running->address:0),running?running->label:"unknown",(unsigned long)(next?next->address:0),(unsigned)(next?next->size:0));return String(buf);
}
void helmetWebBegin(){
 const char *headers[]={"Origin"};web.collectHeaders(headers,1);
 auto statusRoute=[]{web.sendHeader("Cache-Control","no-store");web.send(200,"application/json",helmetStatusJSON());};
 web.on("/api/status",HTTP_GET,statusRoute);web.on("/api/helmet/status",HTTP_GET,statusRoute);
 web.on("/",HTTP_GET,[]{
  char mac[18],body[2000];helmetMAC(mac);const auto *next=helmetUpdatePartition();
  snprintf(body,sizeof(body),"<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Theatre helmet OTA</title><h2>Theatre helmet %s</h2><p>MAC %s</p><p>Обслуживание шлема. Звук/свет настраиваются в master.</p><a href='/api/status'>Статус</a><h3>OTA: только helmet .bin</h3><p>Максимум %u байт текущего OTA-слота. Во время обновления звук и свет останавливаются. Ошибка оставляет STOP; новый запуск — явной командой или reboot.</p><form method='post' action='/update?token=%s&target=%s&role=helmet' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><button>Обновить шлем</button></form><p>Сохранённые настройки, банк и разметка не стираются. После успешного reboot автономный старт сохранённых настроек. Только доверенная LAN: без TLS и криптографической подписи образа.</p>",RADIO_VERSION,mac,(unsigned)(next?next->size:0),otaToken,mac);
  web.send(200,"text/html; charset=utf-8",String(body));
 });
 web.on("/update",HTTP_POST,[]{
  if(!helmetAuthorized()){web.send(403,"text/plain","Wrong token/target/role/origin");return;}
  if(restartAt){web.send(409,"text/plain","Reboot pending");return;}
  if(otaWriting&&otaImageComplete){if(Update.end(true)){otaWriting=false;otaOK=true;}else failHelmetUpdate("invalid-image");}
  web.send(otaOK?200:400,"text/plain",otaOK?"Helmet update OK. Rebooting.":otaError);
  if(otaOK)restartAt=millis()+700;else if(uploadSeen)failHelmetUpdate(otaError);uploadSeen=false;
 },[]{
  auto &u=web.upload();
  if(u.status==UPLOAD_FILE_START){
   if(!helmetAuthorized()){otaError="unauthorized";return;}
   if(restartAt){otaError="reboot-pending";return;}
   if(uploadSeen){failHelmetUpdate("multiple-files");return;}
   uploadSeen=true;otaOK=otaImageComplete=false;otaError="none";
   if(strcmp(u.name.c_str(),"firmware")){otaError="wrong-field";return;}
   const auto *next=helmetUpdatePartition();if(!next){otaError="no-ota-slot";return;}
   otaImage.reset(next->size);otaLocked=true;runner.stop();
   if(!runner.defaults.save()){failHelmetUpdate("startup-save-failed");return;}
   lastUploadAt=millis();otaWriting=Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);if(!otaWriting)failHelmetUpdate("begin-failed");
  }else if(u.status==UPLOAD_FILE_WRITE&&otaWriting){
   lastUploadAt=millis();if(!otaImage.feed(u.buf,u.currentSize)){failHelmetUpdate("bad-header-chip-or-oversize");return;}
   if(Update.write(u.buf,u.currentSize)!=u.currentSize)failHelmetUpdate("write-failed");
  }else if(u.status==UPLOAD_FILE_END&&otaWriting){
   if(!otaImage.acceptable()){failHelmetUpdate("not-a-helmet-image");return;}otaImageComplete=true;
  }else if(u.status==UPLOAD_FILE_ABORTED&&uploadSeen){failHelmetUpdate("upload-aborted");uploadSeen=false;}
 });
 web.onNotFound([]{web.send(404,"text/plain","Helmet maintenance only");});web.begin();httpReady=web.startWorker();
}
void helmetWebTick(){web.service();if(otaWriting&&millis()-lastUploadAt>=15000)failHelmetUpdate("upload-timeout");}

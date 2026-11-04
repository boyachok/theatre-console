#pragma once
#include <WebServer.h>
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
// HTTP parsing/socket waits live on another task. State, UDP and actors remain
// owned by loop(): callbacks execute there, but their HTTP replies are deferred.
class ControlWebServer:public WebServer {
  struct Reply {bool present=false,flash=false;int code=200;String type,body;const char *data=nullptr;size_t length=0;bool sized=false;};
  struct Job {THandlerFunction fn;SemaphoreHandle_t done;Reply reply;};
  QueueHandle_t jobs=nullptr;Job *current=nullptr;
  THandlerFunction wrap(THandlerFunction fn){
    if(!fn)return fn;
    return [this,fn]{
      Job job{fn,xSemaphoreCreateBinary(),{}};
      if(!job.done){WebServer::send(503,"text/plain","Busy");return;}
      Job *ptr=&job;xQueueSend(jobs,&ptr,portMAX_DELAY);xSemaphoreTake(job.done,portMAX_DELAY);vSemaphoreDelete(job.done);
      if(job.reply.present){
        if(job.reply.flash){if(job.reply.sized)WebServer::send_P(job.reply.code,job.reply.type.c_str(),job.reply.data,job.reply.length);else WebServer::send_P(job.reply.code,job.reply.type.c_str(),job.reply.data);}
        else WebServer::send(job.reply.code,job.reply.type.c_str(),job.reply.body);
      }
    };
  }
  static void worker(void *arg){
    auto &self=*static_cast<ControlWebServer*>(arg);
    for(;;){
      // Browsers may open speculative idle TCP connections. Do not let one
      // reserve the single-client server for Arduino's default five seconds.
      if(self._currentStatus==HC_WAIT_READ&&!self._currentClient.available()&&millis()-self._statusChange>100){
        self._currentClient.stop();self._currentStatus=HC_NONE;
      }
      self.WebServer::handleClient();vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
public:
  explicit ControlWebServer(int port):WebServer(port){}
  String takePlain(){
    for(int j=0;j<_postArgsLen;j++)if(_postArgs[j].key=="plain")return std::move(_postArgs[j].value);
    for(int i=0;i<_currentArgCount;i++)if(_currentArgs[i].key=="plain")return std::move(_currentArgs[i].value);
    return String();
  }
  bool startWorker(){jobs=xQueueCreate(2,sizeof(Job*));return jobs&&xTaskCreatePinnedToCore(worker,"stage-http",12288,this,1,nullptr,0)==pdPASS;}
  void service(){
    Job *job;int budget=2;
    while(budget--&&jobs&&xQueueReceive(jobs,&job,0)==pdTRUE){current=job;job->fn();current=nullptr;xSemaphoreGive(job->done);}
  }
  RequestHandler &on(const Uri &uri,HTTPMethod method,THandlerFunction fn){return WebServer::on(uri,method,wrap(fn));}
  RequestHandler &on(const Uri &uri,HTTPMethod method,THandlerFunction fn,THandlerFunction upload){return WebServer::on(uri,method,wrap(fn),wrap(upload));}
  void onNotFound(THandlerFunction fn){WebServer::onNotFound(wrap(fn));}
  void send(int code,const char *type=nullptr,const String &body=String("")){
    if(!current){WebServer::send(code,type,body);return;}
    current->reply.present=true;current->reply.code=code;current->reply.type=type?type:"text/plain";current->reply.body=body;
  }
  void send(int code,const char *type,String &&body){
    if(!current){WebServer::send(code,type,body);return;}
    current->reply.present=true;current->reply.code=code;current->reply.type=type?type:"text/plain";current->reply.body=std::move(body);
  }
  void send_P(int code,PGM_P type,PGM_P data,size_t length){
    if(!current){WebServer::send_P(code,type,data,length);return;}
    current->reply.present=true;current->reply.flash=true;current->reply.sized=true;current->reply.length=length;current->reply.code=code;current->reply.type=type;current->reply.data=data;
  }
  void send_P(int code,PGM_P type,PGM_P data){
    if(!current){WebServer::send_P(code,type,data);return;}
    current->reply.present=true;current->reply.flash=true;current->reply.code=code;current->reply.type=type;current->reply.data=data;
  }
};

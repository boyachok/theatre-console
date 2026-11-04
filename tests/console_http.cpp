#include <cassert>
#include <cstdio>
#include "../console/control_web.h"
int main(){
 ControlWebServer web(80);assert(web.startWorker());const char binary[]={'a',0,'b',0};auto controller=std::this_thread::get_id();
 web.on("/asset",HTTP_GET,[&]{assert(std::this_thread::get_id()==controller);web.send_P(200,"text/javascript",binary,sizeof(binary));});
 mockAllowWrite=false;std::thread http([&]{web.handler();});auto end=millis()+2000;
 while(!mockSending&&millis()<end){web.service();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 assert(mockSending&&mockWriteThread!=controller);int ticks=0;auto start=millis();while(millis()-start<50){web.service();ticks++;std::this_thread::sleep_for(std::chrono::milliseconds(1));}assert(ticks>10);
 mockAllowWrite=true;http.join();assert(web.response.size()==4&&web.response[1]==0&&web.response[2]=='b');for(auto q:mockQueues)delete q;
 puts("Console gzip assets preserve embedded NUL bytes; slow HTTP client never owns controller task");
}

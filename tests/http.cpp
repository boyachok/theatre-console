#include <cassert>
#include <cstdio>
#include "../control/control_web.h"
int main(){
 ControlWebServer web(80);assert(web.startWorker());
 const auto controller=std::this_thread::get_id();int calls=0;
 web.on("/command",HTTP_POST,[&]{assert(std::this_thread::get_id()==controller);calls++;web.send(202,"text/plain","ACK");});
 mockAllowWrite=false;std::thread http([&]{web.handler();});
 auto end=millis()+2000;
 while(!mockSending&&millis()<end){web.service();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 assert(mockSending&&calls==1&&mockWriteThread!=controller);
 // A stuck TCP writer cannot occupy the controller/heartbeat task.
 int ticks=0;auto start=millis();while(millis()-start<100){web.service();ticks++;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 assert(ticks>20&&web.response.empty());mockAllowWrite=true;http.join();assert(web.response=="ACK");
 mockSending=false;web.on("/",HTTP_GET,[&]{web.send_P(200,"text/html","FLASH");});
 std::thread page([&]{web.handler();});end=millis()+2000;while(!mockSending&&millis()<end){web.service();std::this_thread::sleep_for(std::chrono::milliseconds(1));}page.join();assert(web.response=="FLASH");
 for(auto q:mockQueues)delete q;
 puts("HTTP callbacks owned by controller; blocked TCP replies leave controller/heartbeat running; deferred RAM/flash replies passed");
}

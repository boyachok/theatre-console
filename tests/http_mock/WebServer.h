#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
using String=std::string;using PGM_P=const char*;using Uri=std::string;
inline uint32_t millis(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
enum HTTPMethod{HTTP_GET,HTTP_POST};enum HTTPClientStatus{HC_NONE,HC_WAIT_READ,HC_WAIT_CLOSE};
struct RequestHandler{};
struct ClientMock{int available(){return 0;}void stop(){}};
inline std::atomic<bool> mockAllowWrite{true},mockSending{false};inline std::thread::id mockWriteThread;
class WebServer {
protected:
 struct RequestArgument {String key,value;};int _currentArgCount=0,_postArgsLen=0;RequestArgument *_currentArgs=nullptr,*_postArgs=nullptr;
 HTTPClientStatus _currentStatus=HC_NONE;ClientMock _currentClient;uint32_t _statusChange=0;
public:
 using THandlerFunction=std::function<void()>;THandlerFunction handler,upload;String response;RequestHandler request;
 explicit WebServer(int){}
 RequestHandler &on(const Uri&,HTTPMethod,THandlerFunction fn){handler=fn;return request;}
 RequestHandler &on(const Uri&,HTTPMethod,THandlerFunction fn,THandlerFunction uf){handler=fn;upload=uf;return request;}
 void onNotFound(THandlerFunction fn){handler=fn;}
 void handleClient(){}
 void send(int,const char *type=nullptr,const String &body=String("")){(void)type;mockWriteThread=std::this_thread::get_id();mockSending=true;while(!mockAllowWrite)std::this_thread::sleep_for(std::chrono::milliseconds(1));response=body;}
 void send_P(int code,PGM_P type,PGM_P body,size_t length){send(code,type,String(body,length));}
 void send_P(int code,PGM_P type,PGM_P body){send(code,type,String(body));}
};

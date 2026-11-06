#pragma once
// Mock only the HTTP/task boundary. Actual ControlWebServer worker is covered separately.
#include <functional>
#include <cassert>
#include <map>
#include <string>
#include <cstring>
enum HTTPMethod {HTTP_GET,HTTP_POST};
enum HTTPUploadStatus {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED};
struct HTTPUpload {HTTPUploadStatus status=UPLOAD_FILE_START;String name{"firmware"};String filename{"remote.bin"};uint8_t buf[2048]{};size_t currentSize=0;};
class ControlWebServer {
public:
 struct Route {std::function<void()> handler,upload;};
 std::map<std::string,Route> routes;std::map<std::string,std::string> args,headers;
 HTTPUpload data;int code=0;std::string response;bool worker=true;
 explicit ControlWebServer(int){}
 void on(const char *path,HTTPMethod method,std::function<void()> fn){routes[std::string(path)+std::to_string(method)]={fn,{}};}
 void on(const char *path,HTTPMethod method,std::function<void()> fn,std::function<void()> upload){routes[std::string(path)+std::to_string(method)]={fn,upload};}
 void onNotFound(std::function<void()>){ }void collectHeaders(const char**,size_t){}void begin(){}bool startWorker(){return worker;}void service(){}
 String arg(const char *key){return String(args[key]);}String header(const char *key){return String(headers[key]);}String hostHeader(){return String("192.168.50.88");}
 HTTPUpload &upload(){return data;}void sendHeader(const char*,const char*){}
 void send(int status,const char*,const String &body){code=status;response=body.c_str();}
 void request(const char *path,HTTPMethod method){routes.at(std::string(path)+std::to_string(method)).handler();}
 void uploadEvent(HTTPUploadStatus status,const uint8_t *bytes=nullptr,size_t n=0){data.status=status;data.currentSize=n;if(bytes){assert(n<=sizeof(data.buf));memcpy(data.buf,bytes,n);}routes.at("/update"+std::to_string(HTTP_POST)).upload();}
};

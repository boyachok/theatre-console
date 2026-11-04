#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
class MockFile {
 std::shared_ptr<std::vector<uint8_t>> bytes;size_t pos=0;
public:
 MockFile()=default;explicit MockFile(std::shared_ptr<std::vector<uint8_t>> b):bytes(b){}
 explicit operator bool()const{return !!bytes;}
 size_t read(uint8_t *out,size_t n){if(!bytes)return 0;n=std::min(n,bytes->size()-std::min(pos,bytes->size()));memcpy(out,bytes->data()+pos,n);pos+=n;return n;}
 size_t write(const uint8_t *data,size_t n){if(!bytes)return 0;bytes->insert(bytes->end(),data,data+n);return n;}
 std::string readString(){return bytes?std::string(bytes->begin(),bytes->end()):std::string();}
 size_t print(const std::string &s){return write((const uint8_t*)s.data(),s.size());}
 void close(){}
};
struct MockFS {
 std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;bool failPlanRename=false;size_t peakBytes=0;
 size_t size()const{size_t result=0;for(auto &item:files)result+=item.second->size();return result;}
 MockFile open(const std::string &name,const char *mode){if(mode[0]=='w')files[name]=std::make_shared<std::vector<uint8_t>>();auto it=files.find(name);return it==files.end()?MockFile():MockFile(it->second);}
 bool rename(const std::string &from,const std::string &to){peakBytes=std::max(peakBytes,size());if(failPlanRename&&from=="/published.json.tmp")return false;auto it=files.find(from);if(it==files.end())return false;files[to]=it->second;files.erase(it);return true;}
};
inline MockFS LittleFS;

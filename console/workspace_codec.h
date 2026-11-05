#pragma once
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include "score_wire.h"
#ifdef THEATRE_HOST_TEST
#include <zlib.h>
#include <vector>
#else
#include <miniz.h>
#include "workspace_inflate.h"
#endif
namespace Show {
struct WorkspaceHeader {uint32_t magic=0x315a4354,length=0,checksum=0;};
// Only the workspace is DEFLATE-compressed. HTTP and exported JSON remain unchanged.
inline bool writeWorkspace(File &file,const char *text){
 size_t size=strlen(text);if(size>2200000)return false;WorkspaceHeader h;h.length=size;h.checksum=crc32(text,size);
 if(file.write((const uint8_t*)&h,sizeof(h))!=sizeof(h))return false;
#ifdef THEATRE_HOST_TEST
 uLongf n=compressBound(size);std::vector<uint8_t> compressed(n);if(compress2(compressed.data(),&n,(const Bytef*)text,size,1)!=Z_OK)return false;return file.write(compressed.data(),n)==n;
#else
 auto *state=(tdefl_compressor*)heap_caps_malloc(sizeof(tdefl_compressor),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!state)return false;
 auto put=[](const void *data,int n,void *user)->mz_bool{return ((File*)user)->write((const uint8_t*)data,n)==(size_t)n;};
 bool ok=tdefl_init(state,put,&file,TDEFL_WRITE_ZLIB_HEADER|8)==TDEFL_STATUS_OKAY&&tdefl_compress_buffer(state,text,size,TDEFL_FINISH)==TDEFL_STATUS_DONE;heap_caps_free(state);return ok;
#endif
}
inline String readWorkspaceFile(const char *path){
 auto file=LittleFS.open(path,"r");if(!file)return String();WorkspaceHeader h;
 if(file.read((uint8_t*)&h,sizeof(h))!=sizeof(h)||h.magic!=0x315a4354){file.close();auto plain=LittleFS.open(path,"r");return plain?plain.readString():String();}
 if(!h.length||h.length>2200000||file.size()<=sizeof(h)||file.size()>2400000)return String();
 size_t n=file.size()-sizeof(h);auto *input=(uint8_t*)heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);auto *output=(char*)heap_caps_malloc(h.length+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!input||!output){heap_caps_free(input);heap_caps_free(output);return String();}
 bool ok=file.read(input,n)==n;file.close();
#ifdef THEATRE_HOST_TEST
 uLongf actual=h.length;ok=ok&&uncompress((Bytef*)output,&actual,input,n)==Z_OK&&actual==h.length;
#else
 ok=ok&&inflateWorkspace(output,h.length,input,n);
#endif
 ok=ok&&crc32(output,h.length)==h.checksum;output[h.length]=0;String result=ok?String(output):String();heap_caps_free(input);heap_caps_free(output);return result;
}
}

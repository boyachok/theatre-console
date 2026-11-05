#pragma once
#include <cstdlib>
#include <cassert>
#include <map>
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline bool mockFailAllocation=false;
inline std::map<void*,size_t> mockHeap;
inline void *heap_caps_malloc(size_t n,unsigned flags){
 assert(flags==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(mockFailAllocation)return nullptr;
 void *p=std::malloc(n);if(p)mockHeap[p]=n;return p;
}
inline void heap_caps_free(void *p){if(p){assert(mockHeap.erase(p)==1);std::free(p);}}

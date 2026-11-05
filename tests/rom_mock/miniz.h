#pragma once
#include <zlib.h>
#include <cassert>
#include "esp_heap_caps.h"
using mz_uint8=unsigned char;
// Model ROM's large state and low-level API; zlib implements the byte codec here.
// This is an API/allocation regression test, NOT execution of ESP ROM on the host.
struct tinfl_decompressor {unsigned m_state;unsigned char tables[11024];};
constexpr unsigned TINFL_FLAG_PARSE_ZLIB_HEADER=1,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF=4;
enum tinfl_status {TINFL_STATUS_DONE=0,TINFL_STATUS_FAILED=-1};
inline unsigned mockInflates=0;
inline void tinfl_init(tinfl_decompressor *p){p->m_state=0;}
inline tinfl_status tinfl_decompress(tinfl_decompressor *state,const mz_uint8 *input,size_t *inSize,
 mz_uint8 *start,mz_uint8 *next,size_t *outSize,unsigned flags){
 assert(mockHeap.count(state)&&mockHeap.at(state)==sizeof(*state)); // Never stack-owned.
 assert(state->m_state==0&&start==next);
 assert(flags==(TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF));
 mockInflates++;uLong consumed=*inSize;uLongf produced=*outSize;
 int result=uncompress2(start,&produced,input,&consumed);*inSize=consumed;*outSize=produced;
 return result==Z_OK?TINFL_STATUS_DONE:TINFL_STATUS_FAILED;
}
inline size_t tinfl_decompress_mem_to_mem(void*,size_t,const void*,size_t,int){assert(false&&"Stack-heavy ESP ROM wrapper must never be called");return (size_t)-1;}

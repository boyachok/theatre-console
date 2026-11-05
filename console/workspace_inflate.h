#pragma once
#include <esp_heap_caps.h>
#include <miniz.h>
namespace Show {
// ESP ROM's tinfl_decompress_mem_to_mem() puts ~11KB of Huffman state on
// the stack. Arduino loopTask is only 8KB: loading a saved score boot-loops.
// Use the low-level coroutine with its state explicitly allocated in PSRAM.
inline bool inflateWorkspace(void *output,size_t expected,const void *input,size_t length){
 if(!output||!input||!expected||!length)return false;
 auto *state=(tinfl_decompressor*)heap_caps_malloc(sizeof(tinfl_decompressor),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!state)return false;
 tinfl_init(state);size_t consumed=length,produced=expected;
 auto *out=(mz_uint8*)output;
 auto status=tinfl_decompress(state,(const mz_uint8*)input,&consumed,out,out,&produced,
   TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
 bool ok=status==TINFL_STATUS_DONE&&consumed==length&&produced==expected;
 heap_caps_free(state);return ok;
}
}

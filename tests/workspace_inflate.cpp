#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "../console/workspace_inflate.h"
int main(){
 static_assert(sizeof(tinfl_decompressor)>8192,"Model the ROM state that exceeds loopTask's stack");
 std::string original;for(int i=0;i<18000;i++)original+="{\"scene\":123,\"fade\":0.2}";
 uLongf size=compressBound(original.size());std::vector<unsigned char> zipped(size);
 assert(compress2(zipped.data(),&size,(const Bytef*)original.data(),original.size(),1)==Z_OK);zipped.resize(size);
 std::vector<unsigned char> output(original.size()+1);
 assert(Show::inflateWorkspace(output.data(),original.size(),zipped.data(),zipped.size()));
 assert(std::string((const char*)output.data(),original.size())==original&&mockHeap.empty());
 assert(!Show::inflateWorkspace(output.data(),original.size()-1,zipped.data(),zipped.size())&&mockHeap.empty());
 assert(!Show::inflateWorkspace(output.data(),original.size()+1,zipped.data(),zipped.size())&&mockHeap.empty());
 assert(!Show::inflateWorkspace(output.data(),original.size(),zipped.data(),zipped.size()-1)&&mockHeap.empty());
 zipped.back()^=1;assert(!Show::inflateWorkspace(output.data(),original.size(),zipped.data(),zipped.size())&&mockHeap.empty());zipped.back()^=1;
 zipped.push_back(42);assert(!Show::inflateWorkspace(output.data(),original.size(),zipped.data(),zipped.size())&&mockHeap.empty());zipped.pop_back();
 unsigned before=mockInflates;mockFailAllocation=true;
 assert(!Show::inflateWorkspace(output.data(),original.size(),zipped.data(),zipped.size())&&mockInflates==before&&mockHeap.empty());mockFailAllocation=false;
 assert(!Show::inflateWorkspace(nullptr,original.size(),zipped.data(),zipped.size()));
 assert(!Show::inflateWorkspace(output.data(),0,zipped.data(),zipped.size()));
 puts("Workspace ROM API regression: >8KB state allocated/freed in PSRAM, no high-level stack wrapper; exact lengths, corruption/truncation/trailing input and allocation failure passed (ROM mocked with zlib)");
}

#pragma once
#include <vector>
struct rmt_data_t {uint32_t duration0:15,level0:1,duration1:15,level1:1;};
enum{RMT_TX_MODE,RMT_MEM_NUM_BLOCKS_2};
inline bool mockRmtBusy=false;inline std::vector<rmt_data_t> mockRmtFrame;inline int mockRmtWrites=0;
inline bool rmtInit(int,int,int,uint32_t){return true;}
inline bool rmtTransmitCompleted(int){return !mockRmtBusy;}
inline bool rmtWriteAsync(int,rmt_data_t *p,size_t n){if(mockRmtBusy)return false;mockRmtFrame.assign(p,p+n);mockRmtWrites++;return true;}

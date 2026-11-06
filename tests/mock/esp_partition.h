#pragma once
#include <cstring>
#include <array>
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_ANY=255,ESP_OK=0;
struct esp_partition_t {size_t size=512;uint32_t address=0x10000;char label[17]="app0";};
inline esp_partition_t mockPartition;
inline bool mockPartitionPresent=true,mockPartitionReadOK=true;
inline std::array<uint8_t,512> mockPartitionData=[](){std::array<uint8_t,512> a{};a.fill(255);return a;}();
inline const esp_partition_t *esp_partition_find_first(int,int,const char*){return mockPartitionPresent?&mockPartition:nullptr;}
inline int esp_partition_read(const esp_partition_t*,size_t offset,void *out,size_t n){if(!mockPartitionReadOK||offset+n>mockPartitionData.size())return -1;memcpy(out,mockPartitionData.data()+offset,n);return ESP_OK;}

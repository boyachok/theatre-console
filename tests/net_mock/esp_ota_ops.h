#pragma once
#include "../mock/esp_partition.h"
inline const esp_partition_t *esp_ota_get_running_partition(){return &mockPartition;}
inline esp_partition_t mockNextPartition{0x140000,0x150000,"app1"};
inline bool mockNextPartitionPresent=true;
inline const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t*){return mockNextPartitionPresent?&mockNextPartition:nullptr;}

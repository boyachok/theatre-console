#pragma once
#include "../mock/esp_partition.h"
inline const esp_partition_t *esp_ota_get_running_partition(){return &mockPartition;}

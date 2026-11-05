#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_partition.h>
// Never format nonempty damaged storage, including scores left by another role.
inline bool mountShowFS(){
 if(LittleFS.begin(false,"/littlefs",8,"storage"))return true;
 const auto *partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"storage");if(!partition)return false;
 uint8_t block[512];for(size_t at=0;at<partition->size;at+=sizeof(block)){if(esp_partition_read(partition,at,block,sizeof(block))!=ESP_OK)return false;for(auto byte:block)if(byte!=255){Serial.printf("Storage is not blank: refusing automatic erase\n");return false;}}
 return LittleFS.format()&&LittleFS.begin(false,"/littlefs",8,"storage");
}

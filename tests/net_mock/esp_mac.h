#pragma once
#include "WiFi.h"
constexpr int ESP_MAC_WIFI_STA=0;
inline int esp_read_mac(uint8_t *out,int){std::copy(WiFi.ownMac,WiFi.ownMac+6,out);return 0;}

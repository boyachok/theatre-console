#pragma once
// Local WLAN settings are private and excluded from Git.
#if __has_include("network_local.h")
#include "network_local.h"
#endif
#ifndef THEATRE_WIFI_SSID
#define THEATRE_WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef THEATRE_WIFI_PASSWORD
#define THEATRE_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif
constexpr char STAGE_WIFI_SSID[]=THEATRE_WIFI_SSID;
constexpr char STAGE_WIFI_PASSWORD[]=THEATRE_WIFI_PASSWORD;

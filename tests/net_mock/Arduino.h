#pragma once
#include "../mock/Arduino.h"
#include <string>
#include <cstdio>
class String {
  std::string text;
public:
  String()=default;String(const char *s):text(s){}String(std::string s):text(std::move(s)){}
  const char *c_str()const{return text.c_str();}
  friend String operator+(const String &s,const char *r){return String(s.text+r);}
};
struct FakeUSB {void begin(int){}int available(){return 0;}int read(){return -1;}template<class... T>void printf(const char*,T...) {}};
inline uint32_t esp_random(){static uint32_t id=123;return ++id;}
inline FakeUSB Serial;

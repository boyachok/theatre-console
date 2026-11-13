#pragma once
#include "../mock/Arduino.h"
#include <string>
#include <cstdio>
#include <type_traits>
class String {
  std::string text;
public:
  String()=default;String(const char *s):text(s){}String(std::string s):text(std::move(s)){}
  template<class T,std::enable_if_t<std::is_arithmetic_v<T>,int> =0>String(T n):text(std::to_string(n)){}
  String(double n,int precision){char b[96];snprintf(b,sizeof(b),"%.*f",precision,n);text=b;}
  const char *c_str()const{return text.c_str();}
  String &operator+=(const String &r){text+=r.text;return *this;}
  friend String operator+(const String &s,const char *r){return String(s.text+r);}
  friend String operator+(const String &s,const String &r){return String(s.text+r.text);}
  friend String operator+(const char *s,const String &r){return String(std::string(s)+r.text);}
};
struct FakeUSB {void begin(int){}int available(){return 0;}int read(){return -1;}template<class... T>void printf(const char*,T...) {}template<class T>void println(const T&) {}};
inline uint32_t esp_random(){static uint32_t id=123;return ++id;}
inline FakeUSB Serial;
inline unsigned mockRestartCalls=0;
struct FakeESP {void restart(){mockRestartCalls++;}};
inline FakeESP ESP;

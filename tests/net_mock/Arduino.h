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
struct FakeUSB {template<class... T>void printf(const char*,T...) {}};
inline FakeUSB Serial;

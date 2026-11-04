#pragma once
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <deque>
#include <vector>
constexpr int HIGH=1,LOW=0,INPUT=0,OUTPUT=1,SERIAL_8N1=0;
inline uint32_t mockNow=0;
inline int mockLevels[49]{},mockModes[49]{};
inline uint32_t mockPWM[49]{},mockPWMFrequency[49]{};
inline uint8_t mockPWMResolution[49]{};
constexpr int LEDC_USE_XTAL_CLK=1;
inline int mockLEDCClock=0;
inline bool mockClockOK=true;
inline bool ledcSetClockSource(int source){mockLEDCClock=source;return mockClockOK;}
inline uint32_t ledcReadFreq(uint8_t pin){return mockPWMFrequency[pin];}
inline bool mockAttachOK=true;
inline size_t mockWriteLimit=1000;
inline std::deque<uint8_t> mockUARTIn;
inline std::vector<uint8_t> mockUARTOut;
inline uint32_t millis(){return mockNow;}
inline void digitalWrite(uint8_t p,uint8_t value){mockLevels[p]=value;}
inline void pinMode(uint8_t p,uint8_t value){mockModes[p]=value;}
inline int digitalRead(uint8_t p){return mockLevels[p];}
inline bool ledcAttach(uint8_t pin,uint32_t hz,uint8_t bits){if(mockAttachOK){mockPWMFrequency[pin]=hz;mockPWMResolution[pin]=bits;}return mockAttachOK;}
inline bool ledcWrite(uint8_t pin,uint32_t value){mockPWM[pin]=value;return mockAttachOK;}
class HardwareSerial {
public:
  explicit HardwareSerial(int){}
  void begin(int,int,int,int){}
  int available(){return mockUARTIn.size();}
  int read(){int c=mockUARTIn.front();mockUARTIn.pop_front();return c;}
  size_t write(const uint8_t *p,size_t n){n=std::min(n,mockWriteLimit);mockUARTOut.insert(mockUARTOut.end(),p,p+n);return n;}
};

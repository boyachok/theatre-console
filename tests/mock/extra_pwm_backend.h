#pragma once
// Explicit host simulation; the production implementation is IDF MCPWM.
class ExtraPwm {
 uint8_t pins[4]{};bool ready=false;
public:
 bool begin(const uint8_t *p){for(int i=0;i<4;i++){pins[i]=p[i];mockPWM[p[i]]=0;mockPWMFrequency[p[i]]=25000;}ready=true;return true;}
 bool write(uint8_t i,uint8_t v){if(!ready||i>=4)return false;mockPWM[pins[i]]=v;return true;}
};

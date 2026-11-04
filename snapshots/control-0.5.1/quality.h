#pragma once
#include <stdint.h>
enum Health : uint8_t { WARMUP, EXCELLENT, GOOD, MARGINAL, BAD, OFFLINE };
// Window score, not a delivery guarantee. RSSI is only additional margin evidence.
inline Health evaluateHealth(int n, int replies, int deadline, float p95,
                             int worstRssi, uint32_t ageMs, uint32_t streak) {
  if(ageMs>3000)return OFFLINE;
  if(n<20)return WARMUP;
  if(replies==n&&deadline==n&&p95<=30&&worstRssi>=-70)return EXCELLENT;
  if(replies*100>=n*99&&deadline*100>=n*99&&p95<=100&&worstRssi>=-80)return GOOD;
  if(replies&&replies*100>=n*95&&p95<=250&&streak<=2)return MARGINAL;
  return BAD;
}

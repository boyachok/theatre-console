#include <cassert>
#include <cstdio>
#include "../radio/quality.h"
int main() {
  assert(evaluateHealth(0,0,0,0,0,0,0)==WARMUP);
  assert(evaluateHealth(19,19,19,1,-40,1,0)==WARMUP);
  assert(evaluateHealth(120,120,120,30,-70,1,0)==EXCELLENT);
  assert(evaluateHealth(120,120,120,30.1f,-70,1,0)==GOOD);
  assert(evaluateHealth(120,120,120,10,-71,1,0)==GOOD);
  assert(evaluateHealth(120,119,119,10,-70,1,0)==GOOD);
  assert(evaluateHealth(120,118,118,10,-70,1,0)==MARGINAL);
  assert(evaluateHealth(120,120,119,80,-80,1,0)==GOOD);
  assert(evaluateHealth(120,120,120,10,-81,1,0)==MARGINAL);
  assert(evaluateHealth(120,114,114,250,-80,1,2)==MARGINAL);
  assert(evaluateHealth(120,113,113,250,-80,1,0)==BAD);
  assert(evaluateHealth(120,114,114,251,-80,1,0)==BAD);
  assert(evaluateHealth(120,114,114,10,-80,1,3)==BAD);
  assert(evaluateHealth(120,0,0,0,0,1,10)==BAD);
  assert(evaluateHealth(120,120,120,1,-40,3001,0)==OFFLINE);
  assert(evaluateHealth(0,0,0,0,0,3001,0)==OFFLINE);
  puts("16 link-quality boundary checks passed");
}

#pragma once
#include <stdint.h>
// Additional report, not part of the existing 64-byte v3 control packet.
struct __attribute__((packed)) UartStats {
  uint32_t txFrames,txBytes,rxBytes,validFrames,replies,checksumErrors,lengthErrors,frameTimeouts;
  uint16_t txAge,rxAge,replyAge;
  uint8_t txLen,rxLen,lastTx[8],lastRx[16];
  uint8_t txPin,rxPin,rxLevel,polling;
  uint32_t loopbacks,txFailures;
};
static_assert(sizeof(UartStats)==76,"UART stats wire size");

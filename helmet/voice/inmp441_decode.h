#pragma once
#include <stdint.h>

// INMP441: signed 24-bit data in the most significant bits of a 32-bit slot.
// Ignore the last eight high-impedance/padding bits.
constexpr int32_t decodeInmp441(uint32_t slot) {
  return (slot & 0x80000000u) ? static_cast<int32_t>((slot >> 8) | 0xff000000u)
                            : static_cast<int32_t>(slot >> 8);
}
constexpr float normalizeInmp441(uint32_t slot) {
  return static_cast<float>(decodeInmp441(slot)) / 8388608.0f;
}
static_assert(decodeInmp441(0x00000000u) == 0, "digital silence");
static_assert(decodeInmp441(0x7fffff00u) == 8388607, "positive full scale");
static_assert(decodeInmp441(0x80000000u) == -8388608, "negative full scale");
static_assert(decodeInmp441(0xffffff00u) == -1, "sign extension");
static_assert(decodeInmp441(0x123456ffu) == 0x123456, "ignore unused low byte");
static_assert(normalizeInmp441(0x40000000u) == .5f, "positive normalization");
static_assert(normalizeInmp441(0xc0000000u) == -.5f, "negative normalization");

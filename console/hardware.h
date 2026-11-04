#pragma once
// ESP32-S3 N16R8 bench pin map. See docs/HARDWARE.md before connecting loads.
#include "control_types.h"
constexpr uint8_t PWM_PINS[PWM_COUNT]={4,5,6,7,9,10,11,12};
static_assert(PWM_COUNT==8,"ESP32-S3 provides eight LEDC channels");
constexpr uint8_t RELAY_PIN=8;
constexpr uint8_t PLAYER_TX=17; // ESP TX -> XY-V17B pin 2, IO1/RX
constexpr uint8_t PLAYER_RX=18; // ESP RX <- XY-V17B pin 1, IO0/TX (3.3 V ONLY)
constexpr uint8_t RGB_PIN=48;
constexpr uint32_t PWM_HZ=25000; // 40 MHz XTAL / (6.25 * 256), hardware LEDC
constexpr uint8_t PWM_BITS=8;
static_assert(PWM_HZ*(1UL<<PWM_BITS)<=40000000UL,"PWM frequency/resolution exceeds the 40 MHz XTAL clock");
// Relay polarity: 0=not configured (GPIO input), 1=active HIGH, 2=active LOW.
// No relay PWM. External hardware pull resistor is mandatory for a defined boot state.

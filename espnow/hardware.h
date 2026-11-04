#pragma once
// ESP32-S3 N16R8 bench pin map. See docs/HARDWARE.md before connecting loads.
constexpr uint8_t PWM_PINS[]={4,5,6,7};
constexpr uint8_t RELAY_PIN=8;
constexpr uint8_t PLAYER_TX=17; // ESP TX -> XY-V17B pin 2, IO1/RX
constexpr uint8_t PLAYER_RX=18; // ESP RX <- XY-V17B pin 1, IO0/TX (3.3 V ONLY)
constexpr uint8_t RGB_PIN=48;
constexpr uint32_t PWM_HZ=1000;
constexpr uint8_t PWM_BITS=8;
// Relay polarity: 0=not configured (GPIO input), 1=active HIGH, 2=active LOW.
// No relay PWM. External hardware pull resistor is mandatory for a defined boot state.

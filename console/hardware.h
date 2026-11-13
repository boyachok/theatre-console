#pragma once
// ESP32-S3 N16R8 bench pin map. Current wiring: docs/PINOUT.md.
#include "control_types.h"
constexpr uint8_t DIMMER_COUNT=12;
constexpr uint8_t PWM_PINS[DIMMER_COUNT]={5,6,7,15,16,17,18,8,9,10,11,12};
constexpr uint8_t STRIP_PIN=47; // WS2811 DATA through a suitable 3.3V -> 5V buffer
constexpr uint8_t RELAY2_PIN=38;
constexpr uint8_t BUTTON_FORWARD_PIN=1,BUTTON_BACK_PIN=2;
static_assert(PWM_COUNT==8,"ESP32-S3 provides eight LEDC channels");
constexpr uint8_t RELAY_PIN=39;
constexpr uint8_t PLAYER_TX=2; // ESP TX -> YX6300 module RX (not the board's COM TX label)
constexpr uint8_t PLAYER_RX=1; // ESP RX <- YX6300 module TX, 3.3 V ONLY
constexpr uint8_t RGB_PIN=48;
constexpr uint8_t AC_ZC_PIN=41,AC_GATE_PIN=40; // Isolated 3.3V Z-C input; active-HIGH optotriac gate, external pulldown required.
constexpr bool validSlavePinMap(){
 const uint8_t fixed[]={RELAY_PIN,RELAY2_PIN,PLAYER_TX,PLAYER_RX,STRIP_PIN,RGB_PIN,AC_ZC_PIN,AC_GATE_PIN};
 for(unsigned i=0;i<DIMMER_COUNT;i++){for(unsigned j=0;j<i;j++)if(PWM_PINS[i]==PWM_PINS[j])return false;for(auto p:fixed)if(PWM_PINS[i]==p)return false;}
 for(unsigned i=0;i<sizeof(fixed);i++)for(unsigned j=0;j<i;j++)if(fixed[i]==fixed[j])return false;
 return true;
}
static_assert(validSlavePinMap(),"Slave PWM/relay/UART/strip/status GPIO overlap");
#include "pwm_curve.h"
constexpr uint32_t PWM_HZ=THEATRE_PWM_HZ; // Default25kHz; lower rates are explicit bench builds only.
constexpr uint8_t PWM_BITS=THEATRE_PWM_BITS;
static_assert(PWM_HZ*(1UL<<PWM_BITS)<=40000000UL,"PWM frequency/resolution exceeds the 40 MHz XTAL clock");
// Relay polarity: 0=not configured (GPIO input), 1=active HIGH, 2=active LOW.
// No relay PWM. External hardware pull resistor is mandatory for a defined boot state.

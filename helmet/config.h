#pragma once
// Change only this pin when the future audio codec/I2S wiring is agreed.
constexpr uint8_t HELMET_STRIP_PIN=47;
constexpr uint8_t HELMET_STATUS_PIN=48;
static_assert(HELMET_STRIP_PIN!=HELMET_STATUS_PIN,"Strip and status LED must use different GPIO");

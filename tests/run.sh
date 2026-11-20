#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="$(mktemp -d /tmp/stage-tests.XXXXXX)"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=c++17 -Wall -Wextra -pedantic -fsanitize=address,undefined)
python3 tests/helmet_usb_layout.py
for TEST in quality control lan remote remote_battery ac_phase; do
  c++ "${FLAGS[@]}" "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
for TEST in actors pwm_inversion pwm_precision pwm_camera pwm_frequency firmware_info helmet_audio helmet_gate helmet_tone helmet_defaults player yx_player compat console dimmer_animation light_delay audio_delay scene_delivery master_boot remote_master pulse_diagnostics; do
  c++ "${FLAGS[@]}" -Itests/mock -Iconsole "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
c++ "${FLAGS[@]}" -DTHEATRE_PLAYER_POLL_MS=1300 -Itests/mock -Iconsole tests/pulse_diagnostics.cpp -o "$OUT/pulse_diagnostics_1300"
"$OUT/pulse_diagnostics_1300"
c++ "${FLAGS[@]}" -Itests/ac_mock -Itests/mock tests/ac_backend.cpp -o "$OUT/ac_backend"
"$OUT/ac_backend"
c++ "${FLAGS[@]}" -Itests/rom_mock tests/workspace_inflate.cpp -lz -o "$OUT/workspace_inflate"
"$OUT/workspace_inflate"
ARDUINOJSON_PATH="${ARDUINOJSON_PATH:-$HOME/Documents/Arduino/libraries/ArduinoJson/src}"
if [[ -f "$ARDUINOJSON_PATH/ArduinoJson.h" ]]; then
  c++ "${FLAGS[@]}" -Itests/mock -I"$ARDUINOJSON_PATH" tests/console_storage.cpp -lz -o "$OUT/console_storage"
  "$OUT/console_storage"
  c++ "${FLAGS[@]}" -Itests/mock -I"$ARDUINOJSON_PATH" tests/helmet_sound_store.cpp -lz -o "$OUT/helmet_sound_store"
  "$OUT/helmet_sound_store"
else
  echo 'Master storage test skipped: set ARDUINOJSON_PATH to ArduinoJson/src.'
fi
for TEST in http console_http; do
  c++ "${FLAGS[@]}" -pthread -Itests/http_mock "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
for TEST in wifi remote_firmware remote_ota remote_buttons remote_battery_firmware helmet_firmware helmet_global_voice helmet_ota; do
  c++ "${FLAGS[@]}" -Itests/net_mock -Itests/mock -Iconsole "tests/$TEST.cpp" -o "$OUT/$TEST"
  if [[ "$TEST" == remote_battery_firmware ]]; then BATTERY_HTML_DUMP="$OUT/remote-battery.html" "$OUT/$TEST";else "$OUT/$TEST";fi
done
if [[ -n "${PLAYWRIGHT_PATH:-}" ]]; then
  node tests/ui.cjs
  node tests/yx_uart_ui.cjs
  BATTERY_HTML_DUMP="$OUT/remote-battery.html" node tests/remote_battery_ui.mjs
else
  echo 'UI test skipped: set PLAYWRIGHT_PATH to an installed playwright package.'
fi

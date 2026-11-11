#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="$(mktemp -d /tmp/stage-tests.XXXXXX)"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=c++17 -Wall -Wextra -pedantic -fsanitize=address,undefined)
for TEST in quality control lan remote ac_phase; do
  c++ "${FLAGS[@]}" "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
for TEST in actors pwm_inversion firmware_info player yx_player compat console dimmer_animation light_delay audio_delay scene_delivery remote_master; do
  c++ "${FLAGS[@]}" -Itests/mock "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
c++ "${FLAGS[@]}" -Itests/ac_mock -Itests/mock tests/ac_backend.cpp -o "$OUT/ac_backend"
"$OUT/ac_backend"
c++ "${FLAGS[@]}" -Itests/rom_mock tests/workspace_inflate.cpp -lz -o "$OUT/workspace_inflate"
"$OUT/workspace_inflate"
ARDUINOJSON_PATH="${ARDUINOJSON_PATH:-$HOME/Documents/Arduino/libraries/ArduinoJson/src}"
if [[ -f "$ARDUINOJSON_PATH/ArduinoJson.h" ]]; then
  c++ "${FLAGS[@]}" -Itests/mock -I"$ARDUINOJSON_PATH" tests/console_storage.cpp -lz -o "$OUT/console_storage"
  "$OUT/console_storage"
else
  echo 'Master storage test skipped: set ARDUINOJSON_PATH to ArduinoJson/src.'
fi
for TEST in http console_http; do
  c++ "${FLAGS[@]}" -pthread -Itests/http_mock "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
for TEST in wifi remote_firmware remote_ota remote_buttons helmet_firmware; do
  c++ "${FLAGS[@]}" -Itests/net_mock -Itests/mock -Iconsole "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
if [[ -n "${PLAYWRIGHT_PATH:-}" ]]; then
  node tests/ui.cjs
  node tests/yx_uart_ui.cjs
else
  echo 'UI test skipped: set PLAYWRIGHT_PATH to an installed playwright package.'
fi

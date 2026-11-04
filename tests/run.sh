#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="$(mktemp -d /tmp/stage-tests.XXXXXX)"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=c++17 -Wall -Wextra -pedantic -fsanitize=address,undefined)
for TEST in quality control lan remote; do
  c++ "${FLAGS[@]}" "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
for TEST in actors player compat console remote_master; do
  c++ "${FLAGS[@]}" -Itests/mock "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
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
for TEST in wifi remote_firmware; do
  c++ "${FLAGS[@]}" -Itests/net_mock -Iconsole "tests/$TEST.cpp" -o "$OUT/$TEST"
  "$OUT/$TEST"
done
if [[ -n "${PLAYWRIGHT_PATH:-}" ]]; then
  node tests/ui.cjs
else
  echo 'UI test skipped: set PLAYWRIGHT_PATH to an installed playwright package.'
fi

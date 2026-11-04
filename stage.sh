#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,FlashMode=dio,PSRAM=opi,PartitionScheme=custom,UploadSpeed=460800'
case "${1:-build}" in
  build) "$CLI" compile --fqbn "$FQBN" --output-dir "$PWD/build" firmware ;;
  usb)
    # USB-JTAG RTS reset can leave this board in download mode; watchdog boots it reliably.
    ESPROOT="$HOME/Library/Arduino15/packages/esp32"
    "$ESPROOT/tools/esptool_py/5.3.1/esptool" --chip esp32s3 \
      --port "${2:-/dev/cu.usbmodem1101}" --baud 115200 \
      --before usb-reset --after watchdog-reset write-flash \
      --flash-mode dio --flash-freq 80m --flash-size 16MB \
      0x0 build/firmware.ino.bootloader.bin \
      0x8000 build/firmware.ino.partitions.bin \
      0xe000 "$ESPROOT/hardware/esp32/3.3.11/tools/partitions/boot_app0.bin" \
      0x10000 build/firmware.ino.bin
    ;;
  monitor) "$CLI" monitor --port "${2:-/dev/cu.usbmodem1101}" --config baudrate=115200 ;;
  ota)
    : "${2:?Usage: bash stage.sh ota IP}"
    # Bench build: OTA has no authentication. Trusted isolated network only.
    curl --fail -F "firmware=@build/firmware.ino.bin" "http://$2/update"
    ;;
  *) echo 'Usage: bash stage.sh {build|usb [PORT]|monitor [PORT]|ota IP}'; exit 1 ;;
esac

# Проверенные записи на платы

## Remote 0.7.2 — MAC 30:30:F9:1B:FD:6C

По явной команде пользователя `deploy remote` записан отдельный пульт.

- Перед записью прочитана MAC чипа, подтверждены ESP32-S3 rev0.2, flash16MB,
  встроенная OPI PSRAM8MB, XTAL40MHz. Это не известный мастер A0:85:E3:E2:BC:D4.
- Потребовался ручной BOOT+RESET: старая программа после сброса меняла USB на
  VID:PID303A:4001/serial123456 и не входила в download автоматически.
- В bootloader устройство — VID:PID303A:1001, serial30:30:F9:1B:FD:6C,
  `/dev/cu.usbmodem11101`. Порт не является постоянной идентичностью.
- Команда: `EXPECTED_MAC=30:30:F9:1B:FD:6C ./deploy remote /dev/cu.usbmodem11101`.
- Записаны bootloader0x0, partitions0x8000, boot_app0/otadata0xe000 и приложение0x10000.
  DIO/16MB. Для всех записанных частей esptool подтвердил hash. Полного erase-flash не было.
- Приложение: `build/console-remote-wifi/remote.ino.bin`,916448B,
  SHA256 `ad83885bb348121d8056acb010607a8b25290b426c4417ecb3312196fe9cc80a`.
- После watchdog-reset USB вновь определён по той же MAC; Serial115200 (`i`/`j`)
  подтвердил **role=remote, version=0.7.2**.

Фактический ответ после перезапуска:

```json
{"role":"remote","version":"0.7.2","name":"remote","mac":"30:30:F9:1B:FD:6C","ip":"192.168.50.4","hostname":"stage-remote-1bfd6c","wifi":true,"masterReady":false,"rssi":-65,"pending":0,"sent":0,"acks":0,"lastAck":0,"lastResult":1,"step":0}
```

Wi-Fi подключён, но согласование с master в момент проверки **не подтверждено**.
Точная причина по одному этому ответу не устанавливается. Проверить/обновить master
до0.7.2 и затем проверить обнаружение remote, ping, кнопки и белую вспышку на стенде.
Тестовых нажатий/команд сцен инструментами не отправлялось. Другие платы в этой
операции не записывались. IP/RSSI — моментальный снимок, не постоянные значения.

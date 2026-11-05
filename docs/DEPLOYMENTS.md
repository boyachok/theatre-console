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

## Master 0.7.2 — MAC A0:85:E3:E2:BC:D4

По следующей явной команде `deploy master` записан мастер. До записи аппаратно
сверены MAC, ESP32-S3 rev0.2,16MB flash,8MB PSRAM,XTAL40MHz. Порт `/dev/cu.usbmodem11101`
теперь принадлежал мастеру, а не ранее записанному remote: MAC проверена повторно.

- Команда: `EXPECTED_MAC=A0:85:E3:E2:BC:D4 ./deploy master /dev/cu.usbmodem11101`.
- DIO/16MB; полный набор bootloader/partitions/boot_app0/app по прежним адресам.
  Esptool подтвердил hash каждой записанной части; NVS-раздел не стирался.
- Приложение: `build/console-master-wifi/console.ino.bin`,1227488B,
  SHA256 `5fce0c59a77aad70d8f2c5fff3126b324bc6209ba922208c2779371ee3fad2ea`.
- После watchdog-reset Serial115200 подтвердил **role=master,version=0.7.2**, ту же
  MAC, подключение к `YOUR_WIFI_SSID`, IP **192.168.50.119**, hostname `stage-master-e2bcd4`.
- На мастере пока нет опубликованной партитуры: `console.version=0`, `step=null`.
  Управляющих команд сцен инструментами не отправлялось; другие платы не прошивались.

В первом ответе remote ещё отсутствовал, но в повторном реальном Serial-статусе
master обнаружил его и подтвердил обмен PING/PONG:

```json
{"role":"remote","name":"remote","mac":"30:30:F9:1B:FD:6C","ip":"192.168.50.4","ageMs":191,"sent":121,"received":121,"timeouts":0,"p50":11.52,"p95":28.19,"rssiSlave":-35,"worstRssi":-64,"health":"excellent"}
```

Это измерение живой связи двух плат, не mock.121/121 и p95 относятся к диагностическим
пакетам/окну, не доказывают сквозное время кнопка→нагрузка или будущее отсутствие потерь.
Физические кнопки, белая вспышка и работа выходов в этой операции не проверялись.

HTTP `/` и `/api/console/status` с Mac проверялись **без proxy**, но завершились
таймаутом: en0 Mac имел **192.168.0.104**, маршрут к мастеру шёл через default gateway
192.168.0.1, тогда как ESP находятся в192.168.50.x. Доступность веба с этого Mac пока
не подтверждена; подключить браузер к LAN/SSID `YOUR_WIFI_SSID` и открыть
`http://192.168.50.119/`. Сам master продолжал отвечать по USB и обмениваться с remote.

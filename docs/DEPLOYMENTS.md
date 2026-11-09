# Проверенные записи на платы

**Текущий комплект:master и7slave (DC,31:19:C4,84,C0,32:02:30,EC,74) на0.7.11;
30:30:F9:17:8B:14 на0.7.9;remote0.7.8.** Последняя запись — «OTA видимого74-slave».
Он обновлён с0.7.10, две другие свежие32/EC уже0.7.11 не трогали. LIVE ужеoff,
workspace2шага/5приборов/v104 совпал JSON/SHA. GO/STOP/перепривязок не было.
Helmet не устанавливался; историческая MAC F0:9E:9E:16:18:A4 не найдена и запись не подтверждена.

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

## Два slave 0.7.2 — EC:DA:3B:56:F2:CC и 30:30:F9:31:19:C4

По явной команде пользователя `deploy two slaves` последовательно записаны две
подключённые платы. До записи для каждой прочитаны аппаратная MAC и flash-id:
ESP32-S3 rev0.2,16MB flash,8MB PSRAM,XTAL40MHz. MAC не совпадают с master/remote.

| MAC | USB во время записи | IP после запуска | Роль / версия |
|---|---|---|---|
| EC:DA:3B:56:F2:CC | /dev/cu.usbmodem1101 | 192.168.50.197 | slave / 0.7.2 |
| 30:30:F9:31:19:C4 | /dev/cu.usbmodem11101 | 192.168.50.55 | slave / 0.7.2 |

Команды содержали `EXPECTED_MAC=<MAC> ./deploy slave <USB_PORT>`. Записан полный
USB-набор DIO/16MB с прежними разделами; esptool подтвердил hash каждой части.
NVS не стиралась. Обе сборки из чистой ревизии `b9b7ac0`, код/общая версия не менялись.
Каждая последовательная команда пересобирает приложение; размер обоих1083248B.
Последняя сборка (для30:30:F9:31:19:C4), `build/console-slave-wifi/console.ino.bin`:
SHA256 `c076ef9867f097be096cc74d0c37799a49fc449fbbd201fecf88c5ded9d68f5b`.
SHA первого app.bin до второй пересборки отдельно не сохранялся; проверка записи
первой платы выполнена esptool и её реальным post-boot Serial-ответом.

Обе после watchdog-reset определены **по MAC**, Serial115200 подтвердил `role=slave`,
`version=0.7.2`, `connected=true`, SSID `YOUR_WIFI_SSID`, `control.ready=true`, `relay=false`.
Восемь PWM-значений в служебном control-снимке равны0; дополнительные4канала этот
legacy-снимок не передаёт. Включающих/тестовых команд нагрузкам инструментами не было.

### Особенность отчёта частоты на выключенном канале

Обе платы сообщили `pwmHz=0`, `control.fault=2`. В установленном Arduino ESP32core3.3.11
`cores/esp32/esp32-hal-ledc.c:408–417` функция `ledcReadFreq(pin)` намеренно возвращает0,
если `ledcRead(pin)==0`. Поэтому0 при duty0 само по себе не означает неудачную настройку
таймера. `fault=2` здесь сохраняется как причина OFF при привязке master; это не код5
ошибки PWM-инициализации. Ради частотного отчёта ненулевая яркость не выставлялась.
Физическая частота/форма импульсов всех12выходов ещё требуют отдельного теста;
не выдавать этот снимок за измерение25кГц осциллографом.

Повторная USB-команда `i` на обеих указала `master=A0:85:E3:E2:BC:D4`,
`control.ready=true`; назначенные в этом снимке номера: EC:DA… —id3,
30:30:F9:31:19:C4 —id2. RSSI−50/−53dBm, RXdrops=0/TXerrors=0 локально.
Номера/адреса изменяемы; это не измерение процента доставки всей радиолинии.
HTTP-статус самого master с Mac повторно запрашивался безproxy, но недоступен по
таймауту; master-side счётчики для этих двух slave в этой операции не получены.
Master и remote не перепрошивались. IP/USB-имена — текущие наблюдения, не идентичности.

## Следующие два slave 0.7.2 — C0:4E:30:27:69:98 и 30:30:F9:17:8B:14

По повторной явной команде `deploy two slaves` подключена и записана **другая пара**.
Аппаратные MAC, ESP32-S3 rev0.2,16MB flash/8MB PSRAM/40MHz проверены до записи.
Использованы точные USB-порты и `EXPECTED_MAC`, две последовательные сборки из
чистой ревизии `b3a4960`. Bootloader/partitions/boot_app0/app записаны DIO16MB;
esptool подтвердил hash каждой части. NVS не стиралась, код/версия не менялись.

| MAC | USB при записи | IP / имя | ID в снимке |
|---|---|---|---|
| C0:4E:30:27:69:98 | /dev/cu.usbmodem1101 | 192.168.50.33 / stage-slave-276998 | 5 |
| 30:30:F9:17:8B:14 | /dev/cu.usbmodem11101 | 192.168.50.138 / stage-slave-178b14 | 4 |

Оба app.bin1083248B, SHA256 сохранены до следующей пересборки:

- C0:4E…: `ffd4ecb76e206e056a64d830c0b3cb5cc0a79da506fe2aa3e4a9c1f5bf565814`.
- 30:30:F9:17…: `fd3f93fa91cb39c15928485e412e22a980cc8109c07bee1796194b5f2909c8b6`.

После watchdog-reset Serial115200 (`i`/`j`) по **каждой MAC** подтвердил `role=slave`,
`version=0.7.2`, `connected=true`, SSID YOUR_WIFI_SSID/channel9, `control.ready=true`,
`master=A0:85:E3:E2:BC:D4`, flash16777216/PSRAM8388608. Локальные RXdrops/TXerrors=0,
RSSI−45/−52dBm в финальном снимке. Master-side RTT/PDR в этой операции не запрашивались.

Служебный снимок: первые8PWM=0, relay=false, preset-уровни255, inversion0.
У C0:4E… relayMode=2, у30:30:F9:17… relayMode=0. Настройки не изменялись специально;
перед записью отдельный конфигурационный снимок не снимался. `pwmHz=0`/fault2 —
та же ранее описанная особенность OFF/readback. Никаких включающих или тестовых
команд нагрузкам инструментами не посылалось; выходы/плеер/лента не тестировались.
Master, remote и две ранее обновлённые платы не перепрошивались. Итого в журнале
подтверждены четыре разных slave0.7.2; их ID/IP остаются изменяемыми.

## Третья пара slave 0.7.2 — 84:FC:E6:57:0F:AC и 74:4D:BD:73:0A:A4

По следующей явной команде `deploy two slaves` записаны ещё две отличающиеся MAC.
До записи esptool независимо подтвердил MAC, ESP32-S3 rev0.2,16MB flash/8MB PSRAM,
XTAL40MHz. Две последовательные команды `EXPECTED_MAC=<MAC> ./deploy slave <USB>`
из чистой ревизии `2e8f7ee`; DIO16MB, стандартные bootloader/partitions/boot_app0/app.
Все части проверены esptool по hash; полного стирания/NVS erase не было.

| MAC | USB во время записи | IP / имя | ID в снимке |
|---|---|---|---|
| 84:FC:E6:57:0F:AC | /dev/cu.usbmodem1101 | 192.168.50.58 / stage-slave-570fac | 6 |
| 74:4D:BD:73:0A:A4 | /dev/cu.usbmodem11101 | 192.168.50.20 / stage-slave-730aa4 | 1 |

Каждый app.bin1083248B. SHA256 сохранены до перезаписи следующей сборкой:

- 84:FC…: `0f6603da6741c233924121649e9e84fe4944e5c08f238cb16598dc539f7a590d`.
- 74:4D…: `fa146c523b2041f96aba0ac93f630be409e0ab5997b4c834125aa851dc91c667`.

Реальные post-boot `i`/`j` по каждой MAC: `role=slave`, `version=0.7.2`, SSID YOUR_WIFI_SSID,
channel9, connected1, `master=A0:85:E3:E2:BC:D4`, `control.ready=true`,
flash16777216/PSRAM8388608. Финальные RSSI−52/−55dBm; локальные RXdrops/TXerrors=0.
Master-side счётчики/HTTP в этой операции не запрашивались; это не замер PDR/RTT.

Первые8PWM=0, relay=false, presets255/inversion0. Прочитанные relayMode:84:FC…=0,
74:4D…=1. Настройки специально не менялись, отдельного pre-flash снимка не было.
Служебный fault2/PWM0 на выключенном канале — как в предыдущих разделах, не
физическое измерение частоты. Команд включения нагрузок инструментами не было;
12PWM, реле, плеер и лента на реальных нагрузках в этой операции не тестировались.
Мастер, remote и четыре уже обновлённые платы не перепрошивались. Код/версия прежние.
**Итого подтверждены шесть разных slave0.7.2**. ID, IP и USB-порты не заменяют MAC.

## Четвёртая пара slave 0.7.2 — DC:DA:0C:43:C3:C0 и 30:30:F9:32:02:30

По очередной явной команде `deploy two slaves` записана новая пара; MAC30:30:F9:32:02:30
ранее не была в историческом списке. Обе MAC независимо прочитаны esptool до записи:
ESP32-S3 rev0.2,16MB flash,8MB PSRAM,40MHz. Исходники — чистая ревизия `bc0981f`.
Последовательные `EXPECTED_MAC=<MAC> ./deploy slave <USB>` записали штатный USB-набор
DIO16MB, esptool подтвердил hash каждой части. NVS не стиралась, код/версия прежние.

| MAC | USB при записи | IP / hostname | ID в снимке |
|---|---|---|---|
| DC:DA:0C:43:C3:C0 | /dev/cu.usbmodem1101 | 192.168.50.41 / stage-slave-43c3c0 | 7 |
| 30:30:F9:32:02:30 | /dev/cu.usbmodem11101 | 192.168.50.65 / stage-slave-320230 | 8 |

Оба app.bin1083248B. SHA256 сохранены до следующей пересборки:

- DC:DA…: `d39192c9b013c0b4dfad1c5cd1e6665ea01bed2fdf9a7c136d2a1f4302ff9566`.
- 30:30:F9:32…: `4d24b360b939ead1949bdf6f61eea598fe68955f52d4cb772f5148889fa08d18`.

После reset реальные Serial115200 `i`/`j` по обеим MAC подтвердили slave0.7.2,
YOUR_WIFI_SSID/channel9/connected1, `master=A0:85:E3:E2:BC:D4`, `control.ready=true`,
flash16777216/PSRAM8388608. В финальном снимке RSSI−47/−55dBm, RXdrops/TXerrors=0
локально. Master-side RTT/PDR/HTTP в этой операции не запрашивались.

Первые8PWM=0, relay=false, relayMode0, preset255/inversion0; fault2 и `pwmHz=0`
на выключенном канале — ранее описанный readback, не физическое измерение25кГц.
Настройки специально не менялись; отдельный pre-flash конфигурационный снимок
не снимался. Включающих/тестовых команд нагрузкам не было; реальные12PWM,
реле/плеер/лента не проверялись. Остальные платы/master/remote не перепрошивались.

**Итого подтверждены восемь разных slave0.7.2**, включая новую30:30:F9:32:02:30.
Это не означает обновление всех восьми из старого инвентаря: для исторической
F0:9E:9E:16:18:A4 запись0.7.2 этим журналом пока не подтверждена. ID/IP/USB изменяемы.

## Восстановление master — 0.7.2-recovery.1, A0:85:E3:E2:BC:D4

По явной команде пользователя «ты перепрошивал его? перепрошей» записан минимальный
совместимый ремонт0.7.2, не новый score7. До записи: аппаратная MAC,
ESP32-S3 rev0.2/16MB/8MB проверены esptool; OTA-селектор прочитан и побайтно совпал
с полной16MB копией (app0). Полная flash и JSON партитуры с проверенным CRC уже
сохранены вне Git; подробности: [MASTER_RECOVERY.md](MASTER_RECOVERY.md).

- Исходники: ветка `recovery/master-072-stack`, commit494f1ee (отv0.7.2).
- Образ `build/console-master-wifi/console.ino.bin` из recovery-worktree,1227616B.
- SHA256 `ceccfe1ee25bfc5c7a0ae1067e862a618ea4700354c0301f538024b1e1d99429`.
- USB `/dev/cu.usbmodem11101`, MAC проверена перед записью повторно.
- **Только app0:0x10000**, keep flash mode/frequency/size,115200;
  esptool hash verified, watchdog-reset. Никакого erase-flash; bootloader,
  partitions/otadata/NVS/storage не входят в диапазон записи.
- Реальный Serial115200 после boot: rolemaster,version0.7.2-recovery.1,
  YOUR_WIFI_SSID,IP192.168.50.119,hoststage-master-e2bcd4,flash16777216/PSRAM8388608.
- console.version1/fingerprint503ee907/stepnull/previewfalse. Uptime двух снимков
  36.1s→128.5s, loopGapMax6.99ms. В post-flash захватах нет stack-canary/Guru/reboot.
- Никаких включающих/сценовых тестовых команд инструментами не отправлялось.
  Slave и remote не перепрошивались, score6 совместимость сохранена.
- HTTP с Mac снова timeout: Mac192.168.0.104, ESP192.168.50.x. Браузерную проверку
  делать из YOUR_WIFI_SSID; доступ к workspace по HTTP этим Mac не подтверждён.

Ремонт заменяет stack-heavy ROM inflate на state в PSRAM. Новая main0.7.5 остаётся
только подготовленной, её новые индексы/режимы не выдавать за установленный recovery.

## Remote 0.7.6 — первое добавление OTA и реальная OTA-проверка

По явному запросу «добавим ota и проверим мигание, подключаю его по юсб» обновлён
**только remote30:30:F9:1B:FD:6C**. MAC прочитана из USB и независимо esptool,
ESP32-S3 rev0.2/16MB flash/8MB PSRAM. До изменений Serial подтвердил remote0.7.2,
Wi-Fi/masterReady true,sent0/acks0; сохранён снимок counter мастера.

Полная исходная flash16MB (SHA256
`501c39d1d355857ca3af4b7dd2bcfae3bdd66e4c4aa88218a0cc4e1b4194b94d`),
образ/логи/статусы — вне Git:
`~/Theatre-console-backups/remote-3030F91BFD6C-20261106-135313/`.

1. Перед USB повторно проверены MAC и OTA selector (побайтно как backup, app0).
   Записано **только app0 по0x10000**, без bootloader/partitions/otadata/NVS/storage.
   Esptool hash verified, watchdog-reset.
2. Образ965056B, `build/console-remote-wifi/remote.ino.bin`, SHA256
   `4fec26ac0e3f2bd851fa1b64a99449f6ce5998011ea015cc0ec83a607de92ef2`.
   Boot: role remote/version0.7.6,IP192.168.50.4,app0/65536,HTTP otaSupported true,
   GPIO13/14 отпущены, masterReady/buttonsEnabled true.
3. Реальный OTA-запрос с target MAC мастера к remote получил403, app/boot/token
   не изменились, otaBytes0. Затем **тот же бинарник** загружен по HTTP multipart:
   ответ200 «Remote update OK. Rebooting.».
4. После OTA: remote0.7.6/MAC прежние, новый token/bootId, **app1/0x610000**,
   serverMs3952 в первом подтверждённом ответе, masterReady true. Это не только
   успешный ответ upload, но проверка фактического нового запуска другого slot.
5. Вызваны identify5с и безопасный tx-test (eventID0/actionNONE/challenge0).
   Master обязан отклонить его: diagSent1/diagAcks1/diagResult6(STALE),whitePulses1,
   навигационныеsent0/acks0. Обычный outbox не использовался. Это реальный UDP/ACK,
   не симуляция. Физическую видимость белого ещё подтверждает оператор; GPIO
   светодиода программно выставлен, но оптического датчика нет.
6. Counter мастера до/после: v2/fingerprint93081004/stepnull/previewfalse — без
   изменения. Никаких действительных команд навигации/включения нагрузок не было.

Мастер остаётся0.7.2-recovery.1,8slave —0.7.2; они не перепрошивались. R/1 прежний.
Реальные кнопки коротко/3с на нагрузках не тестировались. Source0.7.6 для остальных
ролей не выдавать за установленный комплект. Подробности HTTP/OTA: [REMOTE.md](REMOTE.md).

## Remote0.7.7 — проверка физических кнопок без сцен

После сообщения оператора «жму кнопки… ничего с индикацией» read-only статус0.7.6
показал свежую загрузку~124с, masterReady/buttonsEnabled true, PING/PONG241/241,
но navsent0/whitePulses0, GPIO13/14 HIGH. Это не доказывает неисправность GPIO/LED,
но не подтверждает нажатия. Подтяжка10кОм сама по себе не замыкает вход наGND.

Добавлен диагностический режим (подробно REMOTE.md), bright white-held и непрерывный
probe raw-edge/30мс press/release. Настоящие команды навигации в нём запрещены,
на отпускание возможен лишь invalid id0 diagnostic TX/STALE. Lock persistent NVS,
без ключа/при неуспешном чтении default safe commissioning; выход явный и без
held replay. Никаких произвольных GPIO для поиска LED не перебиралось: status48.

После native ASan/UBSan тестов обновлён **только remote30:30:F9:1B:FD:6C** через
`EXPECTED_MAC=… ./deploy ota remote 192.168.50.4`. Реальный script подтвердил MAC,
новую версию0.7.7 и смену token после OTA. HTTP подтвердил **app0**, masterReadytrue,
buttonTesttrue/buttonsEnabledfalse. Дополнительно enabled=1 записал lock в NVS;
перезапуск не должен сам разрешать навигацию. Первый boot default уже locked.

Образ968576B SHA256
`3304d143dbd1fa3cd88d9adbbe97a6457f7958a6bc7cb036f635f9df6a30250d`.
`remote-0.7.7.bin`, SHA, API/monitor/master snapshots в том же private backup
`~/Theatre-console-backups/remote-3030F91BFD6C-20261106-135313/`, не в Git.

Оператору предложено нажимать по очереди2–3с. За60с:339HTTP-чтений/0ошибок,
GPIO13/14 всёHIGH, rawEdges/Presses0, ни одного diagnostic или navigation event.
Это условный вывод: **если физические кнопки нажимались в том окне**, ESP не увидел
замыканий. Нажатия и видимость purple/white/yellow ещё требуют ответа оператора.
Master counter v2/fingerprint93081004/stepnull/previewfalse неизменён; master
0.7.2-recovery.1 и8slave0.7.2 не перепрошивались. **Диагностический lock оставлен
включённым** до завершения проверки/явного разрешения навигации.

## Подготовка0.7.8 — без записи плат

После исправления оператором проводки remote0.7.7 (MAC прежняя) read-only показал
GPIO13 presses5, diagnosticTX4/ACK4, navigationTX0,masterReadytrue; GPIO14 presses0.
Это подтверждает работу GPIO13/UDP диагностики, не FIRST/LAST/нового LED0.7.8.
Диагностический persistent lock **остаётся включённым**. Master counter по-прежнему
v2/fingerprint93081004/stepnull/previewfalse, установлен0.7.2-recovery.1.

Для запрошенных live-preview+remote draft navigation/auto flash sync+long ACK
подготовлена0.7.8: source/tests/build, не deployment. Согласованное обновление ещё
требует разрешения; нельзя незаметно заменить score6 master на score7 при старых
назначенных slave. В проекте2назначенные MAC: DC:DA:0C:43:C3:C0 и30:30:F9:32:02:30.
Неназначенные6slave не обновлялись; helmet также не записывался.

Свежий JSON и статусы вне Git:
`~/Theatre-console-backups/before-preview-mode-20261106-171157/`.
Score SHA256 `586d5e663e063fb2a4b391d01702695f84c131ad155858c31e793ed5531e81a7`.
JSON read-only прошёл parseBackup+compileDraft:9steps/2devices,
draft fp93081004=publicationv2. Никаких сцен/изменений workspace по API не посылалось.
[Новая логика/план установки](PREVIEW_MODE.md).

## OTA0.7.8 всего найденного комплекта — фактически установлена

Явные разрешения: «ота обновление всех… снимаем диагностическую проверку с пульта»
и дополнительно «можно и мастер и ремоут тоже прошить по ота». Все10HTTP-идентичностей
до записи проверены по MAC/role; IP использовался только как адрес транспорта.
Не было USB/erase-flash, NVS/storage не очищались. Штатный OTA master ставит ALL_OFF
в очередь, OTA slave останавливает runner: это безопасное поведение обновления,
не сценный тест GO. Новые сцены/предпросмотр тестовыми командами не запускались.

До начала сохранены свежие workspace и10status. Бинарники взяты из уже собранного
и протестированного **93d0b188500c9ac3670c70bf741a40b563787642**, их длины/SHA сверены
с private prepared manifest. Последовательно обновлены master,2назначенных slave,
6прочих slave, затем remote. Каждый POST `/update` вернул200; после каждого заново
проверены **MAC+role+version0.7.8+смена token**, то есть новый boot, не только upload.

| Роль | MAC | IP после OTA | Версия до |
|---|---|---|---|
| master | A0:85:E3:E2:BC:D4 | 192.168.50.119 | 0.7.2-recovery.1 |
| slave | DC:DA:0C:43:C3:C0 | 192.168.50.41 | 0.7.2 |
| slave | 30:30:F9:32:02:30 | 192.168.50.65 | 0.7.2 |
| slave | 74:4D:BD:73:0A:A4 | 192.168.50.20 | 0.7.2 |
| slave | C0:4E:30:27:69:98 | 192.168.50.33 | 0.7.2 |
| slave | 30:30:F9:31:19:C4 | 192.168.50.55 | 0.7.2 |
| slave | 84:FC:E6:57:0F:AC | 192.168.50.58 | 0.7.2 |
| slave | 30:30:F9:17:8B:14 | 192.168.50.138 | 0.7.2 |
| slave | EC:DA:3B:56:F2:CC | 192.168.50.197 | 0.7.2 |
| remote | 30:30:F9:1B:FD:6C | 192.168.50.4 | 0.7.7 |

Записанные приложения (без bootloader/partitions/FS images):
- master1244272B SHA256 `f344d4b1c49ed44ed37af519aa5172dc89dc42ebd1e0ae1c48e1b25bcd585522`;
- slave1085104B SHA256 `dc0e3a980e0c0bd7038e335f1d00222ca09412f3a3380e14dad615f91f69723a`;
- remote968704B SHA256 `aa446babfe2a314b522582c44f230590b6eed7300916f2c2667eb776e59ff199`.
Helmet-бинарник подготовлен, но плата helmet не обнаруживалась/не записывалась.

Итоговая отдельная10/10инвентаризация после OTA:
- все8узлов fresh и localScoreSupported=true;
- **master ready=true/storedDevices2/plannedDevices2**, scoreProtocol7/repeat200мс;
- назначенные DC:DA…C3:C0 и30:30…02:30 имеют scoreVersion2/sceneStatusready;
-6неназначенных scoreVersion0/idle — ожидаемо, им проект не назначался;
- remote перед разблокировкой:76PING/76PONG/0timeouts,p50≈9,8мс,p95≈15,5мс;
- workspace после master OTA и после всего rollout **JSON-equivalent исходному**:
  «И будет ласковый дождь»,9шагов/2прибора/v2/fingerprint93081004;
- до/после unlock master stepnull/runningfalse/livePreviewfalse/transition0:
  ни одной новой сцены при проверке не запущено;
- оба входа пульта отпущены, masterReadytrue. Только после2/2готовности выполнен
  авторизованный POST buttons-test enabled=0: **buttonTestfalse/buttonsEnabledtrue**,
  настройка записана в NVS. Remote новый boot/app1 подтверждён;
- реальный gzip HTML/JS получен GET и распакован: `/assets/index-BN6bzoxh.js`
  содержит «Предпросмотр» и preview-mode/draft-go/navigate API. Браузерную сессию,
  которая могла бы включить LIVE, для проверки не открывали.

Private backup/evidence (не Git):
`~/Theatre-console-backups/ota-all-078-20261106-182002/`:
`score-backup-before.json`, `score-backup-after-master.json`, `score-backup-after-all.json`,
индивидуальные before/after/final snapshots, HTTP200responses, `deployment-result.json`.
Прежние full16MB recovery/remote backups и prepared SHA-образы сохранены.
Временный rollout-client `/tmp/theatre-deploy-078.py` выполнял только individual
OTA/GET и отдельное снятие diagnostic lock; это не встроенный master-driven fleet OTA.

Оператору: закрыть старую вкладку и обновить `http://192.168.50.119` с очисткой кеша
(⌘⇧R). Предпросмотр ON явно включает выбранную сцену/правки. Проверять реальные
кнопки/3с double-ACK/веб-follow и нагрузки на безопасном стенде; сам этот rollout
не доказывает физический свет/аудио/реле или видимость нового long-ACK.

## Подготовка0.7.9 — новая карта slave/YX6300, без deployment

По просьбе оператора начать пайку и готовить следующий firmware реализована
новая карта: **ESP RX1←TX player, ESP TX2→RX player; REL1/2=39/38;
DIM1–12=5,6,7,15,16,17,18,8,9,10,11,12; DATA47/RGB48**. RX/TX здесь названы
со стороны ESP, это не выводы RX/TX USB-COM. Оператору сразу сообщены направления
и запрет включать новую проводку с ещё установленной0.7.8. Лента оставлена
прежней WS2811; если нужна настоящая SPI/CLK, требуется модель/новая спецификация.

Драйвер YX6300-family/checksummed10B/стенд UART подготовлен; NVS уровни/инверсии/
полярность реле не стёрты/не инвертированы. Архивные control/wifi04/espnow/radio
и их XY-драйверы не менялись. Master1/2buttons и remote13/14 остаются прежними,
исполнительная новая карта и UART инициализируются только на slave.

На реальные ESP **никаких обновлений/команд звука или сцен в этой подготовке
не отправлялось**. Весь известный реальный комплект по-прежнему0.7.8 и старый
pinout. MAC первой уже перепаянной платы и отдельное разрешение на запись ещё
нужно получить перед physical bench. Реальный конкретный модуль YX6300 ещё
не осматривался/не проверен по уровням/звуку; software mocks этого не доказывают.
[Новая разводка](PINOUT.md), [драйвер/совместимость/стенд](YX6300.md).

## OTA0.7.9 master+8slave, пульт исключён —2026-11-08

Оператор уточнил: подключено только питание, силовые нагрузки не включены,
платы ещё не распаяны; **явно разрешил прошить всех, включая master, оставив пульт**.
Новая карта GPIO теперь в firmware; до распайки по ней силовые не подключать.
Перед любым upload каждый адрес проверен по **MAC+role**, образ — по исходному
commit b2e3095ab2931cb3e6a2fecab68e7ea931383fff, размеру/SHA256/ESP header/version.
Master первым, затем8slave последовательно; каждый upload HTTP200 + новая загрузка/
смена token + MAC/role/version0.7.9. Это **не** USB/full erase: только приложение OTA,
NVS/storage/партитура не стирались. Remote MAC30:30:F9:1B:FD:6C отсутствует в списке
записи и дополнительно явно запрещён скриптом; исходный remote image0.7.9 не использован.

| Роль | MAC | IP | Подтверждено |
|---|---|---|---|
| master | A0:85:E3:E2:BC:D4 |192.168.50.119|0.7.9/new boot|
| slave | DC:DA:0C:43:C3:C0 |192.168.50.41|0.7.9/new boot/new GPIO|
| slave |30:30:F9:32:02:30|192.168.50.65|0.7.9/new boot/new GPIO|
| slave |74:4D:BD:73:0A:A4|192.168.50.20|0.7.9/new boot/new GPIO|
| slave |C0:4E:30:27:69:98|192.168.50.33|0.7.9/new boot/new GPIO|
| slave |30:30:F9:31:19:C4|192.168.50.55|0.7.9/new boot/new GPIO|
| slave |84:FC:E6:57:0F:AC|192.168.50.58|0.7.9/new boot/new GPIO|
| slave |30:30:F9:17:8B:14|192.168.50.138|0.7.9/new boot/new GPIO|
| slave |EC:DA:3B:56:F2:CC|192.168.50.197|0.7.9/new boot/new GPIO|

Образы из `~/Theatre-console-backups/prepared-079-YX6300-20261106-233732/`:
- master1247824B SHA256`4b4f2045282eff9b3a668bfad3c49fbdf55acf2cee4e8d4932ad9f2c8bdf9ff3`.
- slave1090304B SHA256`443fa7112c4771c7b2980006a01782a358027a445a4839bfe40767e2d57bbc93`.
Файлы `*-NOT-FLASHED.bin` именованы так при подготовке; **эти два образа теперь
установлены**, remote/helmet из этой папки не записывались. Исходный CI42289853307
успешен. Нового кода между подготовкой и OTA не было.

Свежие копии/evidence: **`~/Theatre-console-backups/ota-master-slaves-079-20261108-014040/`**,
доступ700/600, внеGit. Внутри `score-backup-before.json`, `score-backup-after-master.json`,
`score-backup-after-all.json`,9before/after/final statuses, HTTPresponses, authorization,
`deployment-result.json`. Это новый экспорт, не старый backupv2 от6ноября:
**«И будет ласковый дождь»,9шагов/2прибора/публикацияv14/fingerprint07c8f468**.
JSON до/после эквивалентен и SHA256файлов одинаков:
`6687782cf3e46922cdff88d10544c987a9233a5edfb4fa17354f875ef671e198`.
Видимые persistent levels/inversion первых8PWM и relayMode всех8slave совпадают
до/после; это не утверждение побайтовой проверки всей NVS по OTA.

Master подтвердил **2/2** текущих plans14, все8node reportAge<500мс в финальном
снимке. Текущие назначения **EC:DA:3B:56:F2:CC «Гардероб+Колыбель»** и
**30:30:F9:32:02:30 «Зеркало+3Д принтер»**. Предыдущее назначениеDC:DA:0C:43:C3:C0
из backupv2 больше не действует: у него, как у остальных5неназначенных,score0/idle.
Оба назначенных —score14/ready. Никаких переназначений мы не отправляли.

На каждом slave `/api/status`:hardwareProfile=s3-yx6300-rx1-tx2-v1,
RX1/TX2, dimmerPins5,6,7,15,16,17,18,8,9,10,11,12, relayPins39/38, strip47,
player=YX6300. Это проверка metadata firmware, **не пайки/электрических уровней**.
GET`/uart` подтвердил новую страницу YX/test controls; GET root/JS с распаковкой gzip
подтвердил actual production0.7.9. Первичная попытка прочитать gzip какUTF8 была
исправлена декомпрессией, это не ошибка ESP/OTA.

Master в конце:`running=false/livePreview=false/step=null/navigation0/transition0`,
ready=true,repeat200мс,score7. DraftFingerprint пуст после reboot до подключения
редактора — это RAM draft bank, сохранённый workspace/публикация не потеряны.
**GO/PREVIEW/навигации/команд аудиотеста не было**, физический свет/звук/нагрузки
не проверены. Пульт поHTTP в этом сеансе не ответил, но не сбрасывался/не прошивался;
не выдаём его за заново проверенный. ReloadUI: http://192.168.50.119 (⌘⇧R),
стенд после распайки: http://192.168.50.119/uart. [YX6300.md](YX6300.md).

## После OTA0.7.9 / подготовка DIM animation0.7.10 —без новой прошивки

Оператор сообщил: всё запланированное работает, треки переключаются, диммеры
горят; у WS2811 виден шум, он намерен сам дебажить проводку/сигнал. Это **его
наблюдение**, не наша инструментальная частота/нагрузка/аудиопроверка. Он попросил
сразу разработать независимую анимацию каждого DIM в каждом шаге прибора и оставил
master+одинslave в сети. Это разрешение на разработку, **не новая команда OTA**.

Подготовлена0.7.10: пять функций, period/phase-or-seed/brightness, floor/duty/
random smoothing. GPIO/плеер/WS2811/remote не менялись. [Описание/миграция](DIMMER_ANIMATION.md).
Score8/Cue314/STORE358/RUN44, capP/8/P/9: для будущего теста нужны master+тестовый
slave0.7.10, а для полного опубликованного исполнения — все назначенные совместимые
узлы. Никакого автоматического обновления0.7.9 peer нет.

Перед разработкой сделаны **только GET** `/api/status` и `/api/console/workspace`
на проверенный master MACA0:85:E3:E2:BC:D4. Новый приватный backup:
`~/Theatre-console-backups/before-dimmer-animation-20261108-053418/`.
На тот момент **master0.7.9,12шагов/3прибора/публикация46**; свежий slave в его
статусе **74:4D:BD:73:0A:A4 /192.168.50.20**. Это не старый backupv14/v2 и не
разрешение прошивать по одномуIP. Никаких GO/PREVIEW/navigation/audio/config/OTA
на реальных платах в ходе этой разработки не отправлялось; актуальный workspace
не менялся инструментами. Для будущей OTA снова снять свежую копию/проверить MAC/role.

Программно: ASan/UBSan actual12-channel runtime/codec/cache migration/storage,
36domain, MOCK ESP production/mobile UI (все формы/LIVE и OFF без скрытого GO),
все4роли Arduino ESP32собраны. Источник/binaries сохранены отдельно; физического
теста новой динамики ещё нет, при подготовке устройства оставались0.7.9. Пульт не трогали.

## OTA0.7.10 master+один test slave —2026-11-08

После подготовки оператор явно ответил **«да, обновляем»** на предложение записать
master и оставленный slave **74:4D:BD:73:0A:A4**, без пульта/других slave.
Использованы ранее проверенные образы кода **91c32b6d510a96fbf68df7b1dab4309ebfed1759**,
CI42440406541 success; до записи размер/ESP header/version/SHA256 совпали с manifest.
Нового кода до записи не было, исходники/worktree чистые.

Перед OTA GET по текущим адресам подтвердил MAC/роль обеих плат, обе0.7.9.
Master был **LIVEon/runningtrue,scene4**, publication46/fingerprintcfe5fcde,
12шагов/3прибора. Сначала сохранён свежий workspace/статусы, затем оператору явно
сообщено о maintenance остановке без возобновления. POST`/stop-all` дал202;
master LIVE/runningfalse, test slave COMMAND_ACK accepted и логическиеPWM0/relayoff
подтверждены. Это software ACK/снимок, не физические датчики. Временноsyncingtrue
при1/3планов не выдавался за опасный GO; запуски не отправлялись.

| Роль | MAC | IP | Образ/подтверждение |
|---|---|---|---|
|master|A0:85:E3:E2:BC:D4|192.168.50.119|1251264B,HTTP200→0.7.10/new boot/token|
|slave|74:4D:BD:73:0A:A4|192.168.50.20|1096160B,HTTP200→0.7.10/new boot/token|

SHA256 master`f40860be591ac617f53312d879d806b769ca1f2eb3b0c3c82f0ab66e25dd4866`,
slave`a65863fd5d6102f5e78ed55fa49d5e85d2f2da9877669703c528e0e93dd29153`.
Образы/ELF: `~/Theatre-console-backups/prepared-0710-DIM-animation-20261108-055354/`.
Master записан первым, после сохранности JSON — slave; каждый отдельный upload
проверен по **MAC+role+version+новый token**, не только HTTP200/IP.
Только OTA-приложение, без erase/новых partitions. Остальные7slave иremote **не
записывались**; их новые0.7.10 images не использованы.

Свежий отдельный журнал/экспорт/авторизация/responses/before/after/final статусы:
**`~/Theatre-console-backups/ota-master-slave-0710-20261108-135744/`**, доступ700/600,
внеGit. `score-backup-before.json`, `score-backup-after-master.json`,
`score-backup-after-all.json` JSON-эквивалентны; SHA256 до/после одинаков:
`67c33c2aa7878a9ecc265dab72d786965f11cd051b6c7ab76efe353543bcb60c`.
Это **публикация46,12шагов/3прибора**, не старый14/2прибора. Видимые presets/inversion
первых8PWM и relayMode test slave совпадают, GPIO RX1/TX2,DIM/REL/strip47 прежние.

Финальный master:`version46/fingerprintcfe5fcde/stepnull/LIVEoff/runningfalse/
navigation0/transition0/scoreProtocol8/repeat200`. Test slave:
**localScoreSupported=true,scoreVersion46,sceneStatusready**, reportAge207мс.
Это подтверждает старый статический план после score8 RAM conversion/согласованияCRC.
Общий **stored1/planned3/readyfalse** ожидаем: два других назначения не в сети и не
входили в разрешение этой OTA. Никаких переназначений ради ложной готовности не было;
для полного «Показа» нужны все назначенные совместимые score8-узлы. Для текущего
стенда одного прибора использовать явный LIVE после проверки безопасной нагрузки.

GET root + production JS с распаковкой gzip подтвердил actual0.7.10,
`/assets/index-3zUbVBtS.js` с «Включить анимацию»/period/floor/random-smooth controls.
GET не исполнял JS/не запускал авто-sync браузера. Новые GO/PREVIEW/навигация/аудиотест
**не отправлялись**, физическая новая динамика пока не проверена; старый LIVE не
восстанавливался. Открыть http://192.168.50.119 с⌘⇧R; ячейкаDIM→«АнимацияDIM».

## Подготовка audio start delay0.7.11 —без новой прошивки

Оператор попросил добавить в звук каждого шага задержку старта трека в секундах.
Подготовлено поле0–3600с (optional JSON startDelay, default0), UI text+zero-safe
slider, локальный deadline/cancel/actualTX fade, preview waiting. При STOP/новой
сцене pending/queued PLAY не всплывает; уже играющий same-file forward продолжается.
[Семантика/совместимость](AUDIO_START_DELAY.md). Код score9/P10,P11, размеры314/358/44
прежние; старые cache7/8 переходят в RAM с delay0 без стирания/потери DIM-анимации.

Для сохранности сделаны только GETstatus/workspace проверенного мастера:
`~/Theatre-console-backups/before-audio-delay-20261108-232136/`, **master0.7.10,
12шагов/3прибора/публикация68**. Это не backup46, плейлист/сцены не редактировались.
На реальные ESP не отправляли ни OTA, ни STOP/GO/PREVIEW/audio/navigation/config.
Установленная тестовая пара остаётся0.7.10, другие7slave0.7.9/remote0.7.8 неизменны.
41domain, ASan/UBSan actual Runner/Player/storage/wrap/cancellation, MOCK ESPUI,
четыре Arduino-сборки проверены. Физического теста задержки ещё нет. Перед будущей
отдельно согласованной записью снова проверить MAC/роль и сохранить свежую партитуру.

## OTA0.7.11 только сейчас видимого DC-slave —2026-11-09

Команда оператора: **«перепрошей сейчас видимого слейва»**. По GET master status
единственный свежий peer оказался **DC:DA:0C:43:C3:C0 /192.168.50.41**, не прежний
74:4D:BD:73:0A:A4. Direct GET подтвердил **role=slave,version0.7.9**, известную
YX-карту RX1/TX2/12DIM/relay39/38/strip47. MasterA0:85:E3:E2:BC:D4 —0.7.10,
LIVEoff/runningfalse. Оператору до записи сообщено о конкретномMAC и что master0.7.10
не совместим со slave0.7.11; master/пульт по этой узкой команде не трогать.

До upload сохранены slave/master status и свежий workspace **12шагов/3прибора/v69**,
fingerprinta792a6f9. Образ slave из подготовки0.7.11 кода5eba22ebdd76d85a67fed4171670e66d87ba2d9d,
CI42520729447 success,1096352B,
SHA256`5c0878e6667806430b32f1b7584b0ae18c94818e4b44f3764432c5e7fd968972`.
Размер/ESP header/version/hash проверены; POST только `/update` DC-role/mac,
HTTP200→**новый boot/token и тот же MAC/role/version0.7.11** проверены.
GPIO/видимые preset levels/inversion/relayMode сохранены. Это OTA приложения,
не erase/NVS/разделы; конфигурация и назначения приборов не менялись.

Evidence внеGit:
**`~/Theatre-console-backups/ota-single-slave-0711-20261109-204400/`**, доступ700/600.
`slave-before/after.json`, `master-before/after.json`, response, authorization,
`deployment-result.json`, exportbefore/after. Workspace JSON и SHA256 одинаковы:
`dcdf43c5ff4f4e8622d0abd0dd470da01c68a23ccb1f921f1a6fe72dc4cb31d3`.
Master **token иversion0.7.10 неизменны**, никакой master OTA/STOP/GO/PREVIEW/audio/
navigation/config не было; другие slave/remote также не записывались.

Финальный master:`publication69/fingerprinta792a6f9/stepnull/LIVEoff/runningfalse/
scoreProtocol8/stored0/planned3/readyfalse`. Новый DC-slave обнаружен, но
**localScoreSupported=false у старого master**, sceneStatusidle (не выдаём за plan ACK).
Score9 slave не исполняет score8 сцены. Для использования новой прошивки нужно
отдельно разрешить master0.7.11; его образ подготовлен, не установлен. Физический
аудио-delay/нагрузки при этой записи не тестировались. Остальные устройства нетронуты.

## OTA0.7.11 master, пульт не требует обновления —2026-11-09

Оператор: **«прошей тогда и мастера и пульт тоже. или пульт не требует обновления?»**.
Ответ до записи: пульт R/1 совместим, обновлять не требуется, оставляем как есть;
обновляем только master. Отдельной переназначающей/пускающей команды не было.

GET подтвердил **masterA0:85:E3:E2:BC:D4 /192.168.50.119,version0.7.10**, idle/LIVEoff;
DC-slave до записи0.7.11, remote-readonly0.7.8. Сохранён свежий workspace **12шагов/
3прибора/v69** истатусы. Образ master1251792B кода5eba22ebdd76d85a67fed4171670e66d87ba2d9d,
SHA256`b2b1b0122980afd5795145decd54b629b97861b9ee7a16bf80eefef41b36f291`, CI42520729447
success; header/size/version/hash совпали. Между исходным build и HEAD изменены только
документы (git diff code paths пуст). POST только master`/update`, HTTP200→**новый
token/boot, тот же MAC/role,version0.7.11/score9** проверены. Только OTA-приложение,
NVS/storage/partition table не стирались. Remote/slave не записывались.

Evidence: **`~/Theatre-console-backups/ota-master-0711-20261109-205430/`** (700/600,
внеGit): fresh score before/after, master before/after/final, remote/slave read-only
before/after, upload-response, authorization, deployment-result. Экспорт JSON
эквивалентен, before/after SHA256 одинаков:
`dcdf43c5ff4f4e8622d0abd0dd470da01c68a23ccb1f921f1a6fe72dc4cb31d3`.
DC-token/version0.7.11 неизменны. Remote **version0.7.8/token неизменны**, после
master update **masterReadytrue/buttonTestfalse/buttonsEnabledtrue**. Это software
handshake/ping readiness, не тест физических кнопок/навигации; firmwareпульта не меняли.

Новый master обнаружил DC: **localScoreSupportedtrue**, score9-совместим. Но
**sceneStatusidle**, потому что DC **не назначен ни одному прибору** (не ошибкаOTA).
Неизменённые draft bindings:
- «Гардероб+Колыбель» →EC:DA:3B:56:F2:CC (.197).
- «Зеркало+3Д принтер» →30:30:F9:32:02:30 (.65).
- «тест1» →**74:4D:BD:73:0A:A4 (.20)**, сейчас offline, не DC(.41).
Никаких автоматических перепривязок/публикации нового номера не делали. Перед
сменой MAC «тест1» требуется команда оператора. Для полного показa все назначения
должны быть доступны/score9-совместимы; сейчас **stored0/planned3/readyfalse**.

Финальный master:`publication69/fingerprinta792a6f9/score9/stepnull/LIVEoff/runningfalse/
navigation0/transition0`. Actual GET root/JS (gzip) подтвердил0.7.11 и «Задержка
старта трека». **GO/PREVIEW/STOP/navigation/audio tests не отправлялись**, исполнение
не возобновлялось. Открыть http://192.168.50.119 с⌘⇧R. Проблема несовпадающих protocol
версий решена; следующая задача — явно согласовать привязку testприбора кDC.

## OTA ещё одного видимого30-slave —2026-11-09

Оператор после обсуждения привязок сказал **сам настроит партитуру**, затем
**«прошей ещё одну»**. Единственный свежий peer в master status:
**30:30:F9:31:19:C4 /192.168.50.55** (DC уже stale). Direct GET подтвердилslave0.7.9,
YX hardware map. Оператору до записи сообщены конкретные MAC/IP и maintenance
остановка LIVE без возобновления. Никаких назначений приборов не менять.

Сохранены master/slave before и workspace: **2шага/1прибор,публикация80**, выбран
LIVE step1, RAMdraftFingerprint753fcae1 при publishedFingerprint0104199b. Вычисленный
из приватного экспорта `fingerprint(draft)` совпал с753fcae1 — актуальные LIVE-правки
тоже сохранены, не только старый опубликованный snapshot.

POST master`/stop-all` подтвердил LIVE/runningfalse + accepted software ACK новой
платы. Затем только её OTA: образslave1096352B кода5eba22ebdd76d85a67fed4171670e66d87ba2d9d,
CI42520729447 success,
SHA256`5c0878e6667806430b32f1b7584b0ae18c94818e4b44f3764432c5e7fd968972`.
Header/version/size/hash проверены, HTTP200→тот же MAC/role/version0.7.11 и новый
boot/token. Пресеты/inversion/relayMode/GPIO совпали; приложения толькоOTA, безerase.
Master firmware/token/version0.7.11 не менялись, remote/другиеslave не писались.

Evidence: **`~/Theatre-console-backups/ota-extra-slave-0711-20261109-211909/`**,
доступ700/600 внеGit; before/after/after-stop exports, statuses, authorization,
response иdeployment-result. **Весь JSON до/после не одинаков**: существующий
редактор после перехода LIVEoff штатно синхронизировал тот же draft **v80→v81**.
Инструмент не вызывал publish/import/GO/правки настроек. Проверено:
`before.draft == after.draft == after.published.project`, bindings неизменны.
Не восстанавливать старую80 поверх актуальной81! Оба snapshot сохранены.
Before SHA256`5feb8b5844fa1dd25e45260d754bbd3a155bebe7f364e2b91bc4c05898b2a724`,
after SHA256`74e22c2ddc808ba12a9a82f2b8d0af95ab73da6bc8352966e748a1319cc35031`.

Финальный master:`v81/fingerprint753fcae1/step1/LIVEoff/runningfalse/syncingtrue/
navigation101/transition151/stored0/planned1/readyfalse/score9`. Новый30-slave:
**localScoreSupportedtrue,sceneStatusidle,score0** — совместим, но не назначен прибору.
Текущий прибор«3» привязан **DC:DA:0C:43:C3:C0**, который уже offline. Это изменения
оператора до этой команды, мы их не заменяли и не переносили на новую30плату.
Новую сцену/аудиотест не запускали, физической проверки новой платы не было.
LIVE не возобновляли; дальнейшие назначения/тест оператор делает самостоятельно.

## OTA видимого84-slave —2026-11-09

На повторную команду **«перепрошей ещё одну»** GET master выявил единственный
свежий peer **84:FC:E6:57:0F:AC /192.168.50.58**, direct GET подтвердилslave0.7.9,
YX map инулевые логические выходы. Других свежих slave не было. Master0.7.11
работал вLIVE наstep1,publication89/0d3830d7, RAMdrafta66ea238.

Перед изменением сохранён **актуальный draft2шага/2прибора**, fingerprint приватного
экспорта совпал сRAMa66ea238, то есть сохранены и неопубликованные LIVE-правки.
Оператору сообщена maintenance остановка; `/stop-all` дал accepted software ACK,
master LIVE/runningfalse. Затем только84-slave записан OTA0.7.11,1096352B,
код5eba22ebdd76d85a67fed4171670e66d87ba2d9d, CI42520729447success,
SHA256`5c0878e6667806430b32f1b7584b0ae18c94818e4b44f3764432c5e7fd968972`.
HTTP200→direct GETтотжеMAC/role/version0.7.11/новыйtoken. GPIO/preset levels/
inversion/relayMode сохранены. Partition/NVS erase не выполнялись.

Первый postupload gate ожидал ровно один fresh peer и попал в короткий reconnect
промежуток **до fresh-report у master**. Direct status уже был0.7.11/new boot.
**Повторной записи не было.** Read-only повторная проверка подтвердила84(.58),
localScoreSupportedtrue и свежий master report. В helper уточнена только postupload
проверка: допускать временно отсутствиеfresh target до его появления; preflight
MAC/role/единственныйfresh target остались строгими. Это не failure OTA.

Evidence: **`~/Theatre-console-backups/ota-slave-84fc-0711-20261109-213924/`**,700/600,
внеGit. Сохранены before/after/after-stop exports,statuses,response,authorization,
deployment-result. Как при предыдущейOTA, **весь JSON до/после различается**:
существующий editor приLIVEoff штатно опубликовал тот же draft **89→90**.
Проверено `before.draft == after.draft == after.published.project`; привязки нетронуты.
Не заменять90старой89. Инструмент не отправлял publish/import/GO/настройки.
Before SHA256`26023ba87f62c705be684af0effa80cef78a77bb68ad404a268fd2add93213ae`,
after SHA256`c48f0c14fb6f52265bb6c4e7f0de9989d1708ee11f18bcb9146dda0282db75b7`.

Master token/version0.7.11 не менялись; firmware master/remote/другихslave не писали.
Новый84score9-совместим, ноidle/score0 — не назначен прибору. Неизменённые назначения:
«3»→DC:DA:0C:43:C3:C0, «5»→30:30:F9:31:19:C4, оба ужеoffline. Masterv90/score9,
LIVEoff/runningfalse, публиковался a66ea238; общий show ждёт назначения, не объявлен
ложно готовым. GO/аудио/физические тесты не делали, LIVE не возобновляли. Оператор
настраивает привязки/партитуру самостоятельно.

## OTA видимогоC0-slave —2026-11-09

На команду **«ещё один»** единственный fresh peer — **C0:4E:30:27:69:98 /192.168.50.33**;
direct GET подтвердилslave0.7.9, YX-карту, relayMode2, пресеты/inversion. Master0.7.11
былLIVEon/runningtrue/step1,publication95/fingerprint1641c174, RAMdraftd4fb5f32.
Перед записью оператору сообщены MAC/IP и остановка LIVE без возобновления.

Fresh backup **2шага/3прибора/v95**, вычисленныйfingerprint(draft) совпал сd4fb5f32
RAM. `/stop-all` подтвердил LIVEoff и accepted software ACK. ТолькоC0-slave записан
OTA0.7.11 (0.7.9→0.7.11), образ1096352B, source5eba22ebdd76d85a67fed4171670e66d87ba2d9d,
CI42520729447success,
SHA256`5c0878e6667806430b32f1b7584b0ae18c94818e4b44f3764432c5e7fd968972`.
HTTP200→тотжеMAC/role/version/новыйtoken подтверждены, localScoreSupportedtrue,
sceneStatusidle/score0 (не назначен), GPIO/preset levels/inversion/relayMode2 сохранены.
Master version/token неизменны, firmware другихslave/remote не писались; NVS/разделы
не стирались, GO/аудиотест/перепривязка не выполнялись.

Evidence: **`~/Theatre-console-backups/ota-slave-c04e-0711-20261109-221207/`**,700/600,
внеGit. До/после различается номер публикации: существующий editor приLIVEoff
опубликовал тот же draft **95→96**. Проверено `before.draft == after.draft ==
after.published.project`; draft/привязки сохранены, tool publish/import не вызывался.
Сохранены обаsnapshot, старой95 не откатывали новую96.
Before SHA256`1fbb74b38bafedc91a5810f27f53c1c25b6c4ec679fc2d6ebc1bde3cf7b177fa`,
after SHA256`e3787703526c22e96eb4187786bf0a1b694c80e6e6e521fe3bd12c22de8f8b8a`.

Финальный masterv96/fpd4fb5f32/step1/LIVEoff/runningfalse/syncingtrue/nav263/
transition320/stored0/planned3/score9. Новую плату совместимой firmwareвидно,
но readiness за неназначенные/отключённые приборы не выдумывается. Физических
тестов новой платы/звука не делали, LIVE не возобновляли. Оператор настраивает сам.

## OTA двух видимых32/EC —2026-11-09

Команда оператора **«прошей2»**: в master status ровно два свежих peer, direct GET
подтвердил **30:30:F9:32:02:30 /192.168.50.65** и **EC:DA:3B:56:F2:CC /192.168.50.197**,
обаslave0.7.9. Названы оператору до записи, согласованная maintenance остановкаLIVE
без возобновления, привязки не менять. Master0.7.11 был LIVEon/step1/publication103/
fpc36ef2ae, RAMdraft2184225c, planned4.

Fresh экспорт **2шага/5приборов/v103** сохранён, fingerprint(draft) совпал с2184225c
RAM: все неопубликованные LIVE-правки в backup. `/stop-all` дал software acceptedACK
обоих targets, master LIVE/runningfalse. Затем только эти2slave последовательно
OTA0.7.11, каждый образ1096352B/source5eba22ebdd76d85a67fed4171670e66d87ba2d9d,
CI42520729447success,
SHA256`5c0878e6667806430b32f1b7584b0ae18c94818e4b44f3764432c5e7fd968972`.
КаждыйHTTP200→тотжеMAC/role/version0.7.11/новыйtoken, потом score9 fresh peer
localScoreSupportedtrue. Preset levels/inversion/relayMode/GPIO обоих сохранены.
Только приложениеOTA, безerase/NVS/разделов. Master version/token неизменны,
master firmware/remote/другиеslave не писались.

Evidence: **`~/Theatre-console-backups/ota-two-slaves-0711-20261109-235007/`**,700/600,
внеGit. Оба before/after/finalstatuses иresponse, authorization/maintenance ACK,
master/score before-after-after-stop, deployment-result. Весь JSON различается,
потому что существующий editor приLIVEoff штатно опубликовал тот же draft **103→104**.
Проверено `before.draft == after.draft == after.published.project`; bindings неизменны.
Toolpublish/import/GO не было, старую103 не восстанавливали поверх104.
Before SHA256`e33f3f17ab9d82125a873e104020d914aa2f59814d4971f723fd9158911efa55`,
after SHA256`793484b89f7462f6bbe2b605cd193fa87086ffab79dc190f1820194fed76aac6`.

Финал:masterv104/fp2184225c/step1/LIVEoff/runningfalse/syncingtrue/nav318/
transition384/score9/stored0/planned4. Оба новых peersidle/score0 — не назначены
приборам, не выдаём за plan ACK/готовность проекта. GO/аудиотест/перепривязки не
выполнялись, физического теста этих плат не было. Оператор настраивает сам.

## OTA видимого74-slave —2026-11-09

На команду **«прошей кшк адин»** (ещё один) fresh inventory показал3узла:
32(.65)/EC(.197) уже0.7.11, **74:4D:BD:73:0A:A4 /192.168.50.20** ещё0.7.10.
Проверены directMAC/role/version, оператору назван единственный старый target.
Другие две платы явно исключены из upload. LIVE ужеoff/runningfalse — STOP не нужен.

Fresh backup **2шага/5приборов/v104/fp2184225c**, сохранённый draft совпал сRAMfingerprint.
Только74 записан OTA0.7.11:1096352B, source5eba22ebdd76d85a67fed4171670e66d87ba2d9d,
CI42520729447success,
SHA256`5c0878e6667806430b32f1b7584b0ae18c94818e4b44f3764432c5e7fd968972`.
HTTP200→тотжеMAC/role/version0.7.11/новыйtoken, fresh peer localScoreSupportedtrue.
GPIO/preset levels/inversion/relayMode сохранены. Master token/version не менялись,
master firmware/remote/другиеslave не писались; NVS/разделы не стирали.

Evidence: **`~/Theatre-console-backups/ota-slave-744d-0711-20261109-235734/`**,700/600,
внеGit. UnlikeпредыдущиеLIVE-обновления, здесь **весь workspaceJSON и SHAфайла
совпадают**, публикация104 не изменилась:
`793484b89f7462f6bbe2b605cd193fa87086ffab79dc190f1820194fed76aac6`.
Draft/привязки не менялись; publish/STOP/GO/PREVIEW/audio/navigation не отправлялись.
Masterv104/score9/LIVEoff/runningfalse; новый74idle/score0, не назначен прибору,
readiness за неназначенные/отключённые приборы не выдумана. Физического теста не было.

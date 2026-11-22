# Проверенные записи на платы

## 22.11.2026 — новый USB master 0.8.10 + последний JSON v1.057

По прямому разрешению пользователя первоначально подготовлена **другая плата**:
**A0:85:E3:E3:BF:A8, ESP32-S3/16 MB/PSRAM8 MB**, а не прежний master
A0:85:E3:E2:BC:D4. USB descriptor → ROM MAC → фактический runtime MAC/role/version
проверены; имя порта не использовалось как единственное подтверждение личности.
Ранее на ней был заводской MicroPython (factory/VFS), не театр. После отдельного
явного разрешения заменена заводская разметка на стандартную console OTA6MB×2;
до записи сохранены полные 16 MB исходной flash, затем свежая копия перед записью.
Не было erase-flash; старые VFS-байты на этом шаге физически не менялись (полная
исходная копия нужна для восстановления прежней разметки/системы).

Установлен master0.8.10 из source **9d31bc1**, app **1 320 256 байт** в app0,
SHA-256
`608e57f56b9c76ede0d788034a5e9b8964bea0c37bc0da4919ee6df23e018d7b`.
Одна первоначальная запись bootloader/table/OTAdata/app. Независимым чтением всей
16MB flash проверены точный образ и все остальные байты. Общий script timeout
произошёл уже после успешной записи, во время чтения: повторено только чтение,
а не неизвестная запись. Первый boot пустого master подтверждён отдельно.

По дате изменения выбран и в конце повторно проверен последний спектакль:
`~/Downloads/И-будет-ласковый-дождь_v1.057.json`, 1 797 757B, SHA-256
`cf6a5d29fd962644a599d091af08a3331e22f519473ba8b02e0a5d4b5d7a637c`.
**37 шагов / 8 приборов, published446/fingerprint0c0501a0, draft10905**.
Исходный JSON записан байт-в-байт: оба score/legacy voice-банка, все MAC/привязки,
root independent bank revision64, en/жен/муж/рэп и working сохранены. Черновик не
подменён старой копией и не опубликован вместо исходной published446.

SSID YOUR_WIFI_SSID недоступна: файл тоже загружен **по USB**, без HTTP/OTA. В ранее пустой
storage0xc10000/0x3f0000 записан LittleFS с workspace.json, compiled published.json
(текущий штатный компилятор) и helmet-voices.json. Независимое чтение storage,
распаковка и сравнение всех трёх файлов подтверждены. NVS/таблица/OTAdata/bootloader
во время этой записи байт-идентичны, app SHA повторно проверен. Файловый образ SHA
`001175c18f790c352944fe9dac376bc560b0de60565a2d1509b45e0ebfcc394d`.

Новый boot с этим спектаклем отличается от первого пустого boot; три устойчивых
USB status: master0.8.10/MAC BFA8, version446/planned8/voiceRevision64/savedtrue;
**STOP/LIVEoff/locked/startupPendingfalse, cursorunset/navigation0/transition0**.
Startup intent уже отсутствовал: дополнительный USB `!` или глобальный STOP
не отправлялся. Никаких GO/LIVE/navigation/I/O тестов, POST другим платам или
перепрошивок остальных не было. RAM draft cache revision0 после cold boot не
означает потерю сохранённого draft10905 — файл отдельно проверен целиком.

**Ограничения:** connectedfalse/IP0.0.0.0, peers0 — нет проверки HTTP страницы,
онлайн-приборов, физического звука/света или обычного power-on автостарта на нагрузке.
При доступной YOUR_WIFI_SSID hostname: `stage-master-e3bfa8.local`. Политика нормального
автостарта при следующем обычном включении сохранена. Не эксплуатировать старый
и новый master одновременно в одной сети с одинаковыми привязками спектакля.
Прежний master0.8.8,
№6/AC082 и прочие версии этим действием не менялись; их текущая работа не заявлена
как независимо проверенная. Wi-Fi интернета Mac не отключался.

Приватные свидетельства700/600:
`usb-master-0810-A085E3E3BFA8-20261122-033715/` — full factory backups/readback,
одна firmware write + одна отдельная filesystem write, исходный последний JSON,
compiled plans/voice file/все SHA, независимые извлечённые файлы, новые USB status,
steady checks и deployment-result.json. JSON/NVS/flash в Git не помещались.

## 21.11.2026 — только№6 /0.8.9AC082, безобщегоSTOP

Пользователь отдельно разрешил прошивку№6 при продолжающихработатьостальных.
Свежийworkspace: publication431/draft10820, полныеlegacy/rootvoiceбанки иworking
захвачены безподмены старой413. MasterA0:85:E3:E2:BC:D4/version0.8.8 продолжал
шаг36/idmwl3wlcf-n74s3ss3ve, navigation115/transition132, runningtrue/LIVEfalse.

Цельопределена поимени6вобеихпартитурах ифактическимMAC/role/status:
**C0:4E:30:29:F4:8C,slave,0.8.7→0.8.9**,1124864B SHA
`9bfb6e156c6b30b76f61b5052bfbc95b00f00f1135c8f5a0cfbf226316332075`, sourcea153868.
РовноодинPOSTтольконаего`/update`; никакихPOSTнаmaster/другиеплаты, /stop-all,
/command, GO/LIVE/navigation, новыхпараметров или тестовнагрузки. OTAhandler гасил
только№6. После reboot новоеtoken/версия/профиль`legacy082-fixed200-stop-safe`,
fixed200мкс/max200/pulseSettingSupportedfalse подтверждены. №6сампринял текущий
продолжающийсяRUN431/36; дополнительныйGOне отправлялся. После возврата8/8плановготовы.

До/после mastertoken/runtimeточносовпали, новыеWorkspace/всебанкисовпалиполностью.
PWM12сохранён:active/saved25000Гц,8бит/gamma1/gray.5, уровни/инверсии/relaymode/
UARTpollingпроверены. СтарыйpulseNVSигнорируется, не стирается; NVS/FS/таблицыразделов
сохраняютсяapp-only. У6прочихordinaryslaves token/версии/выходысовпали иdelta
PWM/relay1/relay2/OFFwrites0; пульттотжеboot/sentdelta0. Шлемуже былofflineдоOTA
(age100746мс вfreshpreflight), поэтомуегоboot/outputневыдаётсязапроверенный;
никакихкомандилиобновленийшлемаэтойоперациейнебыло.

Финально№6ACqualifiedtrue/latchedfalse/faultok/requested0/paired0/pulse200,
**физическаяустойчивостьприактивнойACнагрузкеещёне проверена**. Не утверждается,
чтоисходнаяпричинаотключениянайденаилиустранена. FreshZ-Cприlossперезахватывается
старымавтоматом, CPU/resourcehardзащитыостаются. Электрическихизмеренийнебыло.

Свидетельства700/600: приватный`ota-fixture6-089-no-global-stop-20261121-210055/`
(workspace/banks/status/image/hash/auth/одинPOST/newboot/rejoin/otherscomparison/final).
Предыдущий`205906/` — прерванныйread-onlypreflight: одинHTTPнеответил, POSTнебыло;
послеперечитыванияfreshсостояния созданаотдельнаязавершённаязапись, не blindOTA retry.
HTTPfullflash/NVSdump/независимыйrunningflashSHA недоступны, этоявнонеобещается.

## 21.11.2026 — общий OTAрелиз0.8.7 + masterUI0.8.8, завершено10/10

Пользователь явно разрешил общийSTOP и OTA master/шлем/пульт/1–7. ДоSTOP взята
последняя стабильная публикация413, draftrevision10735/37шагов/8приборов; оба
первоначальныхбанкаen/жен/муж/рэп,workingгдеприсутствовал и настройки всех целей.
Draftworking отсутствовал, publishedworking присутствовал — оба факта сохранены,
не подменены старой копией384. Root`helmetVoices` точно повторяетlatestdraftбанк.

Проверены фактические MAC/роль/версии/образыSHA до POST, новые token/boot после:

| Цель | MAC | Было | Стало |
|---|---|---|---|
|master|A0:85:E3:E2:BC:D4|0.8.3|0.8.8 через0.8.7|
|шлем|C0:4E:30:36:C6:88|0.8.3|0.8.7|
|пульт|30:30:F9:1B:FD:6C|0.7.8|0.8.7|
|1|C0:4E:30:27:69:98|0.8.3|0.8.7|
|2|74:4D:BD:73:0A:A4|0.7.18|0.8.7|
|3|DC:DA:0C:43:C3:C0|0.7.18|0.8.7|
|4|EC:DA:3B:56:F2:CC|0.7.18|0.8.7|
|5|30:30:F9:31:19:C4|0.7.18|0.8.7|
|6|C0:4E:30:29:F4:8C|0.8.2|0.8.7|
|7|30:30:F9:32:02:30|0.8.3|0.8.7|

Финальные образы:
- Master0.8.8,1318800B SHA`66e43f08258c52a66743e2e9dec2aecdd3b4c7f2ed50339467f9b483aec0778b`, source3307eae.
- Slave0.8.7,1125920B SHA`49e9abd53e4fbfc96925f86cbcfffce7c440fd5518f87c2ba40235203a7cab54`, sourcef497146.
- Helmet0.8.7,1063680B SHA`7fae9ddde62cf03302de6bc983faf184e1243f2ff59377b5b7800ec85f985a3e`, sourcef497146.
- Remote0.8.7,986384B SHA`353eef7e88ec6ab91bb1862103df98f92dbeeaf13be5ea1bdcda9f9972fab69e`, sourcef497146.

Master сначала прошит7 при старомшлеме3, не способном ACKнезависимыйголос; сразу
явныйSTOP/cancelpending. Послешлема7 сеть меняла linkBoot, firmwareBootId/token
оставалисьстабильны; OTAнеповторялась, ALL_OFFотправлен по свежейпривязке и acceptedACK.

Вживуюобнаружено productionимя успешного ACK`accepted`, в UIпроверке было`applied`.
Подготовлена master-only8,95domain/TS/nativeASanUBSan/productionbrowser/masterSDK.
Перед намеренной второйOTA master пользователь обесточил№6, провереноoffline;
послеmaster8boot явноотменёнpendingдо его возврата. Затемпользовательвключил№6,
снова провереныMAC/роль/boot и выполнена егоединственнаяOTA7. Все остальные цели
получили поодномуPOST. Нет слепых повторов неизвестного исхода и версиирадиверсии.
Master8 отдаёт JS/CSS, побайтно совпадающие с проверенной productionсборкой.

На каждом этапе draft/published/обаlegacyбанка проверены точно, independentбанк
равенlatestdraft. Сохранены PWMуровни/инверсии/relaymode; app-only NVS/FS/partition
layout не стирались. Helmetlegacy saved/appliedSound сохранён, отдельныйglobalNVS
подтверждёнsaved. EQ/presenceOFF. RemoteFULL/LOWавтоматическинепроводились.

Финал:STOP/LIVEoff/editorlocked/startupPendingfalse,8/8плановготовы, cursorunset,
transition0. GO/LIVEon/PLAY/тестоввыхода не отправлялось. КонфигурацияPWM25кГц/8бит/
gamma1/gray.5; AC200мкс.№1плеерудалён:UARTpollOFF восстановленвRAM, genericboot400мс
вместопрежнейдиагностической1300мс, persistentno-player всёещёне реализован.
На№7плеерсохранён/poll400мс; за6.12с16TX/16valid/0checksum/lengtherrors.
За тот же STOPинтервална7slavesнетновыхPWM/relay/OFFwrites; у№7ACqualified,
requested/paired0, scheduledDelta0. Это софтсчётчики, не электрическиеосциллограммы,
не камерная/тепловая/акустическая сертификация. Физическаяпробамерцанияещёпредстоит.

Свежие данные/запросы/ответы/образы/хеши/newboot/сравнения/финал: приватный
`Theatre-console-backups/ota-fleet-087-20261121-165938/`,700/600. HTTPне даётfullflash/
NVSдамп или runningflashreadbackSHA: проверен SHAотправленногообраза, результатOTA,
новаяверсия/boot и побайтныйUI; не утверждается независимый readbackвсейflash.
Неназначенный84:FC:E6:57:0F:AC не прошивался и не удалялся.

## UART1/7 — подтверждённая проблема общей земли, без новыхOTA

На активном шаге36 пользователь подтвердил, что мерцание1 замедлилось вслед за
400→1300ms UART. Счётчики программныхPWM/relay/OFF записей при этом стояли, bootстабилен;
на7 за~9с23CRCошибки, у исправного4 за тот же интервал23валидных ответа.
По разрешению выключен ТОЛЬКО фонUART1/7: TX/RXdelta0, безSTOP/GO/audioкоманд.
Далее пользователь обнаружил и исправил отсутствие нормальной общейGND ESP–плеер.
Это вывод пользователя о физическом подключении, не электрическая осциллограмма.

По следующему разрешению на7 возвращён poll400ms: за~12с TX31/RX310/valid31/replies31,
CRC/length0. Плеер отвечает state1/track2/volume24. Нет командыPLAY/STOP/смены сцены.
На1 модуль физически удалён как ненужный; после его reboot polling успел вернуться,
повторно выключен по согласованному назначению. Флаг остаётсяRAM-only: постоянный
«плеера нет» требует отдельной доработки; отключение нельзя обещать после перезапуска.
Первичные ответыHTTP408 означали непрошедший deadline; перед повтором UART_POLL
состояние перечитано, повторялось лишь идемпотентное значение флага, неPLAY/GO.
Приватные свидетельства: `pulse-083-live-UART-correlated-20261117-202903/`,
`uart-poll-off-1-7-083-20261117-205425/`,
`restore-poll7-ground-fixed-verified-20261117-220121/` в Theatre-console-backups.

Отдельный [8bit/2kHzPWM](PWM_CAMERA.md) подготовлен по просьбе пользователя,
но НЕ установлен. Действующие платы по-прежнему25kHz.

## OTA0.8.3 — master, helmet, диагностические1/7

После явного разрешения сервисаSTOP обновлены ровно4цели, по одному MAC/role/token-bound
POST на цель, новые token/версия/роль/MAC проверены:
- Master A0:85:E3:E2:BC:D4,1286000B SHA
  `d35450506e2df2b9b5c97e605fc8e29b97d79db3259494fe0e07549723f85386`.
- Helmet C0:4E:30:36:C6:88,1058096B SHA
  `a0ef1e4c1184eb0aa54d8ce899bfd8920f91cabfa2564107f409d98ebba947d8`.
- Прибор1 C0:4E:30:27:69:98,1120352B SHA
  `64b259e723e04c489d5687572496d6ccc784671ec2d9c1d704124711df8811a3`, UART1300мс.
- Прибор7 30:30:F9:32:02:30,1120352B SHA
  `396611676fee713a64c59235bf4f03159ab927c6cc2c0700b27a15c2dcc67873`, UART400мс.

Обычный8bit25kHz/gamma1/gray.5, NVS PWM/relay и polling=true сохранены.
Шлем saved/appliedSound/111Bstartup не менялся, новые audio.gateDb/envelopeDb/gain есть;
после автономного reboot отправлен explicitALL_OFF только для удержания serviceSTOP.
Masterотдаёт8.3scrollJS/CSS. НикакихGO/LIVEon не отправлялось, remote/остальные slave
не прошивались. Таблицы/NVS/FS оставлены на месте; свежий fullflash поHTTP недоступен.

Начальный снимок ещё содержал2пресета, пока пользователь дописывал проект. ДоSTOP
дождались стабильной последней версии, сохранили4текущих пресета(en/жен/муж/рэп) и
working в обоих draft/published банках. Автосинхронизация послеSTOP далаpublication383,
36шагов/8приборов. Непосредственно до/после каждогоOTA весьJSON и оба банка совпали;
ничего не откатывалось к более ранним снимкам. После4OTA STOP/LIVEoff и готовность8/8.

В ~21с послеOTA вSTOP:1 TX17/RX170/valid17,7 TX53/RX530/valid53, ошибокCRC0;
счётчики softwarePWM/relay1/relay2/ALL_OFF не увеличивались, новых boot не было,
resetReason3 соответствует программной перезагрузке. Это НЕ проверка мерцания под
нагрузкой: пользователю предложен тот же статический шаг36 и сравнение0.4/1.3с без видео.
Шлем:errors0,late51→52,gate−62/envelope−57…−51/gain≈1 — открытие ожидаемо при таком
уровне детектора; выход mute из-заSTOP. Физический звук/gate под нагрузкой не объявлялся проверенным.
Приватные workspace/банки/настройки/логи/образы700/600:
`~/Theatre-console-backups/ota-debug-083-20261117-194612/`.


## Helmet Wi-Fi OTA0.8.2 — C0:4E:30:36:C6:88

По разрешению пользователя обновлён только helmet0.8.1→0.8.2, первый реальный upload
через егоOTAendpoint. Один token/target/role-boundPOST, app1057488B SHA256
`22d3f7ea799dcac13787e2d97681c3551684fa5b76b88ce2b2ce72a4e9a36c5e`.
Новые token/MAC/helmet/0.8.2, OTA-supported и переключение app0x10000→0x150000
проверены; старый слот1.25MiB/table/NVS/FS не заменялись. Полныйflash поHTTP недоступен.

Дообновления явно проверены полные2пресета master:«Голос 1» и «Роботик жен» в draft
и published, selected voice-default. Сделаны отдельные копии обоих банков и workspace.
Непосредственно до/послеOTA весьJSON380 и оба банка полностью совпали, actual/savedSound
шлема совпали, defaultsSaved=true. Применялся прежний автономный старт после reboot,
никакой командыGO/сменыLIVE или глобальногоSTOP в этой операции не отправлялось.
Master/1/7/остальные платы не прошивались. I2Sпослеboot захватывает/играет одинаковое
числоframes, errors0,late2; физическое звучание не объявлялось проверенным.
Приватные JSON/банк/образ/логи700/600:
`~/Theatre-console-backups/helmet-presets-check-20261117-170327/`.


## Targeted OTA0.8.2 — приборы1 и7

По явному повторному запросу пользователя обновлены только C0:4E:30:27:69:98 (1) и
30:30:F9:32:02:30 (7) с0.7.18→0.8.2. На каждый один MAC/role/token-bound POST;
отправлен стандартный slave1118528B SHA256
`71775b597e040d17ffb6293866cc444ddd3101e4308eff2f90c07b6157a92792`.
Новые boot/token/MAC/slave/версия подтверждены,8bit25kHz/gamma1/gray.5, NVS PWM-настройки
и relayMode сохранились. UART polling=true сохранён на обоих, плеер7 не отключался.
Таблица/NVS/FS не переписывались, fullflash поHTTP недоступен; доступны свежие workspace/status.

Перед OTA разрешённый serviceSTOP; обычная автосинхронизация обновила опубликованный
план376→377. Последний377 сохранён, JSON до/после двухOTA полностью совпал.
Master/остальные slave/helmet/remote не прошивались. Оставлены STOP/LIVEoff, GO не отправлялся.
Оба новых узла подтвердили plan377/ready; общая готовность7/8 из-за offline helmet.

ПослеOTA ~15с heartbeat отличные, перезапусков/сетевых ошибок нет; UART1:38valid/38TX,
UART7:37valid/37TX, ошибокCRC0 на обоих. Важно: на7 уже на старой прошивке ПОСЛЕ STOP
наблюдались6valid/6TX безCRCошибок. ДоSTOP под активной сценой была повреждённая RXсвязь.
Поэтому физическое мерцание НЕ объявлено исправленным новой прошивкой; проверка при той
же нагрузке/сцене ещё нужна. Фото/видео пользователь не предоставляет.
Приватные backup/log/result700/600:
`~/Theatre-console-backups/ota-slaves-1-7-082-20261117-164925/`.


## USB slave0.8.2 — C0:4E:30:29:F4:8C, БЕЗ бэкапа по запросу

Подключённая плата оказалась прежним ACbench0.7.13-test.3 (armed=false), не master/helmet.
По запросу пользователя заменена обычным slave0.8.2: app SHA256
`71775b597e040d17ffb6293866cc444ddd3101e4308eff2f90c07b6157a92792`.
Factory/noOTA разметка(nvs6000/phy_init/factory/FATvfs) несовместима с обычным хранением
сцен; установлены штатные bootloader/table/two-slotOTA/app, очищены NVS и новый storage
только этой платы. Бэкап старой прошивки/NVS/FS не снимался по явному указанию;
малые table/OTA участки прочитаны только для проверки разметки временно и удалены.
ROMMAC/16MB проверены до записи; digest app/table/OTAdata и новый runtimeMAC/slave/0.8.2
проверены. Стандартные8bit/25kHz/gamma1/gray.5; PWM0/relayOFF при проверке, fault0.
Позднее подтверждено подключениеYOUR_WIFI_SSID, IP192.168.50.248. НикакихGO/привязок/изменений
партитуры или прошивок других плат не отправлялось. Эта плата больше не standalone ACtest.
Только приватные логи/результат, НЕ flash backup:
`~/Theatre-console-deployments/usb-slave-NO-BACKUP-C04E3029F48C-20261116-181718/`.


## Master-only OTA0.8.2 — A0:85:E3:E2:BC:D4

По явному разрешению пользователя обновлён только master0.8.0→0.8.2.
Отправлен app1285168B SHA256
`4650e790d8baabc33e36c063f5badc00dd0cad723c847b8b04b88208be1db27f`,
один target/MAC/role/token-bound multipart POST, HTTP200UpdateOK, новая версия/роль/MAC
и boot token проверены; реально отданный gzipJS содержит новые group-drag элементы.

Сначала обнаружены продолжающиеся правки аудио шага24: до стабильного снимка STOP/OTA
не отправлялись. После разрешённогоSTOP старый браузер автоматически синхронизировал
неизменённый draft изpublication321→322. Это состояние отдельно сохранено, не откатывалось.
Перед фактическим единственнымOTA повторно сохранён свежий322:35шагов/8приборов;
JSON до/после полностью совпал. После reboot/повторной проверки running=false/LIVE=false,
никакогоGO/возвратаLIVE не отправлялось.

ЧерезHTTP свежий full-flash/NVSdump недоступен; сохранены доступные status/workspace,
NVS/table/FS оставлены на месте, обновлялось только приложение.
Slave/helmet/PWM не прошивались. Контрольные №1 C04E30276998 и №7 F9320230 остались0.7.18,
опросUART на обоих false (RAM-only, не доказательство постоянного сохранения).
Приватная папка700/файлы600:
`~/Theatre-console-backups/ota-master-082-20261116-142937/`.


## USB helmet0.8.1 с OTA — C0:4E:30:36:C6:88

По явному запросу пользователя установлен0.8.1,1057488B SHA
`fd7ca359b9a79a242183a735edab127d4e518a38a5f7d5aff3d10c16ac5f2b0d`.
Новый app-only USB helper: explicitMAC, полный16MBbackup, текущий app0 выбран поOTAdata,
образverify успешен; bootloader/table/NVS/OTAdata до reboot побайтно прежние, FSнеписалась.
RuntimeMAC/helmet/0.8.1/новый boot/token/appAddress/OTAavailable проверены поUSB.
HTTP192.168.50.21/api/status и реальнаяOTAстраница проверены; правильные MAC/role/версия,
исходные savedSound до/после совпали. Это НЕ физический тест upload поWiFi или звука.
Master/slave/PWM не менялись. Приватные логи/полныйflash/до-после700/600:
`~/Theatre-console-backups/usb-helmet-ota-C04E3036C688-20261114-142245/`.


## Master0.8.0 установлен — A0:85:E3:E2:BC:D4

Пользователь разрешил обновить только master, PWM отложен до0.8.1.
Свежие status/workspace сохранены, авторизованный serviceSTOP, один target-bound OTA.
App1283504B SHA `13a1bb570e7b24021aaeaedb4acf60021ed460b16bc02323f7b47bfcfa09c641`,
source8a81c732811f62bae7458dc35ba1b56923ed82bf. MAC/master/0.8.0/новый boot-token
проверены; HTTP200UpdateOK. Содержимое JSON партитуры227 (draft и published) совпало
до/после. Привязки не менялись. Проверены реально отдаваемые JS assets0.8 с QuickEditor
и подсказкой. STOP удержан, GO не отправлялся. Slaves/helmet/remote не прошивались,
PWM и экспериментальные сборки не применялись.
Логи/образ/свежая партитура700/600:
`~/Theatre-console-backups/ota-master-080-20261113-223641/`.
Полный свежий flash поHTTP недоступен; OTA не заменяет разметку/NVS/FS.


## Онлайн slave0.7.18 — три обычных исполнителя

Пользователь явно разрешил все обычные слейвы, которые сейчас в сети.
Обнаружены и обновлены последовательно с0.7.11 до0.7.18:
- 30:30:F9:31:19:C4 /192.168.50.55;
- C0:4E:30:27:69:98 /192.168.50.33;
- EC:DA:3B:56:F2:CC /192.168.50.197.

Slave app1117600B SHA `b48ba2da8f1e2fe72ebc94eda4bd7e167cc1f885bb8f844e6a87da82930683f4`.
Перед каждым OTA свежий MAC/role/token, masterLIVE/runningfalse, snapshots настроек.
Для каждого200UpdateOK, новый boot-token/MAC/slave/0.7.18, сохранённые8PWMlevels,
инверсия, relayMode/pins проверены; master видит firmware0.7.18/P16/score12 и
sceneStatusready/scoreVersion119. Workspace до/после равен целиком, привязок/GO/LIVE
не меняли. Автоматическая синхронизация существующего плана119 штатно прошла безGO.
Master/helmet/remote повторно не писались. DC:DA:0C:43:C3:C0,84:FC:E6:57:0F:AC,
30:30:F9:32:02:30,74:4D:BD:73:0A:A4 остались offline в master и не обновлялись.
Полный свежий flash по HTTP недоступен: OTA сохраняет FS/NVS/разметку на месте;
доступные настройки и оба draft/published сохранены отдельно до записи.

Первый postcheck ошибочно ожидал pwmHz25000 при OFF, остановил скрипт после
успешной записи. ESP32core3.3.11 ledcReadFreq() явно возвращает0 при duty0 (строки408–417).
Проверку исправили, первый MAC подтвердили без повторной записи и продолжили двумя.
Это не отказ PWM: source PWM_HZ25000; физическую частоту не измеряли и выходы не
включали ради теста. На старой0.7.11 эти же платы также отдавали pwmHz0 при OFF.
Логи/бинарник/настройки/workspace/исходники700/600:
`~/Theatre-console-backups/ota-online-slaves-0718-20261113-123741/`.
Свет/реле/MP3/AC физически не тестировались.


## OTA master0.7.18 — A0:85:E3:E2:BC:D4, исправлено распознавание P/17

Пользователь явно запросил обновление master: шлем исчезал в picker типаhelmet.
Read-only preflight подтвердил master0.7.11 на192.168.50.119 и helmet0.7.18,
ошибочно role=slave/localScoreSupported=false. LIVE/running=false.
До OTA свежий полный JSON workspace: draft7scenes, публикация119, все bindings;
fixtureР имеет profilehelmet, macnull. Свежий полный flash по HTTP недоступен;
не подменять его исходниками: OTA сохраняет текущие FS/NVS/таблицу на месте.
Master1277424B SHA `131f0524033be945f40141dc6ad81efce9a5fdaa492cf3fae10bb05002f92375`.
Только один POST/update с targetMAC/role/token,200 Update OK, новый boot-token,
masterMAC/role/version0.7.18 проверены. Workspace draft+published и весь JSON равны
до/после OTA и после read-only browser. Никаких GO/LIVE/новых bindings/публикаций.
На живом master браузер проверил Приборы→Р: typehelmet, picker содержит
`Ведомый8 ·192.168.50.21 ·0.7.18`, valueC0:4E:30:36:C6:88. Rolehelmet,
firmware0.7.18, localScoreSupported=true. Все browser POST были заблокированы.
Сначала browser открывал диалог раньше гидратации workspace и видел default10;
повтор с ожиданиемv119/Р подтвердил настоящий сохранённый fixture без изменений.
После OTA голос шлема enabled=false (сервисный STOP при обновлении master),
автоматического восстановления не делали: явная новая команда или reboothelmet.
Обычные slave0.7.11 остались без изменений и несовместимы со score12 до отдельного
разрешённого обновления; пользователь предупреждён до OTA. Не запускать спектакль.
При первом SDK build обнаружена устаревшая forward declaration mountShowFS();
заменена ранним include show_filesystem.h с действительной сигнатурой label.
Успешный повторный production build; это исправление только master-source.
Логи/бинарник/свежие оба workspace/скрин/исходники700/600:
`~/Theatre-console-backups/ota-master-0718-A085E3E2BCD4-20261113-121748/`.


## USB helmet0.7.18 — C0:4E:30:36:C6:88, рабочий профиль из5.9

Пользователь сообщил, что на старой5.9 микрофон работает, железо/лента настроены,
и явно запросил актуальный helmet. До записи serial5.9 dirty=false: chip154Гц,
gain0.8/volume1/gate−42, preset0, bankCRC2481115094. MAC проверена повторно.
Полный16MB backup SHA `3c3c8d5545d5a7fc592786626abb90fb4a586aa5596eaf1122873070f056ea39`.
Записан только app0x10000,1011472B SHA
`11cac5402476885ed18541e12f40d1aaa89d964ef7a5e8032695ec954e826c35`.
Verify-flash успешен; bootloader/table/OTAdata не менялись, NVS не стиралась.
Явной USB-командой L скопирован активный валидный старый звук в helmet-start,
не весь банк в проект master. Diagnostic=false, нормальный chip154/gain0.8/volume1/gate−42.
После контрольного reboot новый boot, те же сохранённые параметры, autonomous=true;
RX=TX1032960, errors0/late0 в снимке. Слышимость/свет пользователь проверяет отдельно.
Таблица побайтно прежняя. NVS bank_v1 состоит из двух chunks128/129 на разных страницах:
read-only offline parser собрал их, bank_v1 И bank_backup совпали полностью до/после,
CRC2481115094 сохранён. Простое contiguous-сканирование сначала не нашло split blob;
это не было потерей банка, повторного upload не делали.
Логи/полный flash/исходники/бинарник700/600:
`~/Theatre-console-backups/usb-helmet-current-C04E3036C688-20261113-115114/`.
Другие платы/привязки/спектакль не менялись. Архив0.7.17 сохранён отдельно.


## USB helmet0.7.16 — C0:4E:30:36:C6:88, автономный старт

Пользователь разрешил прошивку подключённого шлема и изменил bootstrap: включаться
без master с сохранёнными настройками, один общий звук для всех сцен.

- До записи serial показал Robot Voice5.9, saved/dirty=false, preset1bodj,
  ring349Гц/gain0.9/volume1. MAC проверена esptool, S3rev0.2/16MB/PSRAM8MB.
- Полный flash16MB сохранён SHA256
  `a4043d3ce7b905670917533cf21d9eb038142627a2db268cbedb912c740ef747`.
- Только app0x10000,1009792B SHA256
  `f6c4f74b9af9829f3ca01f5abe6a513aeae0f4ac734af7e66344b551ff3c40ce`.
  Esptool verify-flash digest успешен. Bootloader/partition table/OTAdata не писались,
  полного erase/NVS erase не было. Старый app0x140000 достаточен; пустой spiffs
  0x290000/0x160000 используется LittleFS без смены таблицы.
- После boot MAC/role helmet/version0.7.16 подтверждены. connected=false/masterReady=false,
  standalone=true, step=null, audio enabled/ready, исходный349/0.9/1 восстановлен.
  Factory свет waiting/blue10%/60точек на47; последующие принятые настройки сохраняются.
- STOP держал OFF6с без master, не включал fallback. После контрольного reset новый
  boot/token, defaultsSource=helmet-start/defaults, writes0, те же звук/свет включились.
  Audio captured=played1031936, errors0/late0 на втором boot; не доказательство слышимости.
- Таблица после записи совпала побайтно; CRC1397020606 старого полного voice bank
  найдена валидной до/после. Новый startup-record имеет отдельный namespace.
- Приватные логи/16MB backup/result700/600:
  `~/Theatre-console-backups/usb-helmet-C04E3036C688-20261112-173307/`.
  Настройки/партитуры других плат, сети и привязки не менялись.


## Новая USB-плата → master0.7.13 — 84:FC:E6:66:F3:8C

Пользователь явно разрешил роль master, затем подтвердил новую плату и замену
старой разметки/данных. Это **дополнительная плата**, не прежний masterA0:85:E3:E2:BC:D4.
MAC проверена esptool перед записью; ESP32-S3 rev0.2/16MB/PSRAM8MB.

- До изменений сохранён полный16MB flash, SHA256
  `d73b6dd5a15e469a30629798664fb6e9e5c0ee30ef2679d75858d97a72b6fb15`.
  Старая сторонняя factory/VFS-разметка, app build2022/IDF4.4.1, не Theatre.
- Записаны bootloader/console partitions/otadata/master0.7.13, app1267600B SHA256
  `dd60d490ce1448fe7911dff263c9f0c50454a27604558aefa5ed54ae1033c2bd`.
  Инициализирован новый NVS0x9000/0x5000, полного erase-flash не было.
- Esptool write hashes и отдельный verify-flash digest успешны. Дополнительный
  raw-readback оборвался по USB; вместо повторного upload выполнен verify-flash.
- Serial подтвердил MAC/role=master/version0.7.13, flash16MB/PSRAM8MB и новую
  загрузку/token после reset. Host `stage-master-66f38c`; YOUR_WIFI_SSID ещё не подключена,
  connectedfalse/IP0.0.0.0. HTTP/Wi-Fi доступ пока не подтверждён.
- Партитура не импортировалась, sceneVersion0/планы0, LIVEoff/runningfalse.
  GO и команды другим платам не отправлялись; старый master/партитура/привязки
  не менялись. Native/CI относятся к source c22e4cc.
- Приватное доказательство700/600:
  `~/Theatre-console-backups/usb-master-84FCE666F38C-20261112-010911/`.

Ниже — исторические установки основного комплекта, не новая проверка его онлайна.

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

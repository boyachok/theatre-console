# 0.7.2-recovery.1 — минимальное восстановление master (НЕ записано)

Отдельная ветка `recovery/master-072-stack` от tagv0.7.2. Не заменяет новую
main0.7.5 с индексами сцен: здесь намеренно сохранены score6/214B RUN,
совместимость с записанными slave0.7.2 и прежние GPIO/интерфейс.

Исправление: вместо ROM `tinfl_decompress_mem_to_mem()`, который держит ~11KB
состояния на8KB стеке loopTask, вызывается низкоуровневый tinfl_decompress со
состоянием в PSRAM (`console/workspace_inflate.h`). Формат workspace, CRC,
LittleFS, NVS, публикация и партитура не меняются. Версия веба/прошивки совпадает.
Добавлены ROM-API allocation/error regression и GCC packed-array test fix.
Новые фейдеры/общий fade/изменение полярности реле сюда НЕ включены.

Плата: masterA0:85:E3:E2:BC:D4. Полная исходная flash и экспорт партитуры сохранены
вне Git: `~/Theatre-console-backups/master-A085E3E2BCD4-20261105-231034/`.
CRC партитуры исправна:8шагов,2прибора,публикацияv1. Это не повреждение памяти,
которое нужно лечить erase-flash.

```sh
python3 tools/release.py --check
./tests/run.sh
./deploy build master
```

Master app SHA256 подготовленной сборки:
`ceccfe1ee25bfc5c7a0ae1067e862a618ea4700354c0301f538024b1e1d99429`.
Повторная сборка может изменить hash. Запись только после отдельного выбора
пользователем этого варианта, проверки MAC, с сохранением NVS/storage.
**Post-boot на реальной плате ещё не проверен; аппаратное восстановление не объявлено.**

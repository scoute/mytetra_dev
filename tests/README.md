# Тесты MyTetra

12 сьютов, 126 проверок, QtTest, сборка через qmake. Правило простое:

- **Новая фича — новый тест.** Нет теста — фича не готова.
- **Багфикс — регрессионный тест**, воспроизводящий баг до правки.
- **Покрытие не ронять.** Замер — командой из раздела «Покрытие».

## Очереди

- Первая (`testOrderedMap`, `testSortHelper`, `testHtmlHelper`,
  `testUniqueIdHelper`, `testPbkdf2Qt`, `testRc5Simple`,
  `testCryptService`, `testFixedParameters`): чистые модули без глобалов,
  дисплея не требуют.
- Вторая (`testWalkHistory`, `testAppConfig`): модули с глобалами
  приложения. Линкуются со всеми `.o` приложения, см. `mytetra_harness.pri`.
  Прогон в `offscreen` (включается сам в `main()` теста).
- Третья (`testKnowTree`, `testTreeScreen`): дерево знаний на фикстуре
  `fixtures/knowtree`, тоже `offscreen`.

## Сборка и прогон

Только вне исходников. Qt 5.15.2:

```bash
Q=/media/user/data/Qt_installed/5.15.2/gcc_64
mkdir -p /tmp/mt-tests && cd /tmp/mt-tests
$Q/bin/qmake /путь/к/mytetra-dev/tests/tests.pro
make -j$(nproc)
make check
```

Для второй и третьей очередей **сначала** собрать приложение
(нужны готовые `.o` для линковки):

```bash
mkdir -p /tmp/mt-app && cd /tmp/mt-app
$Q/bin/qmake /путь/к/mytetra-dev/mytetra.pro
make -j$(nproc)
```

а тесты конфигурировать с указанием сборки:

```bash
$Q/bin/qmake APP_BUILD_DIR=/tmp/mt-app/app /путь/к/mytetra-dev/tests/tests.pro
```

(`APP_BUILD_DIR` по умолчанию `/tmp/opencode/mt-app-build/app`.
После пересборки приложения тесты перелинкуются сами.)

## Изоляция (важно)

Harness-тесты сами кладут `conf.ini` и фикстуру рядом с бинарем
и проверяют в `initTestCase()`, что рабочая директория совпадает
с директорией бинаря. Если ассёрт упал — тест чинить, а не обходить:
иначе он полезет в данные пользователя (`~/.mytetra/`).
Выход из harness-тестов — только через `_exit()`, не `return`.

## Покрытие

```bash
# Приложение с инструментацией
mkdir -p /tmp/mt-app-cov && cd /tmp/mt-app-cov
$Q/bin/qmake CONFIG+=debug QMAKE_CXXFLAGS+=--coverage QMAKE_LFLAGS+=--coverage \
  /путь/к/mytetra-dev/mytetra.pro && make -j$(nproc)
# Тесты против него
mkdir -p /tmp/mt-tests-cov && cd /tmp/mt-tests-cov
$Q/bin/qmake APP_BUILD_DIR=/tmp/mt-app-cov/app CONFIG+=debug \
  QMAKE_CXXFLAGS+=--coverage QMAKE_LFLAGS+=--coverage \
  /путь/к/mytetra-dev/tests/tests.pro && make -j$(nproc) && make check
# Замер (пример: дерево знаний)
gcovr -r /путь/к/mytetra-dev \
  --object-directory /tmp/mt-app-cov/app/build \
  --filter 'app/src/models/tree/.*' \
  --exclude '.*moc_.*' --gcov-ignore-errors=no_working_dir_found
```

## Как добавить тест

1. Скопировать каталог похожего сьюта (имя каталога = имя `.pro`).
2. Добавить каталог в `SUBDIRS` в `tests.pro`.
3. Нужны глобалы приложения — подключить `include(../mytetra_harness.pri)`
   и определить глобалы в `.cpp` один в один как в `app/src/main.cpp`.
4. Нужна база — подключить `include(../knowtree_fixture.pri)`.
5. Стиль кода — по разделу «Соглашение о кодировании» в `README.md`:
   пробелы, скобки с новой строки, комментарии к методам.

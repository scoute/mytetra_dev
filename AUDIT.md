# Аудит MyTetra: ветка `develop-v1-mytetra`

Дата: 2026-10-22  
База: `cd14b7da` (`1.149.1`, `[chore][version=>1.149.1]`)  
Рабочая ветка аудита: `fix/259_audit_hardening`  
Объём: ~103 тыс. строк C++/H, 418 файлов в `app/src`, Qt 5.15 / qmake.

Аудит ручной: криптография, импорт/экспорт, MCP, буфер обмена и загрузка
из сети, конфиги и права файлов, обработка ошибок. Дополнительно собраны
метрики по архитектуре и проверены сценарии headless-пробами.

---

## 1. Кратко

Найдено и исправлено в ветке:

| # | Проблема | Класс | Статус |
|---|----------|-------|--------|
| 1 | Path traversal в импорте Telegram (чтение и запись вне каталога экспорта) | Critical | исправлено |
| 2 | Symlink traversal в импорте/экспорте ветки | High | исправлено |
| 3 | Игнорировались все ошибки TLS; после ужесточения `NoPeerCertificate` ломал загрузку | High | исправлено (фильтр) |
| 4 | Потеря мастер-соли XChaCha при расшифровке (молчаливая перегенерация) | High | исправлено |
| 5 | Выход за границу буфера в RC5 на обрезанном блобе | High | исправлено |
| 6 | Расшифровка с неверным ключом затирала данные пустотой | High | частично исправлено |
| 7 | `file://` в Web Clipper | — | возвращено как фича, ограничено лимитом |
| 8 | Слабый источник соли проверочного пароля (`rand()`) | Medium | исправлено |
| 9 | Неатомарная перезапись файла при шифровании/расшифровке | Medium | исправлено |
| 10 | Обрезание расшифрованной строки по NUL | Medium | исправлено |
| 11 | `criticalError`/`exit(1)` на ошибках путей экспорта и импорта | Medium | исправлено |
| 12 | Права на `database.ini`, журнал действий, копии файлов | Medium | исправлено |

Осталось в плане (не исправлено в этой ветке): слабый KDF старого пароля,
отсутствие аутентификации в RC5, открытый текст расшифрованных аттачей
в корзине, произвольные пути MCP export/import, отсутствие тестов и CI,
архитектурные God-классы и глобальные синглтоны.

---

## 2. Безопасность

### 2.1. Импорт Telegram: path traversal (Critical) — исправлено

Файл: `app/src/libraries/telegram/TelegramImport.cpp`.

До исправления:

- `media.srcPath = exportDir.absoluteFilePath(href)` — `href` из чужого
  `messages.html`/`messages.json` мог быть `../../../../home/user/.ssh/id_rsa`
  или `file:///etc/passwd`; файл попадал в заметку/аттач.
- `media.fileName` из `<div class="title bold">` или поля `file_name`
  не очищался; имя `../../.bashrc` уводило запись за каталог заметки
  (перезапись/создание файлов вне базы).
- `takeUniqueName()` доверял имени и не убирал разделители.

Что сделано:

- `safeExportMediaPath()`: только относительный путь, без схемы и
  абсолютного корня; лексическая и каноническая проверка нахождения
  внутри каталога экспорта (симлинк наружу отсекается).
- `safeMediaFileName()`: базовое имя, без `/`, `\`, NUL и управляющих
  символов, с ограничением длины; применяется при разборе и повторно
  в `takeUniqueName()`.
- Перед записью в каталог заметки — ещё одна проверка `isPathInside()`.
- Ссылки в тексте заметки (`text_link`, `link`, `email`, `phone` и HTML
  `<a href>`) пропускаются, если схема не `http(s)`, `mailto`, `tel`,
  `tg` или `#`. Иначе `javascript:`/`data:`/`file:` могли стать
  кликабельными действиями в редакторе.

Проверка: проба `tgprobe` на поддельном JSON/HTML — медиа с `../../etc/passwd`
не попало в список, имя `../../evil.txt` превратилось в `evil.txt`.

### 2.2. Symlink traversal при импорте/экспорте (High) — исправлено

Файлы: `libraries/helpers/DiskHelper.cpp`, `models/tree/KnowTreeModel.cpp`.

`QDir::Files` включает симлинки; `QFile::copy` шёл по ссылке. Каталог
обмена с `base/<dir>/passwd -> /etc/passwd` копировал чужие файлы в базу;
экспорт, наоборот, мог вынести файл из базы наружу.

Что сделано:

- `DiskHelper::copyDirectory()`: `QDir::NoSymLinks`, отказ для
  каталогов-симлинков, каноническая проверка источника и приёмника,
  запрет перезаписи существующих файлов, права `0600` на копии.
- `KnowTreeModel::copyImportRecordDirectories()`: канонический путь
  каталога записи обязан лежать внутри каталога обмена; приёмник —
  внутри `base` текущей базы.
- В экспорте проверяются `dir`, `file` и имена аттачей; `mkpath`
  больше не вызывает `criticalError`.

Проверка: проба `symprobe` — симлинк-файл не скопирован, симлинк-каталог
источника и приёмника отклонены.

### 2.3. TLS и `file://` в клиппере (High) — исправлено с оговорками

Файлы: `libraries/Downloader.cpp`, `libraries/Clipper.cpp`.

В исходном коде оба места вызывали `ignoreSslErrors()` на любое событие
`sslErrors`, то есть принимали и просроченный, и самоподписанный, и
подменённый сертификат. Это превращало HTTPS в канал, открытый для MITM.

Первая версия исправления отменяла загрузку на любой ошибке TLS. На
практике это сломало легитимные сайты: при возобновлении TLS-сессии
сервер не присылает сертификат повторно, и Qt выдаёт
`QSslError::NoPeerCertificate` («Сертификат не был предоставлен»),
хотя полный handshake ранее уже был проверен.

Итоговое поведение:

- `NoPeerCertificate` в единственном числе — не ошибка цепочки,
  загрузка продолжается (`ignoreSslErrors(errors)`).
- любая другая ошибка (срок, имя хоста, цепочка, самоподписанный) —
  запись в журнал и `abort()`.
- для корпоративных/самоподписанных сертификатов есть явная галочка
  «Разрешать загрузку с невалидными или самоподписанными TLS-сертификатами»
  на странице «Разное» (по умолчанию выключена, с предупреждением).
- `file://` в клиппере возвращён: клиппер запускается вручную, и вставка
  своей локальной картинки — штатный сценарий. Размер ограничен тем же
  `clipMaxImageBytes`, что и для сетевых изображений.

Остаточный риск: если сайт отдаёт картинки по HTTP, подмена возможна
по определению; это не TLS-проблема, а политика сайта.


### 2.4. XChaCha: потеря мастер-соли (High) — исправлено

Файл: `libraries/crypt/XChaChaCipherBackend.cpp`.

`masterSalt()` вызывался и при шифровании, и при расшифровке. Если
`cryptSalt` в `database.ini` потерян/пуст, расшифровка молча генерировала
новую соль и записывала её в конфиг. Старые блобы после этого не читались,
а «лечение» закреплялось на диске.

Что сделано:

- `masterSalt(bool createIfMissing)`: при расшифровке соль не создаётся.
- `sessionKek(key, createSalt)`: пустой ключ/соль — отказ, а не KDF
  от пустого пароля.
- Рабочая область Argon2 (64 МБ) затирается перед `free`.
- Убран дублирующий `extern DataBaseConfig`.
- `encDecFileSmart()` при непустом шифротексте и пустом результате
  останавливается, а не пишет пустоту.

Проверка: `hardeningprobe` — после удаления соли расшифровка возвращает
пусто, `cryptSalt` остаётся пустым; после восстановления соли старый блоб
снова читается; обрезанные блобы и неверный ключ отклоняются.

### 2.5. RC5: чтение за границей буфера (High) — исправлено

Файлы: `libraries/crypt/RC5Simple.cpp`, `RC5CipherBackend.cpp`.

`RC5_Decrypt()` читал IV и блок размера без проверки минимальной длины,
а затем делал `out.erase(begin, begin + removeBlocks*16)` и
`erase(begin+data_size, end)` без проверки границ. Обрезанный блоб
(повреждённый файл, чужой импорт) — UB/падение/порча памяти.

Что сделано:

- Проверка минимального числа блоков для форматов 1/2/3 и отказ для
  нераспознанного формата.
- `data_size` сверяется с объёмом открытых данных, а не с длиной
  шифротекста; перед `erase` — дополнительная проверка.
- `RC5CipherBackend` отклоняет ключ не 16 байт и данные < 32 байт,
  проверяет коды ошибок RC5.
- `convertByteArrayToVector`/`convertVectorToByteArray` больше не берут
  `&vec[0]` у пустого вектора.
- IV/добивка RC5 генерируются `QRandomGenerator::system()` вместо
  предсказуемого `rand()`.

Проверка: `hardeningprobe` прогоняет 201 случайный блоб разной длины без
падений; обычный RC5 roundtrip сохранён (`cryptprobe_new` — `CRYPT-OK`).

### 2.6. Тихая расшифровка пустотой (High) — частично исправлено

Файлы: `TreeItem.cpp`, `Record.cpp`, `KnowTreeModel.cpp`.

`switchToDecrypt()` ставил `crypt=0` и присваивал результат расшифровки
даже если он пустой. Для XChaCha это означало: неверный ключ/порча →
пустые поля → следующее сохранение затирает данные.

Что сделано: перед деструктивной расшифровкой полей/текста проверяется,
что непустой блоб `MTENC1` дал непустой открытый текст; иначе
`criticalError` (операция не продолжается). Это закрывает XChaCha.
Для legacy RC5 аутентификации нет: неверный ключ даёт непустой мусор,
и отличить его от текста нельзя. Полное решение — миграция на
аутентифицированный формат (см. план).

### 2.7. Слабые места, которые остались

1. **KDF пароля.** Ключ = `MD5(PBKDF2-SHA1(password, static_salt, 1000))`.
   1000 итераций и статическая соль — мало для 2026 года; проверочный
   хеш в `database.ini` позволяет офлайн-перебор. XChaCha сверху считает
   Argon2id, но вход в него уже ослаблен. Нужен версионированный KDF
   (Argon2id с параметрами в конверте/конфиге) и миграция при смене пароля.
2. **RC5 без MAC.** Целостность не проверяется, wrong-key неотличим от
   мусора. Оставить только для чтения старых баз, новые — XChaCha.
3. **Расшифрованные аттачи в корзине.** При включённой опции
   «decrypt to trash» копия расшифровывается в `trashdir` и не удаляется.
   Нужен временный каталог с `0700`, очистка при выходе/старте.
4. **MCP: произвольные пути export/import.** Агент в `--mcp-rw` может
   выгрузить ветку в любой пользовательский путь и прочитать обменник
   из любого места. Нужен allowlist каталогов (настройка) и запрет
   `tetradir`/системных путей.
5. **Нет лимитов на размер импорта.** Большой XML/HTML/JSON или
   «billion laughs» в XML — риск зависания/переполнения памяти.
6. **`criticalError` = `exit(1)`** из глубины модели/крипты. Любая
   ошибка ввода-вывода убивает приложение, а в headless MCP — процесс
   сервера. Нужны коды ошибок/исключения и откат операции.
7. **Права.** `conf.ini`/`editorconf.ini`/`mytetra.xml` в установщике
   уже 0600, но `database.ini`, созданный позже, раньше получал umask;
   журнал действий — тоже. Исправлено, но стоит централизовать
   `privateFile()`/`privateDir()`.

---

## 3. Архитектура

### 3.1. Метрики

- ~103 тыс. строк, 210 `.cpp`, 208 `.h`.
- `MainWindow.cpp` — 136 КБ, `McpServer.cpp` — 103 КБ,
  `TreeScreen.cpp` — 84 КБ, `KnowTreeModel.cpp` — 84 КБ.
- 224 вызова `find_object<T>("name")` — связность через глобальное
  дерево QObject и строковые имена.
- 195 вызовов `criticalError()` — завершение процесса как обработка
  ошибки.
- 700 `qDebug/qWarning` — отладочный вывод в релизе (частично
  управляется настройкой).
- Нет каталога тестов, CI, линтеров. Сборка — qmake + явные списки
  файлов; i18n — ручная правка `.ts`.

### 3.2. Что мешает развитию

1. **Глобальный контекст.** `mytetraConfig`, `dataBaseConfig`,
   `globalParameters`, `trashMonitoring`, `actionLogger` и др. —
   глобальные объекты в `main.cpp`. Их видят все слои; тестировать
   модуль отдельно почти невозможно.
2. **`find_object` вместо зависимостей.** Любое переименование
   `objectName` — runtime-поломка; в headless-режиме половина объектов
   `nullptr`, поэтому код обвешан проверками.
3. **God-классы.** `MainWindow` совмещает меню, команды, MCP-хендлер,
   синхронизацию, настройки; `KnowTreeModel` — DOM, шифрование,
   файловые операции, статистику и уведомления; `McpServer` — протокол,
   маршрутизацию записи, IPC и бизнес-логику.
4. **Модель данных без транзакций.** `mytetra.xml` + каталог на запись;
   импорт копирует файлы до проверки/сохранения, при ошибке остаются
   сироты; удаление — в корзину без журнала восстановления.
5. **Крипто-слой статический.** `CryptService` — набор статических
   методов, ключ в `GlobalParameters`, кэш KEK — статические переменные
   в backend. Нет времени жизни ключа, блокировки, явного `lock()`.
6. **Дублирование.** Две копии `qtSingleApplication`; собственная
   реализация PBKDF2/RC5; несколько похожих парсеров полей/тегов.
7. **Сборка.** qmake-списки вручную, нет `CMake`/пресетов, нет
   воспроизводимой CI-сборки; Qt5 и Qt6-ветки расходятся.
8. **Ошибки через диалоги.** `showMessageBox`/`criticalError` в модели
   и парсерах — UI-зависимость в бизнес-логике; headless-режим вынужден
   обходить это флагами.

### 3.3. Что сделано хорошо

- Чёткое разделение `CipherBackend`/`CryptService`, конверт `MTENC1`
  с версией и параметрами KDF.
- Атомарная запись `mytetra.xml` через `QSaveFile`, приватные права.
- MCP: read-only по умолчанию, отдельные режимы rw/delete, запись через
  живой GUI с подтверждением, история действий.
- Импорт/экспорт уже имел проверки имён (`isSafeImportFileName`) —
  аудит дополнил их симлинками и каноническими путями.
- Понятные сообщения коммитов и дисциплина версий.

---

## 4. Незавершённые функции и TODO

По `TODO.md` линии `develop-v1`:

1. Повторный импорт Telegram без удаления ветки (досинхронизация).
2. Синхронизация с мобильной версией (`mytetra.xml` + `base`).
3. Транскрипция голосовых через локальный whisper.cpp.
4. Переключатель баз при 2+ подключённых (нужно отображаемое имя базы).
5. Qt6-форк Telegram для Windows 7.

По коду (выборочно):

- `RecordTableData.cpp` — TODO: удаление приаттаченных файлов при
  удалении записи, шифрование имён/содержимого аттачей при
  шифровании ветки.
- `KnowTreeModel::exportBranchToDirectory` — TODO: выгрузка через
  `QXmlStreamWriter` вместо DOM (память на больших ветках).
- `MainWindow` — TODO: функционал в областях, помеченных «сделать
  функционал».
- `EditorToolBar`, `TypefaceFormatter`, `RecordTableView` — мелкие
  архитектурные TODO (дублирование, владение объектами, drag-n-drop).

Функции, которых не хватает с точки зрения безопасности/эксплуатации:

- восстановление после смены пароля (прогресс + откат при сбое);
- проверка целостности базы (контрольные суммы/бэкап перед миграцией);
- менеджер ключей/recovery key;
- очистка временных расшифрованных файлов;
- ограничение импорта по размеру и глубине XML;
- allowlist путей MCP;
- журнал аудита с защитой от подмены (сейчас `actionLog.txt` — простой
  XML-текст).

---

## 5. План улучшений

### P0 — сделано в `fix/259_audit_hardening`

- Закрыть path traversal в Telegram-импорте (чтение/запись/ссылки).
- Запретить симлинки в импорте/экспорте, проверять канонические пути.
- Убрать `ignoreSslErrors`, но не ломать сессии с `NoPeerCertificate`:
  фильтр только для этого кода, остальные ошибки TLS фатальны.
- Добавить opt-in галочку для невалидных/самоподписанных сертификатов
  (по умолчанию выключена).
- Вернуть `file://` в клиппере как штатную локальную вставку с лимитом.
- XChaCha: fail-closed при потере соли, затирание рабочей области.
- RC5: проверки границ, CSPRNG для IV, отказ на битых блобах.
- Не затирать данные при неудачной расшифровке (XChaCha).
- Атомарная запись при encrypt/decrypt файла.
- CSPRNG для соли проверочного пароля, `sync()` конфигов.
- Приватные права на `database.ini`, журнал, копии файлов.
- Убрать `criticalError` из путей экспорта/импорта.

### P1 — 1–2 недели (следующий unit-набор)

1. **KDF v2 + миграция.**
   - В `database.ini` хранить `kdf=argon2id`, `kdf_mem`, `kdf_passes`,
     `kdf_salt`; для старых баз — fallback `pbkdf2-sha1/1000`.
   - Новый ключ: Argon2id непосредственно от пароля (без MD5).
   - При вводе правильного старого пароля — перешифровка check-хэша и
     ключа; при смене пароля — атомарно, с бэкапом.
   - Критерий: база 1.149 открывается, после смены пароля старый пароль
     не подходит, новый работает; прерывание миграции не теряет данные.
2. **Безопасные временные файлы.**
   - `QTemporaryDir` с `0700` для просмотра расшифрованных аттачей;
     очистка при выходе и при старте; запрет `trashdir`.
3. **MCP allowlist.**
   - Настройка `mcpExportRoot`/`mcpImportRoot`; по умолчанию — только
     `tetradir/../exchange`; запрет `tetradir`, `/etc`, домашних
     секретов; проверка канонических путей.
4. **Лимиты импорта.**
   - Максимальный размер XML/HTML/JSON, число записей, глубина вложенности;
     отказ до копирования файлов.
5. **Ошибки без `exit()`.**
   - Для импорта/экспорта/крипты вернуть `Result`/`bool` + сообщение;
     `criticalError` оставить только для действительно фатальных
     состояний (повреждённый конфиг на старте).
6. **Тесты.**
   - `QTest`/CTest: crypto roundtrip, потеря соли, битые блобы, path
     traversal (`..`, symlink, absolute), Telegram HTML/JSON, MCP
     read-only/rw/delete.
   - Фаззинг импортеров (libFuzzer/AFL) и парсеров XML.

### P2 — 1–2 месяца

1. **AppContext вместо глобалов.** Один объект владеет конфигами,
   моделями, сервисами; `find_object` заменить на внедрение зависимостей.
2. **Разделить God-классы:**
   - `MainWindow` → `MainWindowUi`, `SyncController`, `McpBridge`;
   - `KnowTreeModel` → `TreeRepository` (DOM/файлы),
     `TreeCryptoService`, `TreeStats`;
   - `McpServer` → `JsonRpcTransport`, `McpRouter`, `GuiBridge`.
3. **Транзакционный импорт.** Копирование во временный каталог, проверка,
   затем атомарная замена; очистка сирот при ошибке.
4. **CI и статический анализ.** GitHub Actions: qmake build Qt5/Qt6,
   `-Wall -Wextra -Werror` для новых файлов, `cppcheck`/`clang-tidy`,
   `git diff --check`, тесты, проверка i18n.
5. **CMake-пресеты** для Linux/Windows/Android; убрать ручные списки
   файлов, сохранив qmake как fallback.
6. **Единый слой приватных файлов.** `privateFile(path)` (0600) и
   `privateDir(path)` (0700) для конфигов, журналов, корзины, временных
   файлов.

### P3 — продукт

1. Мобильная синхронизация: файловый обмен + разрешение конфликтов,
   тест на шифрованных ветках.
2. Whisper: локальный whisper.cpp, менеджер моделей, фоновый воркер.
3. Имена баз и переключатель при 2+ базах.
4. Повторный импорт Telegram (дельта по `message.id`).
5. Qt6-линия: оценить форк Qt для Windows 7, но не блокировать
   безопасность ради Win7.

---

## 6. Проверка

Сборка (Qt 5.15.2, debug):

```sh
export PATH=/media/user/m2data/Qt_deb11/5.15.2/gcc_64/bin:$PATH
cd build/Desktop_Qt_5_15_2_GCC_64bit/app
make -j12
```

Результат: собрано `bin/mytetra`, новых ошибок нет; единственное
предупреждение — существовавший `QSet::toList()` в `KnowTreeModel`.

Headless-пробы (в `/tmp/opencode`, не входят в репозиторий):

| Проба | Результат |
|-------|-----------|
| `cryptprobe_new` | `CRYPT-OK` (RC5/XChaCha roundtrip, detect, file) |
| `hardeningprobe` | `HARDENING-OK` (потеря соли, битые блобы, wrong key) |
| `tgprobe` | `TG-OK` (traversal в JSON/HTML отклонён, имя очищено) |
| `symprobe` | `SYM-OK` (симлинки импорта/экспорта отклонены) |

---

## 7. Ветка и дальнейшие шаги

- Ветка: `fix/259_audit_hardening`, один commit.
- В `develop-v1-mytetra` не влита, версия не поднята: по `AGENTS.md`
  merge/push в общие линии — только по явной команде.
- Ветка: `fix/259_audit_hardening`, один commit.
- В `develop-v1-mytetra` не влита, версия не поднята: по `AGENTS.md`
  merge/push в общие линии — только по явной команде.
- Если изменения приняты: `merge --no-ff` + отдельный
  `[chore][version=>1.149.2]`.
- Перед merge рекомендуется прогнать P1-тесты на реальной базе
  (открытие, смена пароля, экспорт/импорт ветки, Telegram-импорт).

---

# Аудит MyTetra: ветка `develop-v1-mytetra` (дополнение 2026-11-06)

Дата: 2026-11-06
База: `7065b5a7` (`1.227.1`, `[chore][version=>1.227.1]`)
Объём: `app/app.pro` + `app/src` (~250 файлов), Qt 5.15 / qmake, PCH `stable.h`.

Метод: ручной разбор ядра (модели, хранение, крипта, конфиги, индексы),
wyedit-редактора, MCP/IPC, сборки, тестов и `mytetra_ru.ts`; скриптовые
сверки `app.pro`/`.qrc` и переводов.

## 1. Критические / High (падения, потеря данных, безопасность)

1. `AppConfigTables.cpp:17-107` — безусловный `table.at(i*3)` на 200 итераций
   без проверки размера/терминатора `"0","0","0"`. Неизвестный ключ =
   `ASSERT failure in QList::at`. Плюс `AppConfigUpdater.cpp:269` делает
   `conf->clear()` и пишет только ключи из таблицы: все новые ключи
   (`attachNameMode`, `notePaperColor`, `treeToolbarOrder`,
   `voiceNoteFormat` и др. из `AppConfigUi/Storage/Services`) молча
   стираются при следующем бампе версии `conf.ini`.
2. `KnowTreeModel::importBranchFromDirectory:706-765` — после
   `setupModelData()` не выставляются `m_idIndexDirty`/`invalidateStats()`,
   `getItemById(importItemId)` ищет в устаревшем индексе, `nullptr` ->
   `switchToEncrypt()` = SIGSEGV при импорте в шифрованную ветку.
   `KnowTreeModel::moveBranch:1250-1314` (+ `KnowTreeView::dropBranch`,
   `TreeScreenClipboard::pasteCutBranch`) не сверяет `crypt` родителя/
   переносимого и не требует ключ: открытая ветка внутри шифрованной,
   сироты при `reEncrypt`.
3. `Record::switchToEncryptAndSaveLite:765-788` шифрует файлы, но не делает
   `ftsTextIndex()->remove(id)`: открытый текст остаётся в
   `mytetra-fts5.db` (`fts_text`/`fts_plain`) до пересборки.
4. `OpusFeedDevice::readData (OpusPlayer.cpp:24-46)` считает
   `maxFrames = maxSize/sizeof(short)` без деления на `channels()`,
   а `OpusFileReader::readPcm` пишет `copy` сэмплов: стерео `.opus` =
   heap buffer overflow до `2*maxSize`.
5. `QtLocalPeer::receiveConnection (qtlocalpeer.cpp:180-185)`:
   `while(bytesAvailable()<4) waitForReadyRead();` без таймаута = вечный
   цикл GUI-потока при обрыве клиента; `remaining` без лимита = OOM.
   `McpServerGui.cpp:60 socketNameForId` без `toLower()` на Windows, а
   `QtLocalPeer` с `toLower()` -> разные `qChecksum`, MCP на Windows
   всегда `GUI_NOT_RUNNING` и пишет мимо живого GUI.
6. `EditorShowTextDispatcher.cpp:14-35,160-220` живёт в своём `QThread`,
   слоты трогают `KnowTreeModel` и `QWidget::close()` из рабочего потока
   (запрещено Qt); деструктор инвертирован:
   `if(mThread->wait(1000)) terminate();` вместо `if(!wait(...))`.
7. `Record::setAttachTable:485-491`, `AttachTableData:167-189` не
   переставляют `setRecord/updateAttachTableBackLink/setParentTable` ->
   висячие `parentTable/record`. `AttachTableController::addAttach:368-381`
   вызывает `attach.encrypt()` до проверки `result`.
8. Причина SIGSEGV headless-тестов на выходе: `AppConfig::~AppConfig:36-43`
   и `DataBaseConfig::~DataBaseConfig:29-36` делают `m_conf->sync()` /
   `conf->sync()` при `m_conf==nullptr` (тесты зануляют `m_conf` в
   `cleanupTestCase`, `m_isInit` остаётся `true`).

## 2. Средние (логика, производительность, целостность)

- `CryptMigrate::reencryptAll:15-65` собирает вложенные `crypt=="1"` и
  гоняет `switchToDecrypt/Encrypt` по каждому: глубина D = D проходов
  по диску; `out.records` всегда 0.
- `KnowTreeModel::deleteItemsByModelIndexList:2097-2185` сортирует только
  по глубине пути: удаление сиблингов 0 и 1 сносит строку 1 дважды (не та
  ветка + `TreeItem*` с затёртыми записями); нет `invalidateStats()`.
- `KnownBasesConfig::renumSections:146-189` без числовой сортировки:
  `childGroups()` даёт `num10` перед `num2`, при 10+ базах
  `renameSection(num10->num2)` затирает `num2`.
- `Record::replaceInternalReferenceByTranslateTable:910-959` клеит
  `readLine()` без `\n` (ломает `<pre>`), без `setCodec("UTF-8")`,
  пишет через `QFile` вместо атомарной записи.
- `McpTools::applyUpdateNote:342-430` делает
  `switchToFat/pushFatAttributes/switchToLite` ради смены текста: двойная
  перезапись аттачей, `criticalError` при битом аттаче; `applyRenameTag/
  applyDeleteTag` без `invalidateStats()`. `McpGuiBridge:256-385`
  не сбрасывает `MetaEditor` при delete/update открытой записи.
- `Favorites:220-228` (`m_sidecarLoaded`) и `BacklinkIndex::isFresh:174-193`
  (только счётчик записей) не хранят `tetradir`: переключение базы
  отдаёт чужие звёзды/бэклинки и затирает чужой `favorites.xml`.
- `Editor.cpp:74-91,298` — нет `delete audioFormatter` (утечка на каждый
  `AddNewRecord`/detached-окно; удалять до `delete textArea`).
  `TreeItem::removeAllChildren:389-393` удаляет половину
  (`takeAt(0)` + растущий `i`). `TreeModel::getIndexRecurse:181`
  сравнивает с `modelIndex.row()` вместо `rowCount()`.
- Ключи в памяти: `GlobalParameters::passwordHash` дублирует
  `CryptKeyHolder::m_key` и не зануляется; `XChaChaCipherBackend:228-267`
  не `crypto_wipe(kek)` в `decryptBytes`, статик `sessionKekBuf/Key`
  живёт после сброса пароля; `RC5Simple::~RC5Simple:146-151` не трёт
  `rc5_s[26]`.
- `RecordTableController:314-325` отдаёт `selectRow(workPos)` вместо
  `convertSourcePosToProxyPos`; `RecordTableView:412-418` кликает по
  старому `proxyIndex` после пересортировки; `Attach::renameFile:886`
  вместо `getExistingInnerFileFullName()`.
- `TelegramImport.cpp:189-242`: `telegramTextToHtml` для `isString()`
  возвращается до замены `\n-><br/>` (многострочные без форматирования
  склеиваются); `href=text.toHtmlEscaped()` повторно экранирует `&`.

## 3. Низкие / гигиена / i18n

- Дубли `parseWinHotkey/sendCopyKeys/ClipperWinFilter` в
  `ClipperParse.cpp:108-245` и `ClipperHotkey.cpp:109-251` (в Parse не
  используются, ODR-риск на Windows).
- `static`-переменные в рекурсиях `TreeItem/KnowTreeModel`
  (`getAllChildrenPathAsFieldRecurse`, `getAllRecordCountRecurse`,
  `isRecordDirExistsRecurse`, `isContainsCryptBranchesRecurse`,
  `isContainsBlockRecordsRecurse`); `getAllRecordCount` может брать
  готовый `subtreeRecordCount()`.
- `TrashMonitoring.h:32-47` — `unsigned int` для размеров: переполнение
  при корзине >4 ГБ, нужен `qint64`.
- `HtmlHelper.cpp:11-28` не экранирует `&`.
- `IconSelectDialog.h:50` — `QComboBox` вместо `MtComboBox` (пустые
  квадраты в тёмной теме).
- `mytetra_ru.ts`: 1532/0/0 (ru 100%), но несколько новых `tr()` нет в
  `.ts`: `FtsTextIndex "Index rebuild is in progress"`,
  `ReferenceFormatter "Unable to preview..."`,
  `AppConfigPage_Crypt "Re-encryption canceled..."`,
  7x `KnowTreeModel "Export/Import error: ..."`. Контекст `tr()` =
  класс метода (проверять `EditorImageProperties/Real image size`,
  `MainWindowMcp`, `TagsPanel`, `TreeScreen*`, `TypefaceFormatter*`).
- Мусор в репо: `app/iZIKtb.json` (34 КБ) — удалить, не коммитить.

## 4. План исправлений (юниты fix/NNN_*)

- `fix/537_appconfig_tables_bounds` — границы в `AppConfigTables.cpp`,
  сохранение/регистрация новых ключей при миграции, `nullptr`-гарды
  в `~AppConfig/~DataBaseConfig` (чинит и teardown QTest).
- `fix/538_import_move_crypt` — `m_idIndexDirty/invalidateStats/
  recountSubtreeTotals` в импорте, гейты пароля и `switchToEncrypt/
  Decrypt` при переносе, `fts->remove()` в `switchToEncrypt*`.
- `fix/539_opus_stereo_buffer` — фреймы/каналы в `OpusFeedDevice`,
  `preskip` и буфер в `OpusFileReader`.
- `fix/540_ipc_mcp_socket` — `toLower()` сокета на Windows, таймаут+лимит
  в `QtLocalPeer`, `saveTextDirect` + сброс `MetaEditor` в MCP.
- `fix/541_dispatcher_thread` — диспетчер в GUI-поток (или фикс
  `wait/terminate` + перенос `QWidget`), `delete audioFormatter`.
- `fix/542_tree_record_helpers` — сортировка удаления веток,
  нерекурсивный `CryptMigrate`, `sort` в `renumSections`, UTF-8 и
  переносы в `replaceInternalReference` и Telegram-импорте.

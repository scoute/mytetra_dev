# MyTetra — текущая архитектура

> Документ: `03_architecture_current.md`
> Источник: код `app/src`, ветка `experimental-synctetra`.

## 1. Общая картина

MyTetra — классическое **Qt-приложение MVC** с рядом глобальных объектов.
Приложение **единственный экземпляр** (`QtSingleApplication5`), управление
вторым экземпляром — через IPC `--control`.

### 1.1 Глобальные объекты (`main.cpp`)

| Объект | Назначение |
|--------|-----------|
| `fixedParameters` | фиксированные списки полей, константы |
| `globalParameters` | вычислимые параметры: рабочий каталог, целевая ОС, ключ шифрования, CommandRunner |
| `mytetraConfig` | конфигурация приложения `conf.ini` |
| `mytetraFiles` | создание стартовых файлов/каталогов программы |
| `dataBaseConfig` | настройки БД `database.ini` |
| `trashMonitoring` | контроль корзины (лимиты, вытеснение) |
| `walkHistory` | история переходов между записями (в памяти) |
| `actionLogger` | журнал действий (XML, файл `actionLog.txt`) |
| `shortcutManager` | горячие клавиши |
| `periodicCheckBase` | периодическая проверка внешнего изменения `mytetra.xml` |
| `periodicSyncro` | периодическая синхронизация (внешняя команда) |
| `internalClipboard` | внутренний буфер обмена (ветки/записи) |
| `pMainWindow` / `win` | главное окно |

### 1.2 Старт программы

1. `main.cpp:main` — флаг высокого DPI, `Q_INIT_RESOURCE(mytetra)`;
2. `GlobalParameters::init()` — выбор рабочего каталога (`findWorkDirectory`),
   установка кодов локали/консоли;
3. `mytetraConfig.init()` — чтение/миграция `conf.ini`; если была инсталляция —
   запись автоопределённого языка;
4. `dataBaseConfig.init()` — чтение `database.ini`;
5. иконки-коллекции, CSS-темы (`CssHelper`);
6. `actionLogger.init()`, лог `startProgram`;
7. сплэшскрин, переводы (`.qm`);
8. `shortcutManager.init()`, создание `MainWindow`;
9. восстановление состояния окна, геометрии, открепляемых окон;
10. если `synchroonstartup` и задана команда — `win.synchronization()`;
11. запрос пароля если надо (при старте / сохранённый пароль);
12. запуск `periodicCheckBase` и `periodicSyncro`;
13. `QtSingleApplication::messageReceived` → `MainWindow::messageHandler`.

## 2. Рабочий каталог и файлы конфигурации

Рабочий каталог (порядок в `GlobalParameters::findWorkDirectory`):
`<каталог бинарника>/conf.ini` (portable) → `~/.mytetra/conf.ini` →
`~/.config/mytetra/conf.ini`.

Файлы:

- **`conf.ini`** (`AppConfig`) — версионная таблица параметров (42 таблицы),
  миграции `update_version_process()` + `AppConfigUpdater`. Ключевые группы:
  пути БД/корзины, интерфейс, внешний вид, редактор, шифрование, логирование,
  синхронизация. `QSettings::IniFormat`, UTF-8, `sync()` в деструкторе и в
  `saveAllState()`.
- **`knownbases.ini`** (`KnownBasesConfig`) — список известных БД
  (`num0..numN`, поля `dbPath`, `trashPath`, `descript`). Расположение: рабочий
  каталог. При добавлении базы путь проверяется; при удалении секции
  перенумеровываются.
- **`actionLog.txt`** — журнал действий (см. п. 8).

## 3. MVC-структура

### 3.1 Модели (`models/`)

- `tree/KnowTreeModel` — модель дерева; хранит `TreeItem`-дерево,
  загрузка/сохранение/экспорт/импорт веток; `reload()`, `reEncrypt()`.
- `tree/TreeItem` — узел дерева: карта полей, список потомков, таблица записей,
  состояние «detached», иконка, методы шифрования.
- `tree/TreeModel` — абстрактная модель дерева (базовый класс MVC).
- `tree/XmlTree` — DOM-загрузка `mytetra.xml`.
- `recordTable/RecordTableData` — таблица записей ветки; создание/удаление
  записей, сохранение через `editorSaveCallback`, работа с файлами записей.
- `recordTable/Record` — запись: «лёгкая»/«полная» (`lite/fat`), чтение текста
  с диска по требованию, шифрование файлов, пути к каталогу/файлу.
- `attachTable/*` — вложения записей (файлы/ссылки), хранение в каталоге записи.
- `dataBaseConfig/DataBaseConfig` — `database.ini`.
- `databasesManagement/*` — список баз и переключение.
- `appConfig/*` — `conf.ini`, создание стартовых файлов.
- `actionLog/*`, `shortcutSettings/*` — модели служебных окон.

### 3.2 Контроллеры (`controllers/`)

- `recordTable/RecordTableController` — операции записей: создание, удаление,
  блокировка, редактирование полей, перемещение, копирование/вырезание,
  инициализация редактора.
- `attachTable/AttachTableController` — вложения: добавить/удалить/переименовать.
- `tree/` (логика дерева в `views/tree/TreeScreen` и `KnowTreeView`).
- `databasesManagement/DatabasesManagementController` — переключение базы
  (сохраняет состояние, блокируется во время синхронизации).
- `actionLog`, `shortcutSettings` — служебные.

### 3.3 Представления (`views/`)

- `mainWindow/MainWindow` — главное окно, диспетчер `reload`, `synchronization`,
  обработка IPC-сообщений, `applicationExit`, `saveAllState`, восстановление
  состояния.
- `tree/TreeScreen` (+`KnowTreeView`) — панель дерева; `saveKnowTree()`,
  `reloadKnowTree()` с базисом `(mtime,size)`.
- `recordTable/RecordTableScreen` (+`RecordTableView`) — таблица записей; кнопка
  синхронизации.
- `record/RecordScreen` — экран записи.
- `consoleEmulator/CommandRunner`, `ConsoleEmulator` — запуск внешних команд
  (в т.ч. команды синхронизации) асинхронно через `QProcess`.
- `appConfigWindow/*` — окна настроек (в т.ч. `AppConfigPage_Synchro`).
- `findInBaseScreen`, `attachTable`, `actionLog`, `enterPassword`,
  `dialog`, `installDialog`, `printPreview`, `waitClock`.

## 4. Жизненный цикл данных БД

### 4.1 Загрузка

`DatabasesManagementController::switchToDatabase()` / старт:
`initFromXML(<tetradir>/mytetra.xml)` → DOM → рекурсивное построение `TreeItem` →
модель готова; деревья/таблицы строятся контроллерами и экранами.

### 4.2 Моменты записи на диск

**`mytetra.xml`** перезаписывается **целиком** при каждом структурном изменении
(вызывается через `TreeScreen::saveKnowTree()` → `KnowTreeModel::save()`):

- создание/переименование/удаление/перемещение ветки;
- шифрование/дешифрование ветки (тот же `saveKnowTree`);
- смена иконки ветки;
- создание/удаление/блокировка/редактирование полей записи;
- перемещение записи в таблице; drag&drop записи;
- изменение вложений (через `AttachTableController::saveState()`).

**Файлы записей** (`base/<dir>/text.html`, картинки, вложения) пишутся:

- при **переключении записи**/навигации/истории/сохранении по кнопке и выходе
  — `MainWindow::saveTextarea()` → `Editor::saveTextarea()` →
  `RecordTableData::editorSaveCallback()` (запись напрямую, старый файл в
  корзину, «осиротевшие» картинки в корзину);
- при **создании записи** (`RecordTableData::insertNewRecord`) — сразу на диск
  (`pushFatAttributes`/encrypt/decrypt);
- при **шифровании/дешифровании** — файлы перезаписываются на месте.

### 4.3 Редактор

`libraries/wyedit/Editor` — WYSIWYG-редактор (Qt HTML-подмножество, таблицы,
картинки, math-формулы через mimetex). Текст хранится в памяти только для
«полной» (fat) записи; лёгкие записи читаются с диска по требованию.
Признак изменения — `getTextareaModified()`; сохранение только при изменении.

## 5. Перезагрузка БД

`MainWindow::reload()` = `reloadSaveStage()` + `reloadLoadStage()`:

- **save-stage**: `saveTextarea()`, сохранение позиций (дерево/таблица/курсор/
  скролл-бар редактора). `mytetra.xml` при этом **не** сохраняется;
- **load-stage**: `treeScreen->reloadKnowTree()` → полный сброс модели
  (`beginResetModel`, пересоздание из `initFromXML`); восстановление позиций;
  закрытие откреплённых окон для исчезнувших записей.

Триггеры reload:

1. **IPC**: `mytetra --control --reload` → `messageHandler("reload")`;
2. **`PeriodicCheckBase`**: если mtime `mytetra.xml` на диске новее, чем
   `max(lastSave, lastLoad)` модели → автоматический reload (опционально
   сообщение). Следит **только за `mytetra.xml`**, не за `base/`;
3. **после завершения команды синхронизации** — `onSyncroCommandFinishWork`
   → `reloadLoadStage(true)`;
4. механическая защита в `TreeScreen::reloadKnowTree`: reload только если
   изменились mtime/размер файла.

## 6. Существующая синхронизация (внешняя команда)

Модель «синхронизация через внешнюю команду» (по прежней задумке — rsync и т. п.):

- Ключи конфига: `synchrocommand` (команда, макрос `%a` = канонический путь
  `tetradir`), `synchroonstartup`, `synchroonexit`, `enablePeriodicSyncro`,
  `periodicSyncroPeriod`, `enablePeriodicCheckBase`, `checkBasePeriod`,
  `enablePeriodicCheckMessage`;
- `MainWindow::synchronization(visible)` — `reloadSaveStage()`, затем команда
  через общий `CommandRunner` (асинхронно, окно консоли). Кнопка
  синхронизации на время блокируется;
- `PeriodicSyncro::timerEvent` — периодический (скрытый) запуск;
- завершение → `onSyncroCommandFinishWork` → reload + обновление откреплённых
  окон; при выходе с включённой `synchroonexit` доводит завершение приложения;
- `DatabaseManagementController` блокирует переключение баз во время синка.

Ограничения модели (важно для задачи командной работы):

- **контроль изменений — только mtime/размер `mytetra.xml`**;
- конфликт файлов на стороне внешнего инструмента (rsync) — потеря одной из
  версий при одновременной записи;
- команда не атомарна: пользователь может свернуть окно → данные пишет сам
  инструмент.

## 7. Корзина

- Каталог `trashdir` (вне `tetradir`). Файлы переносятся `QFile::rename`,
  переименование `uniqueId_<имя>`.
- Политика вытеснения: по числу файлов (`trashmaxfilecount`, 200) и объёму
  (`trashsize`, 5 МБ); `TrashMonitoring` хранит метаданные (размер, время
  рождения).
- В корзину попадают старые копии `mytetra.xml`, `text.html`, конфигов и
  удалённые записи — то есть корзина является де-факто ограниченным «журналом
  предыдущих версий», но не предназначена для этого и **перетирается**.

## 8. Журнал действий (`ActionLogger`)

- Файл `actionLog.txt` в рабочем каталоге, опционально (`enableLogging`).
- Формат строк: `<r v="1" t="<unix>" a="<action>" .../>`, значения
  HTML-экранируются.
- Белый список действий: `startProgram`, `stopProgram`, `createRecord`,
  `editRecord`, `editRecordText`, `createBranch`, `deleteBranch`,
  `moveBranchUp/Down`, `copyRecordToBuffer`, `dropRecord`,
  `startSyncro`, `stopSyncro`, `syncroError` и др.
- Кольцевой лимит 1000 строк; `actionLogPrev.txt` объявлен, но не используется.
- Просмотр: экран ActionLog.
- **Перспективно**: единственный существующий поток событий — хорошая база для
  журнала изменений синхронизации, но сейчас он одноэкземплярный, без
  переносимости между машинами и без данных для слияния.

## 9. IPC и единственный экземпляр

- `QtSingleApplication5` + `QtLocalPeer`: локальный сокет + lock-файл; второй
  обычный запуск отклоняется.
- `--control` команды: `--show`, `--hide`, `--quit`, `--reload`,
  `--openNote <id>`, `--addNoteDialog`, `--openTreeItem <id>`; сообщения идут в
  `MainWindow::messageHandler`.
- Always-running режим: закрытие окна не завершает процесс (сокрытие в трей),
  `QApplication::setQuitOnLastWindowClosed(false)`.

## 10. Управление несколькими базами

- Список известных баз: `knownbases.ini`.
- Модель сканирует: каталог бинарника, `~/.mytetra`, `~/.config/mytetra`,
  рабочий конфиг, `knownbases.ini`.
- Признак базы: `mytetra.xml` + `database.ini` + `base/`.
- Переключение: `DatabasesManagementController::switchToDatabase()` — обновляет
  `tetradir`/`trashdir` в `conf.ini`, переинициализирует `DataBaseConfig` и
  модель дерева.

## 11. Слабые места текущей архитектуры (сводка)

1. **`mytetra.xml` перезаписывается целиком** при каждом метаданном изменении —
   главная точка конфликтов при многопользовательском доступе;
2. **Неатомарная запись** (сначала move-в-корзину, затем write) — окно
   отсутствия файла; риск синхронизации частичного/удалённого файла;
3. **Нет журнала изменений** для структуры и содержимого: только ограниченная
   корзина и опциональный actionLog;
4. **Детектор внешних изменений** следит только за `mytetra.xml` по mtime —
   изменения `base/<id>/text.html` другими узлами не отслеживаются вообще;
5. **Нет координации между узлами**: одновременная запись → конфликт и потеря
   данных (или блокировка из-за lock-файла `QtLocalPeer`, но он один на машину);
6. **ID генерируются локально** (время+сл. символы) без глобальной
   дедупликации — малый, но ненулевой риск коллизий на разных машинах;
7. **Нет понятия «владелец ветки»** и разграничения прав — «обмен кусками БД»
   невозможен на уровне формата;
8. **`database.ini` зависит от машины** (крипктография) — его нельзя
   синхронизировать между участниками;
9. Криптованные ветки **нельзя сливать слепо** (поля/контент — бинарные blob-ы).
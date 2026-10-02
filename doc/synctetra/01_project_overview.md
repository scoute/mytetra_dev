# MyTetra — обзор проекта

> Документ: `01_project_overview.md`
> Ветка анализа: `experimental-synctetra`, дата: 2026-09-12

## 1. Что такое MyTetra

**MyTetra** — свободная кроссплатформенная персональная программа-менеджер
накопления информации (personal information manager, PIM), представляющая
древовидный блокнот с записями. По духу близка к CherryTree: «умный менеджер
для сбора информации» (smart manager for information collecting).

Ключевые свойства:

- **Иерархия веток** (дерево) + **таблица конечных записей** внутри каждой ветки.
- **Редактор** заметок на собственной библиотеке **WyEdit** (Qt HTML-редактор
  WYSIWYG).
- **Хранение**: текстовые HTML-файлы записей + единый XML-файл структуры дерева
  (подробнее в `02_database_format.md`).
- Поддержка **нескольких баз данных**, переключение между ними.
- **Шифрование веток** (RC5 + PBKDF2), пароль хранится локально и «не подлежит
  синхронизации через интернет» (идея, заложенная ещё в 2011 г.).
- **Корзина** — отдельный каталог, куда перемещаются старые версии файлов.
- Уже есть (ограниченная) **синхронизация через внешнюю команду** (rsync и т. п.)
  и фоновый мониторинг изменения БД сторонними программами.
- **Единственный экземпляр** приложения (QtSingleApplication) + консольное
  управление (`--control`).
- Мобильный интерфейс (Android сборка), поддержка высокого DPI.
- Автор / основной разработчик: xintrea (webhamster.ru). Сопутствующие проекты:
  MyTetra Share, MyTetra Web Client.

## 2. Технологический стек

| Параметр | Значение |
|----------|----------|
| Язык | C++ (стиль Qt, контейнеры Qt вместо STL), C++14 |
| Фреймворк | Qt 5.15 |
| Модули Qt | `gui`, `core`, `xml`, `svg`, `network` (`app/app.pro`) |
| Сторонние библиотеки | **Нет** — принцип «Qt-only». Единственное встроенное стороннее — `thirdParty/mimetex` (рендер математики) |
| Кодировка исходников | UTF-8 |
| Сборка | qmake (`mytetra.pro` — подпроекты `app` и `mimetex`) + `make` |
| IDE | Qt Creator (`mytetra.pro.user`) |
| Целевые ОС | Linux, Windows, macOS, Android (`TARGET_OS` в `app/app.pro`) |

Основной qt-проект: `app/app.pro` (содержит все исходники, ресурсы, переводы).

## 3. Репозиторий и ветки

Репозиторий: `mytetra_dev`, основной источник — GitHub `xintrea/mytetra_dev`.

Модель веток (из `README.md`):

- `master` — стабильные релизы;
- `experimental` — ветка разработки (актуальный код);
- временные функциональные ветки, вливаются в `experimental`, затем в `master`.

Локально в этом рабочем каталоге присутствуют ветки:
`experimental`, `experimental-full-bugfix`, `experimental-sco`,
**`experimental-synctetra`** (текущая, для задач командной синхронизации),
а также удалённые `origin/*`.

## 4. Структура каталогов репозитория

```
.
├── mytetra.pro              # корневой qmake-проект (subdirs: app, mimetex)
├── app/
│   ├── app.pro              # основной проект приложения
│   ├── bin/                 # ресурсы, собираемые в бинарник (qrc)
│   │   ├── mytetra.qrc, icons.qrc, themes.qrc
│   │   └── resource/standartdata/  # эталонные mytetra.xml и database.ini
│   ├── desktop/             # иконки/инфраструктура рабочего стола
│   ├── doc/                 # Doxyfile, картинки, guide
│   └── src/
│       ├── main.cpp/.h      # точка входа, глобальные объекты
│       ├── controllers/     # контроллеры MVC
│       ├── models/          # модели MVC + конфиги + данные БД
│       ├── views/           # экраны/виджеты MVC
│       ├── libraries/       # вспомогательные библиотеки (криптография, редактор и т.д.)
│       └── licenseGPL.txt / licenseBSD.txt
├── thirdParty/mimetex       # рендер математических формул (mimetex)
├── script/                  # вспомогательные скрипты
├── history.txt              # история версий (до перехода в Git)
└── build/                   # локальная сборка (в .gitignore)
```

## 5. Структура исходников `app/src` (карта кода)

Условное разделение (фактическая структура MVC):

```
app/src/
├── main.cpp                  # вход, инициализация глобальных объектов, консоль
├── main.h                    # версии формата/приложения
├── controllers/
│   ├── actionLog/            # ActionLogController — просмотр журнала действий
│   ├── attachTable/          # AttachTableController — вложения записей
│   ├── databasesManagement/  # DatabasesManagementController — переключение БД
│   ├── recordTable/          # RecordTableController — записи ветки
│   └── shortcutSettings/     # ShortcutSettingsController — горячие клавиши
├── models/
│   ├── actionLog/            # ActionLogModel
│   ├── appConfig/            # AppConfig (conf.ini), AppFiles (создание стартовых файлов)
│   ├── attachTable/          # Attach/AttachTableData/AttachTableModel
│   ├── dataBaseConfig/       # DataBaseConfig (database.ini)
│   ├── databasesManagement/  # DatabasesManagementModel, KnownBasesConfig (knownbases.ini)
│   ├── recordTable/          # Record/RecordTableData/RecordTableModel/ProxyModel
│   ├── shortcutSettings/     # ShortcutSettingsModel
│   └── tree/                 # KnowTreeModel, TreeItem, TreeModel, XmlTree
├── views/
│   ├── mainWindow/           # MainWindow — главное окно, reload, sync
│   ├── tree/                 # TreeScreen/KnowTreeView — дерево веток
│   ├── record/               # RecordScreen/RecordTableView — таблица записей
│   ├── recordTable/          # RecordTableScreen
│   ├── consoleEmulator/      # CommandRunner/ConsoleEmulator — запуск внешних команд
│   ├── appConfigWindow/      # настройки программы (страницы, в т.ч. Synchro)
│   ├── databasesManagement/  # управление базами
│   ├── findInBaseScreen/     # поиск по базе
│   ├── attachTable/          # таблица вложений
│   ├── actionLog/            # журнал действий
│   ├── enterPassword/        # ввод пароля
│   ├── dialog/, installDialog/, printPreview/, shortcutSettings/, waitClock/
└── libraries/
    ├── ActionLogger.*        # журнал действий (XML-строки)
    ├── ClipboardBranch/InternalClipboard.*   # копирование/вставка веток
    ├── DiskHelper.*          # работа с файлами, перемещение в корзину
    ├── FixedParameters.*     # фиксированные списки полей веток/записей
    ├── GlobalParameters.*    # глобальные параметры, рабочий каталог
    ├── PeriodicCheckBase.*   # проверка внешнего изменения mytetra.xml
    ├── PeriodicSyncro.*      # периодический запуск синхронизации
    ├── TimerMonitoring.*     # база периодических таймеров
    ├── TrashMonitoring.*     # контроль корзины (лимиты)
    ├── UniqueIdHelper.*      # генерация уникальных ID
    ├── WalkHistory.*         # история переходов
    ├── WindowSwitcher.*      # переключение окон/мобильный интерфейс
    ├── qtSingleApplication5/ # единственный экземпляр + IPC --control
    ├── crypt/                # CryptService, Password, Pbkdf2Qt, RC5Simple
    ├── helpers/              # ActionHelper, DebugHelper, DiskHelper и др.
    ├── wyedit/               # собственный визуальный редактор (WYMeditor-подобный)
    └── ...                   # прочие (IconSelectDialog, ShortcutManager и т.д.)
```

Полный состав контроллеров/моделей/представлений — см. листинги каталогов
в `03_architecture_current.md`.

## 6. Сборка и запуск

Без сборки: достаточно Qt SDK (принцип Qt-only).

```bash
qmake .
make            # в подпроектах соберутся mimetex и app
make install    # бинарник: /usr/local/bin/mytetra  (Linux/macOS)
```

Для Windows генерируется `mytetra.exe`. Android — через настройку целевой ОС
(`TARGET_OS=android` в сборке Qt).

Рабочий каталог программы (где лежат `conf.ini`, `knownbases.ini`,
`actionLog.txt`) определяется в `GlobalParameters::findWorkDirectory()`:

1. `<каталог бинарника>/conf.ini` — **переносной (portable) режим**;
2. `~/.mytetra/conf.ini`;
3. `~/.config/mytetra/conf.ini` — обычный режим установки (Linux).

БД по умолчанию: `/opt/mytetra/data` (`tetradir`) и корзина `/opt/mytetra/trash`
(`trashdir`) — либо относительно каталога бинарника в переносном режиме.

## 7. Соглашения о коде (сводка из README)

- Отступы 4 пробела, табуляция запрещена; UTF-8; комментарии рус/англ.
- Именование: `ИмяКласса`, `имяМетода()`, `имяПеременной`,
  файлы `ИмяКласса.h/.cpp`.
- Блоки `{ }` на отдельных строках.
- Каждый новый метод — с комментарием назначения.
- Заголовки классов: `#ifndef _ИМЯ_H_ / #define ... / #endif`.
- Только Qt-контейнеры; кроссплатформенность; без внешних библиотек вне Qt
  (стороннее — только включением исходников в проект).
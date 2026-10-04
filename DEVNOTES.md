# Заметки разработки (ветка experimental-sco-new-features)

Живые нюансы, которые легко забыть. Обновлять при новых находках.

## Открытие картинок во внешней программе

- `ImageFormatter::openImage()` — программа по умолчанию через
  `QDesktopServices::openUrl()`. Кроссплатформенно, ничего особого.
- `ImageFormatter::openImageWith()` + `EditorImageOpenDialog` —
  диалог выбора программы:
  - Список программ собирается из `.desktop`-файлов **только на Linux**
    (`~/.local/share/applications`, `$XDG_DATA_DIRS/applications`).
    Фильтр: `Type=Application`, без `NoDisplay`/`Hidden`, в `MimeType`
    есть `image/*`. Коды полей Exec (`%f`, `%U` и прочие) вычищаются
    в `cleanExecCommand()`, файл дописывается последним аргументом.
  - Пометка «(по умолчанию)» — через `xdg-mime query default image/png`.
    Без `xdg-mime` пометки просто нет, диалог работает.
  - Запуск: `QProcess::splitCommand()` + `startDetached()`.
- **Windows — отдельная работа (TODO).** Там нет `.desktop`:
  список системных программ будет пуст, останется только «Обзор...».
  Для полноценного списка нужно перечисление через реестр
  (`HKCR\.png\OpenWithProgids`, раздел `Applications`) или
  `AssocQueryString`. Запуск `.exe` с пробелами в пути через
  `splitCommand` + отдельным аргументом-файлом должен работать,
  но не проверен. `xdg-mime` отсутствует — пометки default не будет.
- **macOS не реализован.** Для списка программ нужен Launch Services,
  для запуска — `open -a "App" файл`. Сейчас там только «Обзор...».
- Путь к файлу картинки обязательно делать абсолютным
  (`QFileInfo(...).absoluteFilePath()`): рабочий каталог записи бывает
  относительным (`./data/...`), а относительный file-URL внешняя
  программа не открывает («Операция не поддерживается»).
  Проверка существования файла — тоже по абсолютному пути, иначе
  зависит от текущего каталога процесса.
- Общий резолвер пути: `ImageFormatter::resolveImageFilePath()`.
  Вся диагностика (статусбар, предупреждения) внутри него, чтобы
  в вызывающих местах не было тихих `return`.

## Переводы

- Новые `tr()`-строки дописываются вручную в `mytetra_ru.ts`
  (контекст = имя класса, location с реальным номером строки),
  затем `lrelease mytetra_ru.ts -qm mytetra_ru.qm` в том же коммите.
- Полный `lupdate` не гонять: идёт 5+ минут и мусорит
  (`ekuTrR.json`, `.qmake.stash`).

## Сборка

- Новый `.cpp`/`.h` не попадает в сборку без записи в `app/app.pro`
  (там явные списки, без wildcard) + перезапуск qmake в shadow-каталоге.
- Инкрементальная сборка в `/tmp/.../nf-clean` — основная рабочая.
  После добавления файлов в `.pro` qmake перегенерировать там же.
- `thirdParty/mimetex/build` общий для платформ: перед кросс-сборкой
  удалять, иначе PE/ELF-мусор в объектниках.
- Тестовые прогоны создают мусорный
  `app/bin/resource/standartconfig/any/editorconf.ini` — перед коммитом
  откатывать, в коммит не брать.
- Headless-харнесы: линковка тестового `.cpp` со всеми `.o` кроме
  `main.o` + определения глобалов из `main.cpp`
  (`globalParameters`, `mytetraConfig`, `actionLogger`, ...,
  `QObject *pMainWindow`, `InternalClipboard *internalClipboard`),
  запуск с `QT_QPA_PLATFORM=offscreen`, выход через `_exit()`.
  Готовые лежат в `/tmp/opencode/test_*.cpp` (вне репозитория —
  `*.sh` и тесты в `.gitignore`, в коммит не брать).
- Каждый коммит с изменением кода/ресурсов: инкремент
  `APPLICATION_RELEASE_MICROVERSION` в `app/src/main.h`.
- `git add` только явными файлами, никогда `git add -A`
  (неотслеживаемый `build/` с данными приложения).

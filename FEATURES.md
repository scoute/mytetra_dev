# Карта веток scoute/mytetra_dev

Ветки `mr/*` — багфиксы для автора (`xintrea/mytetra_dev`), нарезаны
минимальными кусками. Ветки `feat/*` — фичи, по одному коммиту на фичу.
Всё запушено в `scoute/mytetra_dev`.

## Порядок слияния

1. `mr/*` (в любом порядке, но `mr/build-fixes` первым — без него
   не собирается mimetex на современной glibc).
2. `feat/in-note-find-replace`, `feat/cut-branch`, `feat/theme`
   в любом порядке (независимы, база — тип багфиксов).
3. `feat/global-find` поверх `feat/in-note-find-replace`
   (мост в заметку требует полоски).
4. `feat/tags-panel` поверх `feat/global-find`
   (словарь тегов оттуда).
5. `feat/branch-dnd` поверх `feat/cut-branch` (нужен `moveBranch`).

Ветка `experimental` уже содержит всё сразу (влит
`experimental-sco-new-features` поверх авторского `901cb851`).

## Багфиксы (mr/*)

- `mr/build-fixes` — `mimetex_strcasestr`, `CODECFORSRC`, `make install`
  кладёт mimetex рядом, коды возврата `--control`.
- `mr/crash-fixes` — падения `--openNote/--openTreeItem/--openBranch`
  без параметра, `IconSelectDialog` (nullptr + бесконечный цикл),
  границы `RecordTableData` (вплоть до сноса `base/`).
- `mr/file-ops-safety` — проверки переименования аттачей, копирования
  при экспорте/импорте, `QSaveFile`+0600, `QTemporaryFile`, корзина.
- `mr/no-shell-launch` — mimetex и ссылки без shell.
- `mr/downloader-hardening` — закалка `Downloader`.
- `mr/ssl-selfsigned-option` — опция игнора self-signed + политика
  при битом TLS-бэкенде (Qt 5.15.2 под OpenSSL 1.1.1, в рантайме 3.x).
- `mr/image-paste-confirm` — картинки по http(s) только с подтверждением.
- `mr/import-traversal` — защита импорта от path traversal.
- `mr/ipc-control` — коды 4/5, проверка доставки `sendMessage()`.

## Фичи (feat/*, требования — багфиксы выше)

- `feat/in-note-find-replace` — встраиваемая полоска поиска/замены
  вместо модального окна, мост в глобальный поиск, отмена одним Ctrl+Z.
- `feat/cut-branch` — вырезание серым состоянием, вставка через `moveBranch`.
- `feat/global-find` — счётчики и итоги веток, атомарные теги,
  автодополнение запроса и поля тегов, дефис в подстроке.
- `feat/tags-panel` — док тегов (F8): список, фильтр, клик-поиск,
  rename/delete с подтверждениями. Слияние заблокировано.
- `feat/branch-dnd` — перенос веток мышью с картинкой (плюсик
  не ставится осознанно: операция перемещение, а не копия).
- `feat/theme` — тонкие сплиттеры в dark, живое превью темы с откатом
  по Cancel, +1px строке меток.

## В работе

- Клиппер (`experimental-sco-new-features`, в `feat/*` пока не упакован):
  `mytetra --control --clipboard [--url <url>]` читает буфер обмена
  работающим экземпляром и создает заметку в автоветке `Clipboard`
  (имя — первая строка, URL — из `--url` или из текста).
  Пункт меню Tools без дефолтного шортката. Хоткей ОС (например Ctrl+Q)
  вешается на команду `--control` средствами системы.

## Отложено и отклонено

- Глобальный replace и полный автотегинг — «когда-нибудь», нужды не было.
- Слияние тегов — отдельной фичей, позже.
- Спойлеры v1 — **откачены**: вставка только через правку HTML,
  потеря содержимого после открытия-закрытия. Кнопка не доходила
  до старых конфигов (дефолтная линия только для новых).
- MCP — сюрприз в конце фич, план отдельно.

## Прочее

- `backup-syncTetra` — сохранённый коммит `1a489d59` (export/import
  общих веток), убранный с дороги при заливке в `experimental`.
- `experimental-sco-new-features` — рабочая ветка разработки, история
  по коммитам на фичу (до схлопывания в `feat/*`).

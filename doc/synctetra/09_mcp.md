# MyTetra — MCP-сервер (stdio, для ИИ-агентов)

> Документ: `09_mcp.md`
> Статус: реализован stdio-режим (Фаза MCP-1). HTTP-режим — отдельная работа на Qt6.

## 1. Что это

Режим `mytetra --mcp [--db-path <каталог>]` превращает MyTetra в MCP-сервер:
стандартный протокол Model Context Protocol (JSON-RPC 2.0) поверх stdin/stdout
запущенного процесса (NDJSON: одно сообщение — одна строка). Совместим с любым
MCP-клиентом: OpenCode / Claude Code / Claude Desktop / Cursor / Goose и др. —
совместимость на уровне протокола, а не бренда.

Запуск:

```bash
mytetra --mcp-rw [--db-path /path/to/base]   # чтение и запись
mytetra --mcp-ro [--db-path /path/to/base]   # только чтение и поиск
mytetra --mcp                                # алиас --mcp-ro (безопасный дефолт)
QT_QPA_PLATFORM=offscreen mytetra --mcp-rw   # без дисплея
```

Безопасный дефолт: запись включается только явным `--mcp-rw`. Голый `--mcp`
и `--mcp-ro` — read-only, при противоречии флагов побеждает безопасный вариант.
Плохой агент не испортит БД случайно.

Флаг `--mcp-ro`: read-only режим для агентов, которым писать не положено.
Пишущие инструменты скрыты из `tools/list` (остаются 8 читающих) и отклоняются
при прямом вызове ошибкой `-32602` с объяснением. БД гарантированно не меняется.

Пример минимальной сессии:

```bash
$ echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"t","version":"0"}}}' \
  | mytetra --mcp | head -1
{"id":1,"jsonrpc":"2.0","result":{"capabilities":{"tools":{}},"protocolVersion":"2024-11-05",...}}
```

## 2. Правила режима

1. **stdout — только протокол.** Баннер, `qDebug`, диагностика — в stderr
   (перехват `myMessageOutput` в этом режиме не ставится).
2. **Один писатель.** При запущенном GUI-экземпляре старт запрещён
   (exit 5): параллельная запись whole-file `mytetra.xml` несовместима.
   Чтение при запущенном GUI запрещено по той же причине (модель GUI держит
   несохранённое состояние — MCP увидел бы stale с диска).
3. **Свежая модель перед каждым вызовом** (`reload()` с диска).
4. **Шифрование недоступно**: ветки/записи с `crypt=1` возвращают понятную
   ошибку (пароля в MCP-режиме нет; разблокировать — в GUI).
5. **БД по умолчанию** — `tetradir` из `conf.ini`; `--db-path` действует только
   в памяти и на диск (conf.ini) не пишется.
6. **Чистый выход**: конфиги синхронизируются досрочно
   (`syncAndDisableExitSync`), т. к. запись из глобальных деструкторов
   небезопасна (мёртвый QTextCodec-кэш Qt); лок-файл single-instance
   освобождается штатным деструктором (никаких `_exit`).

## 3. Инструменты (v1)

| Инструмент | Аргументы | Описание |
|---|---|---|
| `list_branches` | — | Дерево веток: id, имя, число заметок |
| `list_records` | `branch_id` | Заметки ветки: id, имя, автор, url, теги, ctime |
| `read_note` | `record_id` | Поля + HTML-текст (обрезка 200K с пометкой) |
| `search` | `query`, `limit` (по умолч. 20, макс. 200) | Подстрока без учёта регистра: имена веток, поля, тексты (сниппеты) |
| `create_note` | `branch_id`, `name`, `text?`, `author?`, `tags?`, `url?` | Новая заметка (свежие id/dir, `model.save()`) |
| `update_note_text` | `record_id`, `text` | Замена текста (файл; `mytetra.xml` не трогается — mtime в формате нет) |
| `set_tags` | `record_id`, `tags` | Замена тегов (`model.save()`) |
| `suggest_tags` | `record_id`, `top_n?` (по умолч. 5) | Подсказка тегов из существующего словаря (TF-IDF + стемминг, см. ниже). Ничего не меняет; применять через `set_tags` |
| `move_note` | `record_id`, `target_branch_id` | Перемещение заметки (id сохраняется — карты подписок валидны; старый каталог в корзину) |
| `rename_branch` | `branch_id`, `name` | Переименование ветки |
| `rename_note` | `record_id`, `name` | Переименование заметки (только заголовок) |
| `delete_note` | `record_id` | Удаление заметки (файлы в корзину) + чистка карт подписок |
| `move_branch` | `branch_id`, `new_parent_branch_id` (`"0"` — верхний уровень) | Перемещение ветки с поддеревом (защита от циклов) |
| `delete_branch` | `branch_id` | Удаление ветки с поддеревом (файлы в корзину) + чистка карт; корень запрещён |
| `find_duplicates` | — | Аудит: группы одинаковых имён / идентичных текстов (sha256). Read-only |
| `find_empty_branches` | — | Аудит: ветки без заметок. Read-only |
| `find_untagged_notes` | `limit?` (по умолч. 200) | Аудит: заметки без тегов. Read-only, в паре с `suggest_tags` |

Типовой сценарий наведения порядка ИИ-агентом: `find_duplicates` /
`find_empty_branches` / `find_untagged_notes` → `suggest_tags` → подтверждение
пользователя → `set_tags` / `move_note` / `delete_note`. Удаления необратимы
из MCP (только корзина) — клиент обязан спрашивать пользователя.

Ошибки инструментов: `{"content":[{"type":"text","text":"..."}],"isError":true}`;
протокольные ошибки: стандартные коды JSON-RPC (`-32700`, `-32601`, `-32602`, `-32603`).

## 4. Реализация (где что)

- `app/src/mcp/McpServer.{h,cpp}` — протокол (initialize/ping/tools/*, NDJSON,
  пакеты-массивы), диспетчер, 7 инструментов, `runMcpMode()`;
- `app/src/main.cpp` — раннее определение `--mcp` (до баннера!), пропуск
  `setDebugMessageHandler`, offscreen при отсутствии дисплея, `return runMcpMode(...)`;
- `RecordTableData::insertNewRecord` — null-guard `find_object<KnowTreeView>`
  (в headless видов нет; вызывающий код передаёт свежие ID);
- `AppConfig::syncAndDisableExitSync`, `DataBaseConfig::syncAndDisableExitSync` —
  досрочный sync + запрет записи из деструкторов (аддитивно, GUI не затрагивается);
- `app/app.pro` — регистрация `src/mcp/*`.

Переиспользуется без изменений: `KnowTreeModel` (дерево, `save()`/`reload()`),
`RecordTableData`/`Record` (создание, `pushFatAttributes`), формат БД (никаких
изменений формата).

## 5. Проверено

E2E пайпом на копии тестовой БД: initialize → tools/list (7→17) → list_branches →
list_records → search → read_note → create_note → update → set_tags →
read-back (текст и теги сошлись); затем клининг: suggest_tags → move_note →
rename → delete_note → move_branch (туда-обратно) → delete_branch (пустая) →
отказы self-move и delete-root; XML валиден после всех мутаций; ошибки
(нет ветки/заметки/инструмента, битый JSON) — корректные коды; `conf.ini`
не загрязняется (`--db-path` в памяти); выход 0 без падений.

## 6. Автотеги: движок TagSuggester

`app/src/libraries/TagSuggester.{h,cpp}` (чистый QtCore, без ML-зависимостей):
токенизация (lowercase, ё→е, стоп-слова RU/EN, длина ≥3, лёгкий стемминг
русских окончаний), профили тегов TF-IDF по размеченным заметкам, скоринг
с объяснимостью (топ-3 слова-совпадения). Тексты нормализуются через
`TextDiff::htmlToTextLines` (без `<style>`/`<script>`-мусора). Юнит-тест —
`ALL_PASS`, включая доказательство необходимости стемминга
(«свеклу»≠«свекла» без него). Используется MCP-тулом `suggest_tags`;
позже — кнопкой в UI редактора.

## 7. Дорожная карта

- **MCP-3**: работа с вложениями, ресурсы (`branches://`, `notes://`);
- **Параллельность с GUI**: in-GUI MCP-служба (запросы исполняет GUI-процесс,
  второй процесс — тонкий форвардер) вместо нынешнего запрета;
- **HTTP-режим**: только на Qt6 (`QHttpServer`), с auth-токеном и привязкой
  по умолчанию к localhost. Диспетчер инструментов общий со stdio.

# Headless-стенды (пробы)

Проверки поведения без GUI (`QT_QPA_PLATFORM=offscreen`). Пробы
компилируются против объектных файлов текущей сборки (`$BLD/*.o`),
линкуются без `main.o` (вместо него `main_renamed.o` — глобалы
должны соответствовать текущим заголовкам, пересобирается скриптом).

Запуск (из корня `mytetra-dev`):

```sh
export PATH=/media/user/m2data/Qt_deb11/5.15.2/gcc_64/bin:$PATH
./app/tests/run_probes.sh [имя ...]   # по умолчанию все
```

Правила (см. AGENTS.md):

- Стенд зелёный = нужный текст в stdout (`PASS`/`PROBE-OK`/`VOICEQ-OK`).
  Код возврата НЕ показатель: teardown под offscreen штатно падает.
- Новая проба обязана инициализировать `AppContext::instance().init(...)`
  со стековыми дублями/глобалами, иначе `AppContext::instance().xxx()`
  вернёт null и будет segfault (примеры: `menuprobe.cpp`, `confprobe.cpp`).

| Проба | Что проверяет |
|---|---|
| `confprobe` | Секции конфига round-trip + счётчик фолбэков `get_parameter` |
| `refprobe` | `AppContext`/ядро: типизация, холдер ключа, инъекция `WalkHistory` |
| `trayprobe` | `TrayManager` против голого `QWidget` (DI) |
| `menuprobe` | `MenuBuilder` против `QMenuBar` (десктоп/мобайл наборы, сигналы) |
| `mcpbridgeprobe` | Сессии/гейты `McpGuiBridge` на лямбдах |
| `typefaceprobe` | Маркеры кода, `collectCodeBelowMarker`, снятие подчёркивания copy-ссылок, TOC |
| `mcpcatprobe` | Каталог MCP-тулов: состав, категории `ro`/`rw`/`delete` |
| `bridgewriteprobe` | Write-пути моста на живой `KnowTreeModel` (create/update/batch/delete) |
| `voiceqprobe` | Opus-битрейты, комбо качества, радио настроек, MP3-блок |
| `htmlprobe` | Движок `HtmlSourceEdit`: поиск с зацикливанием, подсветка, замена, переход к строке, гуттер, дерево тегов |
| `splitprobe` | Сплит редактор+превью: живой `MetaEditor`, двусторонняя синхронизация исходника (фикстура из штатных конфигов) |
| `settingstransferprobe` | Перенос настроек: выгрузка в один INI и загрузка обратно, пропуски версий/путей/секретов, бэкапы, отказ на битом файле |

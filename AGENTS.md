# AGENTS.md — MyTetra (mytetra-dev)

Qt 5.15 / qmake C++ knowledge base app. Git workdir is `mytetra-dev/` (`git` elsewhere fails — not a repo).

## Branches & commits

- Lines: `develop-v1-mytetra` (MyTetra, current), `develop-v2-synctetra` (team sync). Old lines live in `backup-*` — don't touch.
- Unit branches `fix/NNN_*`, `feat/NNN_*` (shared 001… numbering). One branch = one commit + one `--no-ff` merge.
- No cherry-picks: shared fixes are ported manually, oldest line first (v1 → merge to v2).
- Commit format `[fix|feat|merge|chore][layer=>module] text`.
- Semver bumps are mandatory after every merged unit, no exceptions:
  `fix` → micro (`1.142.0` → `1.142.1`), `feat` → minor (`1.142.1` → `1.143.0`).
  `merge`/`chore` bump nothing.
- Version bumps only as separate `[chore][version=>X]` commits touching
  `app/src/main.h` and nothing else; never touch `main.h` in feat/fix commits.
- Order: bump right after the unit's `--no-ff` merge, before starting the
  next unit. If several units merged without bumps, add one chore commit
  per unit in merge order (oldest first), not a single jump.
  Example: `[chore][version=>1.143.0] Bump for feat/245_symbol_bigger`.
- Never `git add -A`; add files explicitly. `ChatExport_*` dirs are untracked test data — never commit.
- Push / merge to shared lines only on explicit user order.

## Build

```sh
export PATH=/media/user/m2data/Qt_deb11/5.15.2/gcc_64/bin:$PATH
cd mytetra-dev/build/Desktop_Qt_5_15_2_GCC_64bit/app && make -j12
```

- `app/app.pro` lists every file explicitly. New `.cpp/.h/.svg` → add to `app.pro` (+ `mytetra.qrc` for resources) and rerun `qmake ../../../app/app.pro` in `build/.../app` before `make`. Verify `rcc` re-ran in the log when adding resources.
- Qt-only, no external deps (README rule). MinGW/Windows: `_WIN32_WINNT` must be defined **before any Qt include** (Qt headers pull in `windows.h` first, otherwise Vista+ APIs like `QueryFullProcessImageNameA` are undeclared).
- Code style (README): UTF-8, spaces only (no tabs), indent 4, `{`/`}` on their own lines, RU/EN comments ok, every new method needs a comment.

## Config & i18n

- `EditorConfig` (editorconf.ini) is versioned: new keys require a new `get_parameter_table_N` + registration in the table list + migration rules in `update_version_change_value`. Same pattern as the mic/secret/toggleTools dockings.
- `EditorConfig::get_parameter()` calls `criticalError` on missing **or empty** values. Nullable keys (empty default) must be read via `conf->value(...)` directly.
- `.ts` files: never run bare `lupdate` (rewrites thousands of line numbers). Add messages by hand next to their `<name>` context (context = class name of the `tr()` call site — runtime lookup fails otherwise), then `lrelease` the `.qm`. Only `ru` must be 100% (strict check counts empty + `unfinished`, ignores `obsolete`).

## Verified Qt 5.15 gotchas

- `QSplitter::setSizes` is permanently ignored after `replaceWidget`. Move widgets with `insertWidget`/`addWidget` + deferred sizes.
- `QHeaderView::setSectionResizeMode` on an empty header (no model yet) segfaults. Set modes in overridden `setModel`, not the constructor.
- `QTextDocument` renders scaled images with fast (jaggy) filtering — store/display images at 1:1 instead.
- `QAction` shortcuts die with their widget: a hotkey for a hideable panel must also be `addAction`ed to a permanently visible parent (one QAction = one trigger, no double-fire).
- `QTextCursor::selectedText()` separates lines with U+2029.
- Headless probes segfault at teardown — verify via output text, never via exit code.

## Headless verification (probes)

- Stands live in `/tmp/opencode` (volatile — rebuild as needed, never link its `.o` on Windows shadows).
- Probe recipe: `internalClipboard`, `RandomInitter::init`, `dataBaseConfig.init()`, `mytetraConfig.init()`; link `$BLD/*.o` minus `main.o` with `-Dprivate=public -Dprotected=protected`.
- Fixture `conf.ini` needs matching conf version + `programm=mytetra` + absolute `tetradir`/`trashdir`. Binary resolves workdir from `argv[0]` — copy binary into fixture, run via `cd` + `./`. Env: `QT_QPA_PLATFORM=offscreen`, `GIT_CONFIG_NOSYSTEM=1`, `HOME=/tmp/opencode`.
- `cp` over a running binary fails (ETXTBSY); kill by `/proc` exe match, never self-matching `pkill -f`.

## MCP server

- `opencode.json` runs the built binary with `--mcp-rw` over stdio (offscreen). `tools/list` logs `mcp: tools/list served N tools in MODE` to stderr — proof the server (not the client) is healthy.
- Writes go through the live GUI (socket + ack + save-wait); reads never wait for ack.

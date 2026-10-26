#!/usr/bin/env bash
# Отладочная сборка виндового портабла: без strip, с link map.
# Нужна чтобы по адресу падения из Wine-логa (synctetra+0x...)
# найти точную функцию:
#   nm -n SyncTetra-win-portable/synctetra.exe | grep -B1 ...
#   addr2line -e SyncTetra-win-portable/synctetra.exe 0x... -f -C
# Бинарь жирный и медленнее, для повседневного использования
# собирайте обычным build_portable_windows.sh.
#
# Использование: те же флаги что у build_portable_windows.sh.
# --no-strip и --map подставляются сами и перебить их нельзя.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"

exec "${HERE}/build_portable_windows.sh" "$@" --no-strip --map

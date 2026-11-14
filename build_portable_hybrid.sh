#!/usr/bin/env bash
# Гибридный портабл MyTetra: Windows и Linux в одной папке с общей базой.
# Собирается из двух готовых портаблов (build_portable_linux.sh
# и build_portable_windows.sh):
#   бинари обеих систем лежат рядом в корне,
#   виндовые DLL рядом с exe (Windows ищет их сам),
#   линуксовые .so в lib/, линуксовые плагины в plugins-linux/,
#   виндовые плагины в plugins/ (у них qt.conf),
#   переводы Qt общие в translations/.
# База и настройки общие: conf.ini с относительными путями ./data
# одинаково работает на обеих системах. Первый запуск на любой
# из систем открывает диалог установки и создает все сам.
# В поставке конфигов и данных НЕТ.
#
# Использование:
#   ./build_portable_hybrid.sh                       из готовых портаблов
#                                                    рядом с исходниками
#   ./build_portable_hybrid.sh --linux DIR --win DIR --out DIR
#   ./build_portable_hybrid.sh --smoke                дымовой тест Linux
#   ./build_portable_hybrid.sh --smoke-wine           дымовой тест Windows
#                                                     под Wine (долго)

set -euo pipefail

# Исходники: корень репозитория от места скрипта
SCRIPT_PATH="$(readlink -e "$0" 2>/dev/null || echo "$0")"
SOURCE_DIR="$(cd "$(dirname "${SCRIPT_PATH}")" && pwd)"

LINUX_DIR="$(dirname "${SOURCE_DIR}")/MyTetra-lin-portable"
WIN_DIR="$(dirname "${SOURCE_DIR}")/MyTetra-win-portable"
OUT_DIR=""
OUT_DIR_EXPLICIT=0
DO_SMOKE=0
DO_SMOKE_WINE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --linux) LINUX_DIR="$2"; shift 2;;
        --win) WIN_DIR="$2"; shift 2;;
        --out)
            OUT_DIR="$2"
            OUT_DIR_EXPLICIT=1
            shift 2
            ;;
        --smoke) DO_SMOKE=1; shift;;
        --smoke-wine) DO_SMOKE_WINE=1; shift;;
        --help) sed -n '2,20p' "$0"; exit 0;;
        *) echo "Неизвестный аргумент: $1" >&2; exit 4;;
    esac
done

if [[ "${OUT_DIR_EXPLICIT}" -eq 0 ]]; then
    OUT_DIR="$(dirname "${SOURCE_DIR}")/MyTetra-hybrid-portable"
fi

for need in "${LINUX_DIR}/mytetra" "${LINUX_DIR}/lib" "${WIN_DIR}/mytetra.exe"; do
    if [[ ! -e "${need}" ]]; then
        echo "ОШИБКА: нет ${need}" >&2
        echo "Сначала соберите оба портабла обычными скриптами." >&2
        exit 2
    fi
done

echo "Linux:     ${LINUX_DIR}"
echo "Windows:   ${WIN_DIR}"
echo "Результат: ${OUT_DIR}"
echo

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}/lib" "${OUT_DIR}/plugins-linux" "${OUT_DIR}/translations"

# Бинари обеих систем рядом в корне
cp "${LINUX_DIR}/mytetra" "${LINUX_DIR}/mimetex" "${OUT_DIR}/"
cp "${WIN_DIR}/mytetra.exe" "${WIN_DIR}/mimetex.exe" "${OUT_DIR}/"
chmod +x "${OUT_DIR}/mytetra" "${OUT_DIR}/mimetex"

# Виндовые DLL рядом с exe: Windows резолвит их сам, ничего
# настраивать не надо. Именно поэтому линуксовые убраны в lib/,
# а виндовые лежат в корне: так обе системы довольны
cp "${WIN_DIR}/"*.dll "${OUT_DIR}/"

# Виндовые плагины как есть (у них свой qt.conf ниже)
cp -r "${WIN_DIR}/plugins" "${OUT_DIR}/"

# Линуксовые библиотеки и плагины в свои каталоги, чтобы не
# перемешаться с виндовыми. Имена частично совпадают по смыслу
# (platforms есть у обоих), поэтому plugins-linux отдельно
cp -r "${LINUX_DIR}/lib/." "${OUT_DIR}/lib/"
for plugdir in platforms imageformats iconengines xcbglintegrations; do
    if [[ -d "${LINUX_DIR}/${plugdir}" ]]; then
        cp -r "${LINUX_DIR}/${plugdir}" "${OUT_DIR}/plugins-linux/"
    elif [[ -d "${LINUX_DIR}/plugins/${plugdir}" ]]; then
        cp -r "${LINUX_DIR}/plugins/${plugdir}" "${OUT_DIR}/plugins-linux/"
    fi
done

# Переводы Qt общие (версия Qt одна и та же, 5.15.2)
if [[ -d "${LINUX_DIR}/translations" ]]; then
    cp "${LINUX_DIR}/translations/"*.qm "${OUT_DIR}/translations/" 2>/dev/null || true
fi

# qt.conf виндовой стороны: Prefix=. Plugins=plugins.
# Линуксу он не мешает: ланчер задает пути явно через окружение
cp "${WIN_DIR}/qt.conf" "${OUT_DIR}/qt.conf" 2>/dev/null || true

# Линукс-ланчер: свои lib и свои плагины. Аргументы пробрасываются
# для mytetra --control (хоткей клиппера из ОС)
cat > "${OUT_DIR}/start.sh" <<'LAUNCHER'
#!/bin/sh
PROGDIR=$(dirname $(readlink -e $0))
export LD_LIBRARY_PATH=$PROGDIR/lib:${LD_LIBRARY_PATH}
export QT_PLUGIN_PATH=$PROGDIR/plugins-linux
export QT_QPA_PLATFORM_PLUGIN_PATH=$PROGDIR/plugins-linux/platforms
exec "$PROGDIR/mytetra" "$@"
LAUNCHER
chmod +x "${OUT_DIR}/start.sh"

GITREV="$(git -C "${SOURCE_DIR}" rev-parse --short HEAD 2>/dev/null || echo unknown)"

cat > "${OUT_DIR}/PORTABLE.txt" <<EOF
MyTetra portable гибрид: Linux + Windows с общей базой
=======================================================
Linux:   ./start.sh
Windows: mytetra.exe (двойной клик, DLL рядом подхватываются сами)

База и настройки общие: conf.ini с относительными путями ./data
работает на обеих системах. Первый запуск на любой из систем
открывает диалог установки, дальше база переезжает между системами
вместе с папкой. Конфигов и данных в поставке нет, все создается само.

Глобальный хоткей клиппера вешается на mytetra --control --clipboard
(через start.sh в Linux, напрямую через exe в Windows).

Собрано из ревизии ${GITREV}.
EOF

VERSION="$(grep -E '^#define APPLICATION_RELEASE_(VERSION|SUBVERSION|MICROVERSION)' \
              "${SOURCE_DIR}/app/src/main.h" | awk '{print $3}' | paste -sd. -)"

echo "Готово. Версия ${VERSION}, ревизия ${GITREV}"
echo "Содержимое ${OUT_DIR}:"
ls "${OUT_DIR}"
echo "Размер папки: $(du -sh "${OUT_DIR}" | awk '{print $1}')"
echo
echo "Linux:   ${OUT_DIR}/start.sh"
echo "Windows: ${OUT_DIR}/mytetra.exe"

smoke_one() {
    local label="$1"
    shift
    local wait_secs="$1"
    shift
    echo "Дымовой тест ${label}"
    SMOKE_DIR="$(mktemp -d)"
    cp -r "${OUT_DIR}/." "${SMOKE_DIR}/"
    mkdir -p "${SMOKE_DIR}/data/base/seed00000000000001"
    cp "${SOURCE_DIR}/app/bin/resource/standartdata/mytetra.xml" "${SMOKE_DIR}/data/" 2>/dev/null || true
    cp "${SOURCE_DIR}/app/bin/resource/standartdata/database.ini" "${SMOKE_DIR}/data/" 2>/dev/null || true
    cp "${SOURCE_DIR}/app/bin/resource/standartdata/base/1300000000aaaaaaaaa2/text.html" "${SMOKE_DIR}/data/base/seed00000000000001/" 2>/dev/null || true
    printf '[General]\nprintdebugmessages=true\n' > "${SMOKE_DIR}/conf.ini"
    ( cd "${SMOKE_DIR}" && "$@" >smoke.log 2>&1 ) &
    SMOKE_PID=$!
    sleep "${wait_secs}"
    if kill -0 $SMOKE_PID 2>/dev/null; then
        echo "Дымовой тест ${label} пройден: процесс жив"
        kill $SMOKE_PID 2>/dev/null || true
    else
        echo "ОШИБКА: дымовой тест ${label} провален, лог:" >&2
        tail -20 "${SMOKE_DIR}/smoke.log" >&2
        rm -rf "${SMOKE_DIR}"
        exit 6
    fi
    rm -rf "${SMOKE_DIR}"
}

# Дым Linux быстрый, Wine медленный: разные флаги и таймауты
if [[ "${DO_SMOKE}" -eq 1 ]]; then
    smoke_one "Linux" 12 env QT_QPA_PLATFORM=offscreen timeout 15 ./start.sh
fi

if [[ "${DO_SMOKE_WINE}" -eq 1 ]]; then
    smoke_one "Windows/Wine" 45 env QT_QPA_PLATFORM=offscreen timeout 90 wine ./mytetra.exe
fi

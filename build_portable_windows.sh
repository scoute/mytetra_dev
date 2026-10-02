#!/usr/bin/env bash
# Сборка портабельной версии MyTetra для Windows через Wine.
# Qt для Windows (mingw81_32) и тулчейн берутся из обычной Qt-установки,
# сборка идет вызовом windows-qmake и mingw32-make под Wine.
# Медленно (сам Wine нетороплив), зато без виртуалки.
#
# В поставке только бинари, DLL Qt и рантайма, плагины, qt.conf.
# Конфигов и данных НЕТ специально: при первом запуске диалог
# установки сам создаст conf.ini, базу и корзину. Привезенный
# conf.ini от другой версии приводил к незапуску с ручной чисткой.
# Ланчер не нужен: Windows ищет DLL рядом с exe сам.
#
# Использование:
#   ./build_portable_windows.sh            без параметров: исходники откуда
#                                          запущен скрипт, сборка из каталога
#                                          Qt Creator (если есть), Qt из него,
#                                          результат рядом в MyTetra-portable-win
#   ./build_portable_windows.sh --clean    очистить сборочный каталог
#   ./build_portable_windows.sh --jobs 2   потоки сборки (под Wine больше
#                                          4 обычно только хуже)
#   ./build_portable_windows.sh --portable-dir /путь/к/папке
#   ./build_portable_windows.sh --qt /путь/к/Qt-mingw81_32
#   ./build_portable_windows.sh --mingw /путь/к/mingw730_32
#   ./build_portable_windows.sh --src /путь/к/исходникам
#   ./build_portable_windows.sh --build-dir /путь/к/сборке
#   ./build_portable_windows.sh --no-build  только упаковать готовое
#   ./build_portable_windows.sh --strip / --no-strip
#   ./build_portable_windows.sh --smoke    тестовый запуск в offscreen под Wine
#
# WINEPREFIX пробрасывается как есть (по умолчанию ~/.wine).

set -euo pipefail

# --------------------------------------------------------------------------
# Хардкод параметров. Правьте под свою машину.
# --------------------------------------------------------------------------

# Qt для Windows: предсобранный mingw81_32
QT_SDK="/media/user/big_data/soft/windows/QT_installed/5.15.2/5.15.2/mingw81_32"

# Тулчейн MinGW: разрядность обязана совпадать с Qt (32 бита).
# Несовпадение версий gcc (7.3 против 8.1 у Qt) на практике линкуется,
# libstdc++ обратно совместима. Строго по науке нужен mingw810_32
MINGW_DIR="/media/user/big_data/soft/windows/QT_installed/Tools/mingw730_32"

# OpenSSL 1.1 32 бита для Qt 5.15 (https-качалки). Лежит в QtCreator:
# отдельный OpenSSL Toolkit в эту установку не ставился
SSL_DIR="/media/user/big_data/soft/windows/QT_installed/Tools/QtCreator/bin"

# Исходники: корень репозитория от места скрипта
SCRIPT_PATH="$(readlink -e "$0" 2>/dev/null || echo "$0")"
SOURCE_DIR="$(cd "$(dirname "${SCRIPT_PATH}")" && pwd)"

# Каталог сборки. Пусто значит автоопределение: каталог сборки
# Qt Creator (build/*/app/Makefile), самый свежий
BUILD_DIR=""
BUILD_DIR_EXPLICIT=0

# Папка результата. Пусто значит рядом с исходниками
PORTABLE_DIR=""
PORTABLE_DIR_EXPLICIT=0

JOBS="4"
CLEAN_BUILD=0
DO_BUILD=1
DO_STRIP=1
DO_SMOKE=0

# --------------------------------------------------------------------------
# Разбор аргументов
# --------------------------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN_BUILD=1; shift;;
        --jobs) JOBS="$2"; shift 2;;
        --portable-dir)
            PORTABLE_DIR="$2"
            PORTABLE_DIR_EXPLICIT=1
            shift 2
            ;;
        --qt) QT_SDK="$2"; shift 2;;
        --mingw) MINGW_DIR="$2"; shift 2;;
        --ssl) SSL_DIR="$2"; shift 2;;
        --src) SOURCE_DIR="$2"; shift 2;;
        --build-dir)
            BUILD_DIR="$2"
            BUILD_DIR_EXPLICIT=1
            shift 2
            ;;
        --no-build) DO_BUILD=0; shift;;
        --strip) DO_STRIP=1; shift;;
        --no-strip) DO_STRIP=0; shift;;
        --smoke) DO_SMOKE=1; shift;;
        --help) sed -n '2,32p' "$0"; exit 0;;
        *) echo "Неизвестный аргумент: $1" >&2; exit 4;;
    esac
done

# --------------------------------------------------------------------------
# Проверки окружения
# --------------------------------------------------------------------------

if [[ ! -f "${SOURCE_DIR}/mytetra.pro" ]]; then
    echo "ОШИБКА: в SOURCE_DIR нет mytetra.pro: ${SOURCE_DIR}" >&2
    exit 2
fi

if [[ "${PORTABLE_DIR_EXPLICIT}" -eq 0 ]]; then
    PORTABLE_DIR="$(dirname "${SOURCE_DIR}")/MyTetra-portable-win"
fi

if ! command -v wine >/dev/null 2>&1; then
    echo "ОШИБКА: не найден wine" >&2
    exit 2
fi

# Каталог сборки: автоопределение по build/*/app/Makefile
if [[ -z "${BUILD_DIR}" ]]; then
    BUILD_DIR="$(find "${SOURCE_DIR}/build" -maxdepth 3 -name Makefile -path "*/app/*" -printf "%T@ %h\n" 2>/dev/null | sort -rn | head -1 | awk '{print $2}' | xargs -r dirname 2>/dev/null || true)"
    if [[ -n "${BUILD_DIR}" ]]; then
        echo "Каталог сборки Qt Creator: ${BUILD_DIR}"
    fi
fi

# Внимание: каталог сборки Qt Creator для Linux НЕ годится для
# windows-сборки (там linux-Макefайлы). Для Wine нужен отдельный
# каталог, иначе qmake/make перемешаются
if [[ -n "${BUILD_DIR}" && "${BUILD_DIR_EXPLICIT}" -eq 0 ]]; then
    echo "ПРЕДУПРЕЖДЕНИЕ: найден linux-каталог сборки, для Wine он не подходит" >&2
    echo "Укажите отдельный каталог опцией --build-dir" >&2
    BUILD_DIR=""
fi

if [[ -z "${BUILD_DIR}" ]]; then
    echo "ОШИБКА: каталог сборки не задан, укажите --build-dir" >&2
    exit 2
fi

QMAKE="${QT_SDK}/bin/qmake.exe"
MINGW_MAKE="${MINGW_DIR}/bin/mingw32-make.exe"

if [[ ! -f "${QMAKE}" ]]; then
    echo "ОШИБКА: нет qmake.exe: ${QMAKE}, проверьте --qt" >&2
    exit 2
fi

if [[ ! -f "${MINGW_MAKE}" ]]; then
    echo "ОШИБКА: нет mingw32-make.exe: ${MINGW_MAKE}, проверьте --mingw" >&2
    exit 2
fi

echo "Qt SDK:      ${QT_SDK}"
echo "MinGW:       ${MINGW_DIR}"
echo "Исходники:   ${SOURCE_DIR}"
echo "Сборка:      ${BUILD_DIR}"
echo "Результат:   ${PORTABLE_DIR}"
echo "Потоки:      ${JOBS}"
echo

# --------------------------------------------------------------------------
# Сборка под Wine
# --------------------------------------------------------------------------

to_winpath() {
    winepath -w "$1"
}

if [[ "${DO_BUILD}" -eq 1 ]]; then
    if [[ "${CLEAN_BUILD}" -eq 1 && -d "${BUILD_DIR}" ]]; then
        echo "Очищаю сборочный каталог"
        rm -rf "${BUILD_DIR}"
    fi

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # mimetex пишет объекты и бинарь в дерево исходников
    # (thirdParty/mimetex/build) даже при сборке вне исходников.
    # Там могут лежать ELF-артефакты от Linux-сборки: виндовый линкер
    # их не ест ("file not recognized"). Каталог в .gitignore, чистим
    echo "Очищаю артефакты mimetex в исходниках"
    rm -rf "${SOURCE_DIR}/thirdParty/mimetex/build"

    # Структуру каталогов Makefile mimetex сам не создает
    mkdir -p "${SOURCE_DIR}/thirdParty/mimetex/build/obj" \
             "${SOURCE_DIR}/thirdParty/mimetex/build/bin"

    # Тулчейн виден windows-процессам через WINEPATH (аналог PATH
    # внутри Wine): нужен и qmake (проверка компилятора), и make
    export WINEPATH="${MINGW_DIR}/bin"

    # qmake под Wine: пути в windows-виде через winepath.
    # shadow-сборка вне исходников, как в Linux
    wine "${QMAKE}" "$(to_winpath "${SOURCE_DIR}/mytetra.pro")"

    # make с тулчейном в WINEPATH: так windows-процессы находят g++
    wine "${MINGW_MAKE}" -j"${JOBS}"
fi

# --------------------------------------------------------------------------
# Проверка результата сборки
# --------------------------------------------------------------------------

# Windows-qmake кладет бинари в app/release (или app/debug)
BUILT_DIR=""
for cand in "${BUILD_DIR}/app/release" "${BUILD_DIR}/app/debug" "${BUILD_DIR}/app/bin"; do
    if [[ -x "${cand}/mytetra.exe" ]]; then
        BUILT_DIR="${cand}"
        break
    fi
done

if [[ -z "${BUILT_DIR}" ]]; then
    echo "ОШИБКА: не собран mytetra.exe, смотрим в ${BUILD_DIR}/app" >&2
    exit 3
fi

echo "Бинари из: ${BUILT_DIR}"

# mimetex обязателен: без него молча не рисуются формулы в заметках
if [[ ! -f "${BUILT_DIR}/mimetex.exe" ]]; then
    echo "ОШИБКА: не собран mimetex.exe в ${BUILT_DIR}" >&2
    echo "Без него формулы в заметках не отрисуются." >&2
    exit 3
fi

# --------------------------------------------------------------------------
# Сборка результата в портабельную папку
# --------------------------------------------------------------------------

mkdir -p "${PORTABLE_DIR}"

# Старые бинари убираются. Конфиги и данные пользователя НЕ трогаются:
# их в поставке нет, а чужие удалять нельзя
rm -f "${PORTABLE_DIR}/mytetra.exe" "${PORTABLE_DIR}/mimetex.exe"

cp "${BUILT_DIR}/mytetra.exe" "${PORTABLE_DIR}/mytetra.exe"
cp "${BUILT_DIR}/mimetex.exe" "${PORTABLE_DIR}/mimetex.exe"

STRIP_BIN="${MINGW_DIR}/bin/strip.exe"
if [[ "${DO_STRIP}" -eq 1 && -f "${STRIP_BIN}" ]]; then
    wine "${STRIP_BIN}" "$(to_winpath "${PORTABLE_DIR}/mytetra.exe")" \
                         "$(to_winpath "${PORTABLE_DIR}/mimetex.exe")"
fi

# --------------------------------------------------------------------------
# DLL Qt, рантайм MinGW, OpenSSL
# --------------------------------------------------------------------------

# Модули Qt строго по нуждам программы (транзитивный ldd для PE-файлов
# под Linux ненадежен, поэтому список явный; smoke-тест ловит недостачу)
QT_DLLS="Qt5Core Qt5Gui Qt5Widgets Qt5Network Qt5Xml Qt5PrintSupport Qt5Svg"
MINGW_DLLS="libgcc_s_dw2-1 libstdc++-6 libwinpthread-1"

for dll in ${QT_DLLS}; do
    src="${QT_SDK}/bin/${dll}.dll"
    if [[ -f "${src}" ]]; then
        cp "${src}" "${PORTABLE_DIR}/"
    else
        echo "ОШИБКА: нет ${dll}.dll в ${QT_SDK}/bin" >&2
        exit 5
    fi
done

for dll in ${MINGW_DLLS}; do
    src="${MINGW_DIR}/bin/${dll}.dll"
    if [[ -f "${src}" ]]; then
        cp "${src}" "${PORTABLE_DIR}/"
    else
        echo "ПРЕДУПРЕЖДЕНИЕ: нет ${dll}.dll в ${MINGW_DIR}/bin" >&2
    fi
done

# OpenSSL 1.1 для Qt 5.15: Qt грузит через LoadLibrary, в импортах
# mytetra.exe его не видно. Без этих DLL https-качалки не работают,
# программа при этом не падает (см. политику TLS в AGENTS.md)
for dll in libssl-1_1 libcrypto-1_1; do
    if [[ -f "${SSL_DIR}/${dll}.dll" ]]; then
        cp "${SSL_DIR}/${dll}.dll" "${PORTABLE_DIR}/"
    else
        echo "ПРЕДУПРЕЖДЕНИЕ: нет ${dll}.dll: https в портабле работать не будет" >&2
    fi
done

# --------------------------------------------------------------------------
# Плагины Qt
# --------------------------------------------------------------------------

# platforms обязателен (qwindows), imageformats для картинок и svg-иконок,
# iconengines для svg-иконок. Как в Linux: без Qt5Svg рядом все падает,
# он уже скопирован выше
mkdir -p "${PORTABLE_DIR}/plugins"
for plugin in platforms imageformats iconengines; do
    if [[ -d "${QT_SDK}/plugins/${plugin}" ]]; then
        mkdir -p "${PORTABLE_DIR}/plugins/${plugin}"
        cp "${QT_SDK}/plugins/${plugin}/"*.dll "${PORTABLE_DIR}/plugins/${plugin}/" 2>/dev/null || true
    else
        echo "ПРЕДУПРЕЖДЕНИЕ: нет плагинов ${plugin} в ${QT_SDK}/plugins" >&2
    fi
done

# qt.conf: Qt ищет плагины рядом с программой
cat > "${PORTABLE_DIR}/qt.conf" <<'EOF'
[Paths]
Prefix=.
Plugins=plugins
EOF

cat > "${PORTABLE_DIR}/PORTABLE.txt" <<'EOF'
MyTetra portable для Windows
============================
Запуск: mytetra.exe (двойной клик, DLL рядом подхватываются сами)

Первый запуск открывает диалог установки: выбирается портабельный
режим (база и настройки рядом с программой) или стандартный.
Конфигов и данных в поставке нет специально, все создается само.

Глобальный хоткей клиппера вешается средствами Windows на команду:
  "путь\mytetra.exe" --control --clipboard
MyTetra при этом должна быть запущена.
EOF

# --------------------------------------------------------------------------
# Итог
# --------------------------------------------------------------------------

VERSION="$(grep -E '^#define APPLICATION_RELEASE_(VERSION|SUBVERSION|MICROVERSION)' \
              "${SOURCE_DIR}/app/src/main.h" | awk '{print $3}' | paste -sd. -)"

echo "Готово. Версия ${VERSION}"
echo "Содержимое ${PORTABLE_DIR}:"
ls -lh "${PORTABLE_DIR}" | tail -n +2 | awk '{print "  " $9 "  " $5}'
echo "Размер папки: $(du -sh "${PORTABLE_DIR}" | awk '{print $1}')"
echo
echo "Запуск: ${PORTABLE_DIR}/mytetra.exe"

# --------------------------------------------------------------------------
# Дымовой тест: копия результата с зерном базы, запуск в offscreen
# под Wine. Процесс должен жить, падение от недостающих DLL было бы
# сразу. Зерно только в копии, в поставке конфигов и данных нет
# --------------------------------------------------------------------------

if [[ "${DO_SMOKE}" -eq 1 ]]; then
    echo "Дымовой тест"
    SMOKE_DIR="$(mktemp -d)"
    cp -r "${PORTABLE_DIR}/." "${SMOKE_DIR}/"
    mkdir -p "${SMOKE_DIR}/data/base/seed00000000000001"
    cp "${SOURCE_DIR}/app/bin/resource/standartdata/mytetra.xml" "${SMOKE_DIR}/data/" 2>/dev/null || true
    cp "${SOURCE_DIR}/app/bin/resource/standartdata/database.ini" "${SMOKE_DIR}/data/" 2>/dev/null || true
    cp "${SOURCE_DIR}/app/bin/resource/standartdata/base/1300000000aaaaaaaaa2/text.html" "${SMOKE_DIR}/data/base/seed00000000000001/" 2>/dev/null || true
    printf '[General]\nprintdebugmessages=true\n' > "${SMOKE_DIR}/conf.ini"
    ( cd "${SMOKE_DIR}" && QT_QPA_PLATFORM=offscreen timeout 60 wine ./mytetra.exe >smoke.log 2>&1 ) &
    SMOKE_PID=$!
    sleep 45
    if kill -0 $SMOKE_PID 2>/dev/null; then
        echo "Дымовой тест пройден: процесс жив"
        kill $SMOKE_PID 2>/dev/null || true
    else
        echo "ОШИБКА: дымовой тест провален, процесс умер, лог:" >&2
        tail -20 "${SMOKE_DIR}/smoke.log" >&2
        rm -rf "${SMOKE_DIR}"
        exit 6
    fi
    rm -rf "${SMOKE_DIR}"
fi

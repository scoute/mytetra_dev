#!/usr/bin/env bash
# Сборка портабельной версии MyTetra для Windows нативно, без Wine.
# Запуск из Git Bash на самой Windows-машине:
#   ./build_portable_windows_native.sh [--jobs 8] [...]
# Qt для Windows (mingw81_32) и тулчейн берутся из обычной Qt-установки
# (по умолчанию C:\Qt), сборка прямым вызовом qmake.exe и mingw32-make.exe.
#
# Рядом лежит build_portable_windows_wine.sh - тот же результат, но
# сборка под Linux через Wine (медленно, зато без Windows-машины).
#
# В поставке только бинари, DLL Qt и рантайма, плагины, qt.conf.
# Конфигов и данных НЕТ специально: при первом запуске диалог
# установки сам создаст conf.ini, базу и корзину. Привезенный
# conf.ini от другой версии приводил к незапуску с ручной чисткой.
# Ланчер не нужен: Windows ищет DLL рядом с exe сам.
#
# Использование:
#   ./build_portable_windows_native.sh            без параметров: исходники откуда
#                                                запущен скрипт, сборка из каталога
#                                                Qt Creator (если есть), Qt из C:\Qt,
#                                                результат рядом в MyTetra-win-portable
#   ./build_portable_windows_native.sh --clean    очистить сборочный каталог
#   ./build_portable_windows_native.sh --jobs 8   потоки сборки
#   ./build_portable_windows_native.sh --portable-dir /c/dist/MyTetra-win-portable
#   ./build_portable_windows_native.sh --qt /c/Qt/5.15.2/mingw81_32
#   ./build_portable_windows_native.sh --mingw /c/Qt/Tools/mingw810_32
#   ./build_portable_windows_native.sh --ssl /c/OpenSSL-Win32/bin
#   ./build_portable_windows_native.sh --src /c/Qt_projects/mytetra_dev
#   ./build_portable_windows_native.sh --build-dir /c/Qt_projects/mytetra_dev/build/Desktop_Qt_5_15_2_MinGW_32_bit/app
#   ./build_portable_windows_native.sh --no-build  только упаковать готовое
#   ./build_portable_windows_native.sh --strip / --no-strip
#   ./build_portable_windows_native.sh --map      link map рядом с бинарем
#                                                (по адресу падения найти функцию)
#   ./build_portable_windows_native.sh --smoke    тестовый запуск в offscreen
#                                                (процесс должен жить)

set -euo pipefail

# --------------------------------------------------------------------------
# Хардкод параметров. Правьте под свою машину.
# Пути в любом виде, понятном Git Bash: C:/Qt/... или /c/Qt/...
# --------------------------------------------------------------------------

# Qt для Windows: предсобранный mingw81_32
QT_SDK="C:/Qt/5.15.2/mingw81_32"

# Тулчейн MinGW: разрядность обязана совпадать с Qt (32 бита).
# mingw810_32 это ровно тот gcc 8.1 (posix-seh), которым собран сам Qt
MINGW_DIR="C:/Qt/Tools/mingw810_32"

# OpenSSL 1.1 32 бита для Qt 5.15 (https-качалки). Пусто значит не искать:
# на машине сборки его может не быть, тогда https в портабле работать
# не будет (как без него - см. политику TLS в AGENTS.md). Если OpenSSL
# стоит (например C:/OpenSSL-Win32), укажите здесь или опцией --ssl
SSL_DIR=""

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
DO_MAP=0
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
        --map) DO_MAP=1; shift;;
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
    PORTABLE_DIR="$(dirname "${SOURCE_DIR}")/MyTetra-win-portable"
fi

# Нативным windows-процессам пути отдаются в windows-виде.
# Git Bash понимает и C:/... и /c/... дальше сам, но qmake.exe,
# strip.exe и cmd-утилиты ждут именно C:\... - переводим через cygpath
to_winpath() {
    cygpath -w "$1"
}

# Каталог сборки: автоопределение по build/*/app/Makefile
if [[ -z "${BUILD_DIR}" ]]; then
    BUILD_DIR="$(find "${SOURCE_DIR}/build" -maxdepth 3 -name Makefile -path "*/app/*" -printf "%T@ %h\n" 2>/dev/null | sort -rn | head -1 | awk '{print $2}' | xargs -r dirname 2>/dev/null || true)"
    if [[ -n "${BUILD_DIR}" ]]; then
        echo "Каталог сборки Qt Creator: ${BUILD_DIR}"
    fi
fi

# Каталог сборки под другой компилятор (MSVC, Linux) НЕ годится:
# там чужие Makefiles, qmake/make перемешаются
if [[ -n "${BUILD_DIR}" && "${BUILD_DIR_EXPLICIT}" -eq 0 ]]; then
    case "${BUILD_DIR}" in
        *[Mm][Ss][Vv][Cc]*|*linux*|*Linux*|*gcc_64*)
            echo "ПРЕДУПРЕЖДЕНИЕ: найден каталог не-MinGW сборки, он не подходит" >&2
            echo "Укажите отдельный каталог опцией --build-dir" >&2
            BUILD_DIR=""
            ;;
    esac
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

# Тулчейн и Qt в PATH: qmake проверяет компилятор, make ищет g++
export PATH="${MINGW_DIR}/bin:${QT_SDK}/bin:${PATH}"

echo "Qt SDK:      ${QT_SDK}"
echo "MinGW:       ${MINGW_DIR}"
echo "Исходники:   ${SOURCE_DIR}"
echo "Сборка:      ${BUILD_DIR}"
echo "Результат:   ${PORTABLE_DIR}"
echo "Потоки:      ${JOBS}"
echo

# --------------------------------------------------------------------------
# Сборка нативно
# --------------------------------------------------------------------------

if [[ "${DO_BUILD}" -eq 1 ]]; then
    if [[ "${CLEAN_BUILD}" -eq 1 && -d "${BUILD_DIR}" ]]; then
        echo "Очищаю сборочный каталог"
        rm -rf "${BUILD_DIR}"
    fi

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # mimetex пишет объекты и бинарь в дерево исходников
    # (thirdParty/mimetex/build) даже при сборке вне исходников.
    # Там могут лежать артефакты от другой платформы: линкер
    # их не ест ("file not recognized"). Каталог в .gitignore, чистим
    echo "Очищаю артефакты mimetex в исходниках"
    rm -rf "${SOURCE_DIR}/thirdParty/mimetex/build"

    # Структуру каталогов Makefile mimetex сам не создает
    mkdir -p "${SOURCE_DIR}/thirdParty/mimetex/build/obj" \
             "${SOURCE_DIR}/thirdParty/mimetex/build/bin"

    # Штамп версии в win.rc из app/src/main.h (единственный источник).
    # Без FILEVERSION/CompanyName дефендер чаще ругается на безымянный
    # exe (Bearfoos ML). Идемпотентно: при совпадении версии содержимое
    # не меняется, git остается чистым
    {
        MAJ=$(sed -n 's/^#define APPLICATION_RELEASE_VERSION[[:space:]]*//p' "${SOURCE_DIR}/app/src/main.h" | awk '{print $1}')
        SUB=$(sed -n 's/^#define APPLICATION_RELEASE_SUBVERSION[[:space:]]*//p' "${SOURCE_DIR}/app/src/main.h" | awk '{print $1}')
        MIC=$(sed -n 's/^#define APPLICATION_RELEASE_MICROVERSION[[:space:]]*//p' "${SOURCE_DIR}/app/src/main.h" | awk '{print $1}')

        if [[ -z "${MAJ}" || -z "${SUB}" || -z "${MIC}" ]]; then
            echo "ОШИБКА: не прочиталась версия из app/src/main.h" >&2
            exit 1
        fi

        sed -i -E \
            -e "s/^(FILEVERSION )[0-9]+,[0-9]+,[0-9]+,[0-9]+/\1${MAJ},${SUB},${MIC},0/" \
            -e "s/^(PRODUCTVERSION )[0-9]+,[0-9]+,[0-9]+,[0-9]+/\1${MAJ},${SUB},${MIC},0/" \
            -e "s/^([[:space:]]+VALUE \"FileVersion\",[[:space:]]+\")[^\"]*\"/\1${MAJ}.${SUB}.${MIC}\"/" \
            -e "s/^([[:space:]]+VALUE \"ProductVersion\",[[:space:]]+\")[^\"]*\"/\1${MAJ}.${SUB}.${MIC}\"/" \
            "${SOURCE_DIR}/app/desktop/win.rc"

        echo "Версия в win.rc: ${MAJ}.${SUB}.${MIC}"
    }

    # Shadow-сборка вне исходников, как в Linux.
    # --map: link map для разбора адресов падения (nm/addr2line).
    QMAKE_ARGS=()
    if [[ "${DO_MAP}" -eq 1 ]]; then
        QMAKE_ARGS+=("QMAKE_LFLAGS+=-Wl,-Map -Wl,link.map")
    fi
    "${QMAKE}" "$(to_winpath "${SOURCE_DIR}/mytetra.pro")" "${QMAKE_ARGS[@]}"

    "${MINGW_MAKE}" -j"${JOBS}"
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

# link.map кладется линкером рядом с местом линковки
if [[ "${DO_MAP}" -eq 1 ]]; then
    MAP_FILE="$(find "${BUILD_DIR}/app" -maxdepth 2 -name link.map 2>/dev/null | head -1)"
    if [[ -n "${MAP_FILE}" ]]; then
        echo "Link map: ${MAP_FILE}"
        cp "${MAP_FILE}" "${PORTABLE_DIR}/link.map"
    else
        echo "ПРЕДУПРЕЖДЕНИЕ: link.map не создан" >&2
    fi
fi

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
    "${STRIP_BIN}" "$(to_winpath "${PORTABLE_DIR}/mytetra.exe")" \
                   "$(to_winpath "${PORTABLE_DIR}/mimetex.exe")"
fi

# --------------------------------------------------------------------------
# DLL Qt, рантайм MinGW, OpenSSL
# --------------------------------------------------------------------------

# Модули Qt строго по нуждам программы (транзитивный ldd для PE-файлов
# под Linux ненадежен, поэтому список явный; smoke-тест ловит недостачу).
# Qt5Multimedia здесь с голосовых заметок, Qt5Sql с FTS-индекса текстов,
# Qt5Concurrent с параллельного поиска (список обязан повторять QT
# в app/app.pro): без них exe не стартует вообще
QT_DLLS="Qt5Core Qt5Gui Qt5Widgets Qt5Network Qt5Xml Qt5PrintSupport Qt5Svg Qt5Multimedia Qt5Sql Qt5Concurrent"
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
# программа при этом не падает (см. политику TLS в AGENTS.md).
# Источник: сначала SSL_DIR (опция --ssl), иначе вендорные копии
# из thirdParty/win32 (см. fix/575): на сборочной машине OpenSSL
# может не стоять вообще (SSL_DIR по умолчанию пуст). Нет ни там,
# ни там - только предупреждение
SSL_SRC=""
if [[ -n "${SSL_DIR}" && -f "${SSL_DIR}/libssl-1_1.dll" && -f "${SSL_DIR}/libcrypto-1_1.dll" ]]; then
    SSL_SRC="${SSL_DIR}"
elif [[ -f "${SOURCE_DIR}/thirdParty/win32/libssl-1_1.dll" && -f "${SOURCE_DIR}/thirdParty/win32/libcrypto-1_1.dll" ]]; then
    SSL_SRC="${SOURCE_DIR}/thirdParty/win32"
fi

OPENSSL_COPIED=0
if [[ -n "${SSL_SRC}" ]]; then
    echo "OpenSSL из: ${SSL_SRC}"
    for dll in libssl-1_1 libcrypto-1_1; do
        cp "${SSL_SRC}/${dll}.dll" "${PORTABLE_DIR}/"
    done
    OPENSSL_COPIED=1
else
    echo "ПРЕДУПРЕЖДЕНИЕ: нет OpenSSL 1.1 (ни в SSL_DIR, ни в thirdParty/win32): https в портабле работать не будет" >&2
fi

# MSVCR100 для OpenSSL выше: QtCreator/bin собран MSVC 2010, а MinGW
# живет на msvcrt — рантайм не покрывает. Без msvcr100.dll рядом с exe
# libssl/libcrypto не грузятся и https молча не работает на машинах
# без установленного VC++ 2010 (см. fix/571). DLL подлинная, вендорится
# в thirdParty/win32 (редистрибуция разрешена лицензией пакета),
# копируется рядом с exe - Windows ищет DLL там сам
if [[ "${OPENSSL_COPIED}" == "1" ]]; then
    MSVCR100_SRC="${SOURCE_DIR}/thirdParty/win32/msvcr100.dll"
    if [[ -f "${MSVCR100_SRC}" ]]; then
        cp "${MSVCR100_SRC}" "${PORTABLE_DIR}/"
    else
        echo "ОШИБКА: есть OpenSSL, но нет ${MSVCR100_SRC}: https будет сломан" >&2
        exit 6
    fi
fi

# --------------------------------------------------------------------------
# Плагины Qt
# --------------------------------------------------------------------------

# platforms обязателен (qwindows), imageformats для картинок и svg-иконок,
# iconengines для svg-иконок. mediaservice это бэкенд голосовых заметок,
# audio это вывод звука: без них плеер и запись молча не работают.
# sqldrivers обязателен: без qsqlite молча не работает FTS-индекс
# текстов (см. 502), поиск откатывается на линейный без ошибок.
# Как в Linux: без Qt5Svg рядом все падает, он уже скопирован выше
mkdir -p "${PORTABLE_DIR}/plugins"
for plugin in platforms imageformats iconengines mediaservice audio sqldrivers; do
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
# Дымовой тест: копия результата с зерном базы, нативный запуск
# в offscreen. Процесс должен жить, падение от недостающих DLL было бы
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
    ( cd "${SMOKE_DIR}" && QT_QPA_PLATFORM=offscreen ./mytetra.exe >smoke.log 2>&1 ) &
    SMOKE_PID=$!
    sleep 45
    if tasklist //FI "IMAGENAME eq mytetra.exe" 2>/dev/null | grep -q "mytetra.exe"; then
        echo "Дымовой тест пройден: процесс жив"
        taskkill //F //IM "mytetra.exe" >/dev/null 2>&1 || true
    else
        echo "ОШИБКА: дымовой тест провален, процесс умер, лог:" >&2
        tail -20 "${SMOKE_DIR}/smoke.log" >&2
        rm -rf "${SMOKE_DIR}"
        exit 6
    fi
    rm -rf "${SMOKE_DIR}"
fi

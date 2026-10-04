#!/usr/bin/env bash
# Сборка портабельной версии MyTetra для Linux.
# Все компоненты складываются в одну папку: бинарь mytetra, бинарь
# mimetex для формул, копии библиотек Qt и ее зависимостей в lib,
# плагины Qt в plugins, qt.conf и скрипт запуска start.sh.
# Настоящий portable: Qt лежит рядом с программой и не берется из
# системы. Никаких инсталляторов и пакетов.
#
# Конфигов и данных в поставке НЕТ специально: при первом запуске
# диалог установки сам создаст conf.ini, базу и корзину (портабельный
# или стандартный режим на выбор). Привезенный conf.ini от другой
# версии приводил к незапуску с ручной чисткой, больше так не делаем.
#
# Использование:
#   ./build_portable_linux.sh            без параметров: исходники откуда
#                                        запущен скрипт, сборка из каталога
#                                        Qt Creator, Qt SDK из его Makefile,
#                                        результат рядом в MyTetra-lin-portable
#   ./build_portable_linux.sh --clean    предварительно очистить сборочный каталог
#   ./build_portable_linux.sh --jobs 4   число потоков сборки
#   ./build_portable_linux.sh --portable-dir /путь/к/папке   куда сложить сборку
#   ./build_portable_linux.sh --system-libs  копировать и системные библиотеки,
#                                         вплоть до графики X11
#   ./build_portable_linux.sh --qt /путь/к/Qt   корень Qt SDK
#   ./build_portable_linux.sh --src /путь/к/исходникам
#   ./build_portable_linux.sh --build-dir /путь/к/сборке
#                                         (по умолчанию ищется каталог
#                                         сборки Qt Creator в build/)
#   ./build_portable_linux.sh --no-build  не собирать, только упаковать
#                                         готовое из сборочного каталога
#   ./build_portable_linux.sh --strip     вырезать отладочные символы
#   ./build_portable_linux.sh --smoke     тестовый запуск результата в offscreen

set -euo pipefail

# --------------------------------------------------------------------------
# Хардкод параметров сборки. Правьте под свою машину.
# --------------------------------------------------------------------------

# Qt SDK. Основной путь и запасной: на разных машинах Qt стоит в разном месте
QT_SDK="/media/user/data/Qt_installed/5.15.2/gcc_64"
QT_SDK_FALLBACK="/media/user/m2data/Qt_deb11/5.15.2/gcc_64"

# Исходники проекта с файлом mytetra.pro. По умолчанию корень репозитория,
# посчитанный от места скрипта: работает из любой копии без параметров
SCRIPT_PATH="$(readlink -e "$0" 2>/dev/null || echo "$0")"
SOURCE_DIR="$(cd "$(dirname "${SCRIPT_PATH}")" && pwd)"

# Каталог сборки. Пусто значит автоопределение: ищется каталог сборки
# Qt Creator (build/*/app/Makefile), берется самый свежий.
# Явное значение перекрывает автоопределение
BUILD_DIR=""

# Папка готовой портабельной версии. Пусто значит рядом с исходниками,
# считается после разбора аргументов (важен порядок с --src)
PORTABLE_DIR=""
PORTABLE_DIR_EXPLICIT=0

# Число потоков по умолчанию
JOBS="$(nproc)"

# Очистка сборочного каталога перед сборкой: 1 да, 0 нет
CLEAN_BUILD=0

# Копировать ли системные библиотеки (X11, GL, Kerberos). По умолчанию
# берем системные: на любой desktop-машине они есть, а папка остается
# разумного размера. Для переноса на голую систему без X11: --system-libs
COPY_SYSTEM_LIBS=0

# Собирать ли исходники (1) или только упаковать готовое (0)
DO_BUILD=1

# Вырезать ли отладочные символы из бинарей (1 да, 0 нет)
DO_STRIP=1

# Тестовый запуск результата в offscreen (1 да, 0 нет)
DO_SMOKE=0

# --------------------------------------------------------------------------
# Разбор аргументов
# --------------------------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --portable-dir)
            PORTABLE_DIR="$2"
            PORTABLE_DIR_EXPLICIT=1
            shift 2
            ;;
        --system-libs)
            COPY_SYSTEM_LIBS=1
            shift
            ;;
        --qt)
            QT_SDK="$2"
            QT_SDK_FALLBACK=""
            shift 2
            ;;
        --src)
            SOURCE_DIR="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --no-build)
            DO_BUILD=0
            shift
            ;;
        --strip)
            DO_STRIP=1
            shift
            ;;
        --no-strip)
            DO_STRIP=0
            shift
            ;;
        --smoke)
            DO_SMOKE=1
            shift
            ;;
        --help)
            sed -n '2,30p' "$0"
            exit 0
            ;;
        *)
            echo "Неизвестный аргумент: $1" >&2
            exit 4
            ;;
    esac
done

# --------------------------------------------------------------------------
# Проверки окружения
# --------------------------------------------------------------------------

if [[ ! -f "${SOURCE_DIR}/mytetra.pro" ]]; then
    echo "ОШИБКА: в SOURCE_DIR нет mytetra.pro: ${SOURCE_DIR}" >&2
    exit 2
fi

# Папка результата по умолчанию рядом с исходниками
if [[ "${PORTABLE_DIR_EXPLICIT}" -eq 0 ]]; then
    PORTABLE_DIR="$(dirname "${SOURCE_DIR}")/MyTetra-lin-portable"
fi

# Каталог сборки: если не задан явно, ищется каталог сборки Qt Creator
# (build/*/app/Makefile), берется самый свежий. Тогда же собирается
# в нем, исходники не засоряются
if [[ -z "${BUILD_DIR}" ]]; then
    BUILD_DIR="$(find "${SOURCE_DIR}/build" -maxdepth 3 -name Makefile -path "*/app/*" -printf "%T@ %h\n" 2>/dev/null | sort -rn | head -1 | awk '{print $2}' | xargs -r dirname 2>/dev/null || true)"
    if [[ -n "${BUILD_DIR}" ]]; then
        echo "Каталог сборки Qt Creator: ${BUILD_DIR}"
    else
        echo "ОШИБКА: каталог сборки не найден в ${SOURCE_DIR}/build" >&2
        echo "Соберите проект в Qt Creator или задайте --build-dir." >&2
        exit 2
    fi
fi

# Qt SDK: сначала явные варианты, затем qmake из каталога сборки
# (Qt Creator прописывает его в app/Makefile), затем qmake из PATH
if [[ ! -x "${QT_SDK}/bin/qmake" ]]; then
    if [[ -n "${QT_SDK_FALLBACK}" && -x "${QT_SDK_FALLBACK}/bin/qmake" ]]; then
        QT_SDK="${QT_SDK_FALLBACK}"
    fi
fi

if [[ ! -x "${QT_SDK}/bin/qmake" ]]; then
    MAKEFILE_QMAKE="$(grep -m1 "^QMAKE" "${BUILD_DIR}/app/Makefile" 2>/dev/null | awk -F'= *' '{print $2}' || true)"
    if [[ -n "${MAKEFILE_QMAKE}" && -x "${MAKEFILE_QMAKE}" ]]; then
        QT_SDK="$(dirname "$(dirname "${MAKEFILE_QMAKE}")")"
        echo "Qt SDK из каталога сборки: ${QT_SDK}"
    fi
fi

if [[ ! -x "${QT_SDK}/bin/qmake" ]] && command -v qmake >/dev/null 2>&1; then
    QT_SDK="$(qmake -query QT_INSTALL_PREFIX)"
fi

if [[ ! -x "${QT_SDK}/bin/qmake" ]]; then
    echo "ОШИБКА: не найден qmake" >&2
    echo "Проверьте QT_SDK в начале скрипта или опцию --qt." >&2
    exit 2
fi

QMAKE="${QT_SDK}/bin/qmake"
MAKE_BIN="$(command -v make || true)"

if [[ -z "${MAKE_BIN}" ]]; then
    echo "ОШИБКА: не найден make" >&2
    exit 2
fi

echo "Qt SDK:      ${QT_SDK}"
echo "Исходники:   ${SOURCE_DIR}"
echo "Сборка:      ${BUILD_DIR}"
echo "Результат:   ${PORTABLE_DIR}"
echo "Потоки:      ${JOBS}"
echo

# --------------------------------------------------------------------------
# Сборка
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
    # Там могут лежать артефакты другой платформы (PE после Wine):
    # чужой линкер их не ест. Каталог в .gitignore, чистим
    rm -rf "${SOURCE_DIR}/thirdParty/mimetex/build"
    mkdir -p "${SOURCE_DIR}/thirdParty/mimetex/build/obj" \
             "${SOURCE_DIR}/thirdParty/mimetex/build/bin"

    # qmake создает Makefile, make собирает. mimetex собирается как отдельный
    # проект и копируется рядом с mytetra, поэтому собирается весь mytetra.pro
    "${QMAKE}" "${SOURCE_DIR}/mytetra.pro"
    "${MAKE_BIN}" -j"${JOBS}"
fi

# --------------------------------------------------------------------------
# Проверка результата сборки
# --------------------------------------------------------------------------

BUILT_DIR="${BUILD_DIR}/app/bin"

if [[ ! -x "${BUILT_DIR}/mytetra" ]]; then
    echo "ОШИБКА: не собран mytetra в ${BUILT_DIR}" >&2
    exit 3
fi

# mimetex обязателен: без него молча не рисуются формулы в заметках
if [[ ! -x "${BUILT_DIR}/mimetex" ]]; then
    echo "ОШИБКА: не собран mimetex в ${BUILT_DIR}" >&2
    echo "Без него формулы в заметках не отрисуются." >&2
    exit 3
fi

# --------------------------------------------------------------------------
# Сборка результата в портабельную папку
# --------------------------------------------------------------------------

mkdir -p "${PORTABLE_DIR}"

# Старые бинари убираются, чтобы в папке не осталось прошлой сборки.
# Конфиги и данные пользователя НЕ трогаются: их в поставке нет,
# а чужие удалять нельзя
rm -f "${PORTABLE_DIR}/mytetra" "${PORTABLE_DIR}/mimetex"

cp "${BUILT_DIR}/mytetra" "${PORTABLE_DIR}/mytetra"
cp "${BUILT_DIR}/mimetex" "${PORTABLE_DIR}/mimetex"

chmod +x "${PORTABLE_DIR}/mytetra" "${PORTABLE_DIR}/mimetex"

if [[ "${DO_STRIP}" -eq 1 ]] && command -v strip >/dev/null 2>&1; then
    strip "${PORTABLE_DIR}/mytetra" "${PORTABLE_DIR}/mimetex"
fi

# --------------------------------------------------------------------------
# Копирование библиотек Qt и ее зависимостей внутрь папки
# --------------------------------------------------------------------------

# Базовые библиотеки системы, которые берутся из системы всегда: они
# часть дистрибутива и подменять их нельзя, иначе программа не стартует
SYSTEM_LIBS_PATTERN='^(libc|libm|libdl|libpthread|librt|libresolv|libnsl|libutil|ld-linux)'

# Системные библиотеки, которые можно утащить внутрь папки по желанию:
# графика, шрифты и сетевые библиотечки Kerberos
OPTIONAL_LIBS_PATTERN='^(libX|libxcb|libGL|libglib|libgthread|libgmodule|libgobject|libgio|libEGL|libgdk|libpango|libcairo|libfontconfig|libfreetype|libkrb5|libk5crypto|libkrb5support|libcom_err|libgssapi|libkeyutils|libbsd|libmd)'

LIB_DIR="${PORTABLE_DIR}/lib"
PLUGINS_DIR="${PORTABLE_DIR}/plugins"

rm -rf "${LIB_DIR}" "${PLUGINS_DIR}"
mkdir -p "${LIB_DIR}" "${PLUGINS_DIR}"

# Пути всех библиотек, от которых зависит собранный mytetra.
# mimetex зависит только от libc, поэтому его библиотеки отдельно не нужны
mapfile -t NEEDED_LIBS < <(LD_LIBRARY_PATH="${QT_SDK}/lib" ldd "${PORTABLE_DIR}/mytetra" | awk '{print $3}' | grep "^/")

COPIED_LIBS=0
for lib in "${NEEDED_LIBS[@]}"; do
    [[ -f "${lib}" ]] || continue

    name="$(basename "${lib}")"

    # Базовые системные библиотеки остаются в системе
    if [[ "${name}" =~ ${SYSTEM_LIBS_PATTERN} ]]; then
        continue
    fi

    # Графика и прочее системное копируется только по запросу
    if [[ "${COPY_SYSTEM_LIBS}" -eq 0 && "${name}" =~ ${OPTIONAL_LIBS_PATTERN} ]]; then
        continue
    fi

    # Копируется сам файл библиотеки и ссылка с именем, по которому ее
    # ищет загрузчик. Ссылки из каталога Qt переносить нельзя: они
    # указывают на версионные имена, которых в папке не будет
    real_lib="$(readlink -f "${lib}")"

    if [[ ! -f "${real_lib}" ]]; then
        echo "ПРЕДУПРЕЖДЕНИЕ: не найден файл библиотеки ${lib}" >&2
        continue
    fi

    cp -a "${real_lib}" "${LIB_DIR}/"

    if [[ "${name}" != "$(basename "${real_lib}")" ]]; then
        ln -sf "$(basename "${real_lib}")" "${LIB_DIR}/${name}"
    fi

    COPIED_LIBS=$((COPIED_LIBS + 1))
done

# Все библиотеки Qt копируются целиком, а не только те, что видны в ldd
# mytetra. Некоторые нужны только плагинам: например, svg-плагины
# иконок и форматов требуют Qt5Svg, а сама программа ее не линкует.
# Без нее Qt падает при загрузке плагинов, и это выглядит как
# аварийный старт без всякого сообщения
# Qt-модули, которые нужны программе и ее плагинам. Перечислены явно,
# а не все libQt5* из SDK: лишние модули весят сотни мегабайт
QT_MODULES="Core Gui Widgets Network Xml PrintSupport Svg DBus"

for module in ${QT_MODULES}; do
    for qt_lib in "${QT_SDK}"/lib/libQt5"${module}".so.5.*; do
        # Отладочные символы весят в разы больше самой библиотеки
        [[ "${qt_lib}" == *.debug ]] && continue

        [[ -f "${qt_lib}" ]] || continue

        qt_name="$(basename "${qt_lib}")"
        cp -a "${qt_lib}" "${LIB_DIR}/"

        # Ссылка с именем, по которому библиотеку ищет загрузчик
        if [[ ! -e "${LIB_DIR}/libQt5${module}.so.5" ]]; then
            ln -sf "${qt_name}" "${LIB_DIR}/libQt5${module}.so.5"
        fi

        COPIED_LIBS=$((COPIED_LIBS + 1))
    done
done

# --------------------------------------------------------------------------
# Плагины Qt: платформа отображения, форматы картинок, иконки.
# Копируются ДО транзитивного добора: добор считает их зависимости
# --------------------------------------------------------------------------

# platforms обязателен: без него Qt не выберет графическую подсистему,
# imageformats нужен для svg-иконок, iconengines для svg-иконок на файловой
# системе. platforminputcontexts не копируется: его плагин ввода через
# ibus падает при старте в окружении без DBus, а базовый ввод с клавиатуры
# обеспечивается самой платформой
for plugin in platforms imageformats iconengines; do
    if [[ -d "${QT_SDK}/plugins/${plugin}" ]]; then
        mkdir -p "${PLUGINS_DIR}/${plugin}"
        # Отладочные символы *.debug не нужны и весят половину объема
        find "${QT_SDK}/plugins/${plugin}" -maxdepth 1 -type f -name '*.so' \
             -exec cp -a {} "${PLUGINS_DIR}/${plugin}/" \;
    else
        echo "ПРЕДУПРЕЖДЕНИЕ: не найдены плагины ${plugin} в ${QT_SDK}/plugins" >&2
    fi
done

# Транзитивный добор: зависимости скопированных либ и плагинов
# (ICU, XcbQpa и прочие, которых нет в явном списке). Без этого
# набор хрупкий: добавь Qt новую зависимость и портабл умрет
for target in "${LIB_DIR}"/libQt5*.so* "${PLUGINS_DIR}"/*/*.so; do
    [[ -f "${target}" ]] || continue
    while read -r dep; do
        [[ -f "${dep}" ]] || continue
        case "${dep}" in
            "${QT_SDK}"/lib/*)
                dep_name="$(basename "${dep}")"
                if [[ ! -e "${LIB_DIR}/${dep_name}" ]]; then
                    cp -L "${dep}" "${LIB_DIR}/"
                    COPIED_LIBS=$((COPIED_LIBS + 1))
                fi
                ;;
        esac
    done < <(LD_LIBRARY_PATH="${QT_SDK}/lib" ldd "${target}" 2>/dev/null | awk '{print $3}' | grep "^/")
done

# OpenSSL 1.1 для Qt 5.15: Qt грузит его через dlopen, в ldd его нет.
# Копируется если нашелся рядом с Qt или в системе, иначе предупреждение:
# https-качалки на такой машине не заработают
for sslname in libssl.so.1.1 libcrypto.so.1.1; do
    if [[ ! -f "${LIB_DIR}/${sslname}" ]]; then
        for cand in "${QT_SDK}/lib/${sslname}" "/usr/lib/x86_64-linux-gnu/${sslname}" "/lib/x86_64-linux-gnu/${sslname}" "/usr/lib/${sslname}"; do
            if [[ -f "${cand}" ]]; then
                cp -L "${cand}" "${LIB_DIR}/"
                break
            fi
        done
    fi
done
if [[ ! -f "${LIB_DIR}/libssl.so.1.1" ]]; then
    echo "ПРЕДУПРЕЖДЕНИЕ: libssl.so.1.1 не найден: https в портабле работать не будет" >&2
fi

# Проверка на смешивание версий Qt: ни одна забираемая либа не должна
# резолвиться в системный Qt. Иначе будет как с qsvg: падение на старте
# без сообщений, чинится только разбором ldd
MIXED=0
for target in "${PORTABLE_DIR}/mytetra" "${PLUGINS_DIR}"/*/*.so; do
    [[ -f "${target}" ]] || continue
    BAD="$(LD_LIBRARY_PATH="${LIB_DIR}" ldd "${target}" 2>/dev/null | grep "libQt5" | grep -v "${LIB_DIR}" || true)"
    if [[ -n "${BAD}" ]]; then
        echo "ОШИБКА: смешивание Qt в ${target}:" >&2
        echo "${BAD}" >&2
        MIXED=1
    fi
done
if [[ "${MIXED}" -ne 0 ]]; then
    echo "ОШИБКА: портабл тянет системный Qt, чинить зависимости" >&2
    exit 5
fi

# qt.conf говорит Qt, где искать библиотеки и плагины рядом с программой
cat > "${PORTABLE_DIR}/qt.conf" <<'EOF'
[Paths]
Prefix=.
Plugins=plugins
EOF

# --------------------------------------------------------------------------
# Скрипт запуска с путями к своим библиотекам
# --------------------------------------------------------------------------

# Пути задаются прямо в скрипте, как того требует портабельная сборка:
# запуск идет из любого каталога, директория определяется по самому скрипту.
# Аргументы пробрасываются: это нужно в том числе для mytetra --control
# (глобальный хоткей клиппера из ОС)
LAUNCHER="${PORTABLE_DIR}/start.sh"

cat > "${LAUNCHER}" <<EOF
#!/usr/bin/env bash
# Запуск портабельной версии MyTetra.
# Пути к библиотекам и плагинам рядом с программой заданы вручную,
# поэтому Qt берется из этой папки, а не из системы

set -e

# Каталог этой папки, работает при запуске из любого места
PORTABLE_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"

# Свои библиотеки впереди системных
export LD_LIBRARY_PATH="\${PORTABLE_DIR}/lib:\${LD_LIBRARY_PATH}"

# Плагины Qt (платформа, картинки, иконки)
export QT_PLUGIN_PATH="\${PORTABLE_DIR}/plugins"

exec "\${PORTABLE_DIR}/mytetra" "\$@"
EOF

chmod +x "${LAUNCHER}"

cat > "${PORTABLE_DIR}/PORTABLE.txt" <<'EOF'
MyTetra portable для Linux
==========================
Запуск: ./start.sh

Первый запуск открывает диалог установки: выбирается портабельный
режим (база и настройки рядом с программой) или стандартный.
Конфигов и данных в поставке нет специально, все создается само.

Глобальный хоткей клиппера (пример Ctrl+Q) вешается средствами
рабочего стола на команду:
  /путь/к/start.sh --control --clipboard
MyTetra при этом должна быть запущена.
EOF

# --------------------------------------------------------------------------
# Итог
# --------------------------------------------------------------------------

VERSION="$(grep -E '^#define APPLICATION_RELEASE_(VERSION|SUBVERSION|MICROVERSION)' \
              "${SOURCE_DIR}/app/src/main.h" | awk '{print $3}' | paste -sd. -)"

echo "Готово. Версия ${VERSION}"
echo "Бинари: mytetra, mimetex (Qt-библиотеки внутри: ${COPIED_LIBS} шт)"
echo "Содержимое ${PORTABLE_DIR}:"
ls -lh "${PORTABLE_DIR}" | tail -n +2 | awk '{print "  " $9 "  " $5}'
echo "Размер папки: $(du -sh "${PORTABLE_DIR}" | awk '{print $1}')"
echo
echo "Запуск: ${LAUNCHER}"

# --------------------------------------------------------------------------
# Дымовой тест: копия результата с зерном базы, запуск в offscreen.
# Процесс должен жить (GUI без дисплея висит в event loop,
# падение было бы сразу). Зерно кладется только в копию,
# в поставке конфигов и данных нет
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
    ( cd "${SMOKE_DIR}" && QT_QPA_PLATFORM=offscreen timeout 15 ./start.sh >smoke.log 2>&1 ) &
    SMOKE_PID=$!
    sleep 12
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

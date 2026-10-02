#!/usr/bin/env bash
# Сборка портабельной версии MyTetra для Linux.
# Все компоненты складываются в одну папку: бинарь mytetra, бинарь
# mimetex для формул, conf.ini для портабельного режима (программа
# берет настройки из conf.ini рядом с бинарником и не трогает данные
# пользователя в домашнем каталоге), копии библиотек Qt и ее
# зависимостей в lib, плагины Qt в plugins и скрипт запуска start.sh.
# Настоящий portable: Qt лежит рядом с программой и не берется из
# системы. Никаких инсталляторов и пакетов.
#
# Использование:
#   ./build_portable_linux.sh            сборка с настройками по умолчанию
#   ./build_portable_linux.sh --clean    предварительно очистить сборочный каталог
#   ./build_portable_linux.sh --jobs 4   число потоков сборки
#   ./build_portable_linux.sh --portable-dir /путь/к/папке   куда сложить сборку
#   ./build_portable_linux.sh --system-libs  копировать и системные библиотеки,
#                                         вплоть до графики X11

set -euo pipefail

# --------------------------------------------------------------------------
# Хардкод параметров сборки. Правьте под свою машину.
# --------------------------------------------------------------------------

# Qt SDK. Основной путь и запасной: на разных машинах Qt стоит в разном месте
QT_SDK="/media/user/data/Qt_installed/5.15.2/gcc_64"
QT_SDK_FALLBACK="/media/user/m2data/Qt_deb11/5.15.2/gcc_64"

# Исходники проекта с файлом mytetetra.pro
SOURCE_DIR="/home/user/_TMP/opencode/sco-mytetra-dev"

# Каталог сборки вне исходников, чтобы не засорять репозиторий
BUILD_DIR="/tmp/opencode/mt-portable-build"

# Папка готовой портабельной версии
PORTABLE_DIR="/home/user/_TMP/opencode/MyTetra-portable"

# Число потоков по умолчанию
JOBS="$(nproc)"

# Очистка сборочного каталога перед сборкой: 1 да, 0 нет
CLEAN_BUILD=0

# Копировать ли системные библиотеки (X11, GL, Kerberos). По умолчанию
# берем системные: на любой desktop-машине они есть, а папка остается
# разумного размера. Для переноса на голую систему без X11: --system-libs
COPY_SYSTEM_LIBS=0

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
            shift 2
            ;;
        --system-libs)
            COPY_SYSTEM_LIBS=1
            shift
            ;;
        --help)
            sed -n '2,18p' "$0"
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

# Qt SDK: берется первый существующий из двух вариантов
if [[ ! -x "${QT_SDK}/bin/qmake" ]]; then
    if [[ -x "${QT_SDK_FALLBACK}/bin/qmake" ]]; then
        QT_SDK="${QT_SDK_FALLBACK}"
    else
        echo "ОШИБКА: не найден qmake" >&2
        echo "Проверьте QT_SDK и QT_SDK_FALLBACK в начале скрипта." >&2
        exit 2
    fi
fi

if [[ ! -f "${SOURCE_DIR}/mytetra.pro" ]]; then
    echo "ОШИБКА: в SOURCE_DIR нет mytetra.pro: ${SOURCE_DIR}" >&2
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

if [[ "${CLEAN_BUILD}" -eq 1 && -d "${BUILD_DIR}" ]]; then
    echo "Очищаю сборочный каталог"
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# qmake создает Makefile, make собирает. mimetex собирается как отдельный
# проект и копируется рядом с mytetra, поэтому собирается весь mytetra.pro
"${QMAKE}" "${SOURCE_DIR}/mytetra.pro"
"${MAKE_BIN}" -j"${JOBS}"

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

# conf.ini для портабельного режима берется из ресурсов проекта
CONF_TEMPLATE="${SOURCE_DIR}/app/bin/resource/standartconfig/any/conf.ini"

if [[ ! -f "${CONF_TEMPLATE}" ]]; then
    echo "ОШИБКА: нет шаблона conf.ini: ${CONF_TEMPLATE}" >&2
    exit 3
fi

# --------------------------------------------------------------------------
# Сборка результата в портабельную папку
# --------------------------------------------------------------------------

mkdir -p "${PORTABLE_DIR}"

# Старые бинари убираются, чтобы в папке не осталось прошлой сборки
rm -f "${PORTABLE_DIR}/mytetra" "${PORTABLE_DIR}/mimetex" "${PORTABLE_DIR}/conf.ini"

cp "${BUILT_DIR}/mytetra" "${PORTABLE_DIR}/mytetra"
cp "${BUILT_DIR}/mimetex" "${PORTABLE_DIR}/mimetex"
cp "${CONF_TEMPLATE}" "${PORTABLE_DIR}/conf.ini"

chmod +x "${PORTABLE_DIR}/mytetra" "${PORTABLE_DIR}/mimetex"

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
mapfile -t NEEDED_LIBS < <(ldd "${PORTABLE_DIR}/mytetra" | awk '{print $3}' | grep "^/")

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
# Плагины Qt: платформа отображения, форматы картинок, иконки
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
# запуск идет из любого каталога, директория определяется по самому скрипту
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

# --------------------------------------------------------------------------
# Итог
# --------------------------------------------------------------------------

VERSION="$(grep -E '^#define APPLICATION_RELEASE_(VERSION|SUBVERSION|MICROVERSION)' \
              "${SOURCE_DIR}/app/src/main.h" | awk '{print $3}' | paste -sd. -)"

echo "Готово. Версия ${VERSION}"
echo "Бинарей: mytetra, mimetex (Qt-библиотеки внутри: ${COPIED_LIBS} шт)"
echo "Содержимое ${PORTABLE_DIR}:"
ls -lh "${PORTABLE_DIR}" | tail -n +2 | awk '{print "  " $9 "  " $5}'
echo "Размер папки: $(du -sh "${PORTABLE_DIR}" | awk '{print $1}')"
echo
echo "Запуск: ${LAUNCHER}"
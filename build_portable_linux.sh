#!/usr/bin/env bash
# Сборка портабельной версии MyTetra для Linux.
# Все компоненты складываются в одну папку: бинарь mytetra, бинарь
# mimetex для формул и conf.ini для портабельного режима (программа
# берет настройки из conf.ini рядом с бинарником и не трогает данные
# пользователя в домашнем каталоге). Никаких инсталляторов и пакетов.
#
# Использование:
#   ./build_portable_linux.sh            сборка с настройками по умолчанию
#   ./build_portable_linux.sh --clean    предварительно очистить сборочный каталог
#   ./build_portable_linux.sh --jobs 4   число потоков сборки
#   ./build_portable_linux.sh --portable-dir /путь/к/папке   куда сложить сборку

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
        --help)
            sed -n '2,15p' "$0"
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
# Итог
# --------------------------------------------------------------------------

VERSION="$(grep -E '^#define APPLICATION_RELEASE_(VERSION|SUBVERSION|MICROVERSION)' \
              "${SOURCE_DIR}/app/src/main.h" | awk '{print $3}' | paste -sd. -)"

echo "Готово. Версия ${VERSION}"
echo "Содержимое ${PORTABLE_DIR}:"
ls -lh "${PORTABLE_DIR}" | tail -n +2 | awk '{print "  " $9 "  " $5}'

# Подсказка про первый запуск в headless: без графического экрана
# программу запускать нельзя, иначе она упадет или откроет пустое окно
echo
echo "Запуск: cd \"${PORTABLE_DIR}\" && ./mytetra"
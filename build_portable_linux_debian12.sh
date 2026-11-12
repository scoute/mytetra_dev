#!/usr/bin/env bash
# Портабельная сборка MyTetra именно для Debian 12 (и LMDE6)
# с системным Qt 5.15. Сначала проверяются все компоненты,
# нужные для ПОЛНОЦЕННОЙ сборки. Сам скрипт от root НИЧЕГО
# не делает: если чего-то не хватает - он печатает готовую
# команду доустановки и падает с явной ошибкой. Итог строго
# один из двух: либо нормальная полноценная сборка,
# либо явная ошибка с подсказкой. Молчаливых "почти собралось"
# здесь нет: пропущенный плагин или модуль - это повод упасть.
#
# Использование:
#   ./build_portable_linux_debian12.sh [--jobs N] [аргументы build_portable_linux.sh]
# Аргументы после проверок передаются в build_portable_linux.sh как есть.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_SCRIPT="${SCRIPT_DIR}/build_portable_linux.sh"

fail() {
    echo "ОШИБКА: $*" >&2
    exit 1
}

# 0. Только Debian-семейство: пакеты ниже существуют только там
if [[ ! -f /etc/os-release ]]; then
    fail "нет /etc/os-release, дистрибутив не опознан"
fi

OS_ID="$(grep -m1 '^ID=' /etc/os-release | cut -d= -f2 | tr -d '"')"
OS_LIKE="$(grep -m1 '^ID_LIKE=' /etc/os-release | cut -d= -f2 | tr -d '\"' || true)"

if [[ "${OS_ID}" != "debian" && "${OS_LIKE}" != *debian* ]]; then
    fail "скрипт только для Debian 12 и производных ( LMDE ), обнаружен: ${OS_ID} (like: ${OS_LIKE})"
fi
# 1. Права root скрипту НЕ нужны и НЕ используются.
# Все недостающее ставит сам пользователь руками (см. раздел 4)

# Явная проверка каждой зависимости в стиле configure:
#   check g++ ............................ OK
#   check plugin mediaservice ............ MISSING (пакет: libqt5multimedia5-plugins)
# Молчаливых пропусков нет: каждая строка видна сразу
NEED_PKGS_STR=""

check_item() {
    local label="$1"
    local pkgs="$2"
    local cond="$3"
    local dots
    dots="$(printf '%*s' $((46 - ${#label})) '' | tr ' ' '.')"
    if eval "${cond}" >/dev/null 2>&1; then
        echo "check ${label} ${dots} OK"
    else
        echo "check ${label} ${dots} not found"
        echo "please apt install ${pkgs}"
        NEED_PKGS_STR="${NEED_PKGS_STR} ${pkgs}"
    fi
}

# 2. Обязательные инструменты
check_item "tool g++" "build-essential" "command -v g++"
check_item "tool make" "build-essential" "command -v make"
check_item "tool qmake" "qt5-qmake" "command -v qmake"
check_item "tool git" "git" "command -v git"

# 3. Инструменты Qt: qmake обязан резолвиться и отдавать пути
if command -v qmake >/dev/null 2>&1; then
    QT_HEADERS="$(qmake -query QT_INSTALL_HEADERS 2>/dev/null || true)"
    QT_PLUGINS="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
else
    QT_HEADERS=""
    QT_PLUGINS=""
fi

[[ -n "${QT_HEADERS}" && -d "${QT_HEADERS}/QtWidgets" ]] || NEED_PKGS+=(qtbase5-dev)
[[ -n "${QT_PLUGINS}" && -d "${QT_PLUGINS}" ]] || NEED_PKGS+=(qtbase5-dev)

# 3a. Заголовки Qt-модулей, используемых приложением (см. QT в app/app.pro).
# Проверяются файлы, а не пакеты: пакет уже может стоять частично
check_item "headers QtWidgets" "qtbase5-dev" '[[ -n "${QT_HEADERS}" && -f "${QT_HEADERS}/QtWidgets/QtWidgets" ]]'
check_item "headers QtSvg" "libqt5svg5-dev" '[[ -n "${QT_HEADERS}" && -f "${QT_HEADERS}/QtSvg/QtSvg" ]]'
check_item "headers QtMultimedia" "qtmultimedia5-dev" '[[ -n "${QT_HEADERS}" && -f "${QT_HEADERS}/QtMultimedia/QtMultimedia" ]]'
check_item "tool lrelease" "qttools5-dev" "command -v lrelease"
check_item "plugin sqldrivers" "libqt5sql5-sqlite" '[[ -n "${QT_PLUGINS}" && -f "${QT_PLUGINS}/sqldrivers/libqsqlite.so" ]]'

# 3b. Плагины Qt: платформа (десктоп и offscreen для smoke-теста),
# картинки, иконки, звуковой бэкенд голосовых заметок
check_item "plugin platforms/qxcb" "qtbase5-dev" '[[ -n "${QT_PLUGINS}" && -f "${QT_PLUGINS}/platforms/libqxcb.so" ]]'
check_item "plugin platforms/qoffscreen" "qtbase5-dev" '[[ -n "${QT_PLUGINS}" && -f "${QT_PLUGINS}/platforms/libqoffscreen.so" ]]'
check_item "plugin imageformats/qsvg" "qtbase5-dev" '[[ -n "${QT_PLUGINS}" && -f "${QT_PLUGINS}/imageformats/libqsvg.so" ]]'
check_item "plugin imageformats/qjpeg" "qtbase5-dev" '[[ -n "${QT_PLUGINS}" && -f "${QT_PLUGINS}/imageformats/libqjpeg.so" ]]'
check_item "plugin iconengines" "qtbase5-dev" '[[ -n "${QT_PLUGINS}" ]] && compgen -G "${QT_PLUGINS}/iconengines/*.so"'
check_item "plugin mediaservice" "libqt5multimedia5-plugins" '[[ -n "${QT_PLUGINS}" ]] && compgen -G "${QT_PLUGINS}/mediaservice/*.so"'
check_item "plugin audio" "libqt5multimedia5-plugins" '[[ -n "${QT_PLUGINS}" ]] && compgen -G "${QT_PLUGINS}/audio/*.so"'

# 3c. Переводы строк Qt (qttranslations5-l10n)
check_item "translations qt_*.qm" "qttranslations5-l10n" 'compgen -G "/usr/share/qt5/translations/qt_*.qm"'

# 4. Недостающее НЕ ставится автоматически: печатаем готовую
# команду и падаем. Ставит пакеты сам пользователь руками
# shellcheck disable=SC2207
UNIQUES=($(printf "%s\n" ${NEED_PKGS_STR} | sort -u))
if [[ "${#UNIQUES[@]}" -gt 0 ]]; then
    echo "" >&2
    echo "ОШИБКА: для полноценной сборки не хватает пакетов: ${UNIQUES[*]}" >&2
    echo "" >&2
    echo "Доустановите их одной командой:" >&2
    echo "  sudo apt-get update && sudo apt-get install -y ${UNIQUES[*]}" >&2
    echo "" >&2
    echo "Затем запустите скрипт снова." >&2
    exit 2
fi

echo "all OK, starting build"

# 5. Делегирование общей сборке со всеми аргументами пользователя
[[ -x "${BASE_SCRIPT}" ]] || fail "нет базового скрипта ${BASE_SCRIPT}"

exec "${BASE_SCRIPT}" "$@"

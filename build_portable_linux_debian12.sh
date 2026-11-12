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

# 2. Обязательные инструменты
NEED_PKGS=()
command -v g++ >/dev/null 2>&1 || NEED_PKGS+=(build-essential)
command -v make >/dev/null 2>&1 || NEED_PKGS+=(build-essential)
command -v qmake >/dev/null 2>&1 || NEED_PKGS+=(qt5-qmake)
command -v git >/dev/null 2>&1 || NEED_PKGS+=(git)

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
check_header() {
    [[ -n "${QT_HEADERS}" && -f "${QT_HEADERS}/$1" ]]
}

check_header "QtWidgets/QtWidgets"            || NEED_PKGS+=(qtbase5-dev)
command -v lrelease >/dev/null 2>&1           || NEED_PKGS+=(qttools5-dev)
check_header "QtSvg/QtSvg"                    || NEED_PKGS+=(libqt5svg5-dev)
check_header "QtMultimedia/QtMultimedia"       || NEED_PKGS+=(qtmultimedia5-dev)
{ [[ -n "${QT_PLUGINS}" && -f "${QT_PLUGINS}/sqldrivers/libqsqlite.so" ]]; } || NEED_PKGS+=(libqt5sql5-sqlite)

# 3b. Плагины Qt: платформа (десктоп и offscreen для smoke-теста),
# картинки, иконки, звуковой бэкенд голосовых заметок
check_plugin() {
    [[ -n "${QT_PLUGINS}" ]] && compgen -G "${QT_PLUGINS}/$1" >/dev/null
}

check_plugin "platforms/libqxcb.so"           || NEED_PKGS+=(qtbase5-dev)
check_plugin "platforms/libqoffscreen.so"     || NEED_PKGS+=(qtbase5-dev)
check_plugin "imageformats/libqsvg.so"        || NEED_PKGS+=(qtbase5-dev)
check_plugin "imageformats/libqjpeg.so"       || NEED_PKGS+=(qtbase5-dev)
check_plugin "iconengines/*.so"               || NEED_PKGS+=(qtbase5-dev)
check_plugin "sqldrivers/libqsqlite.so"       || NEED_PKGS+=(libqt5sql5-sqlite)
check_plugin "mediaservice/*.so"              || NEED_PKGS+=(libqt5multimedia5-plugins)
check_plugin "audio/*.so"                     || NEED_PKGS+=(libqt5multimedia5-plugins)

# 3c. Переводы строк Qt (qttranslations5-l10n)
[[ -d /usr/share/qt5/translations ]] && compgen -G "/usr/share/qt5/translations/qt_*.qm" >/dev/null || NEED_PKGS+=(qttranslations5-l10n)

# 4. Недостающее НЕ ставится автоматически: печатаем готовую
# команду и падаем. Ставит пакеты сам пользователь руками
if [[ "${#NEED_PKGS[@]}" -gt 0 ]]; then
    # shellcheck disable=SC2207
    UNIQUES=($(printf "%s\n" "${NEED_PKGS[@]}" | sort -u))
    echo "ОШИБКА: для полноценной сборки не хватает пакетов: ${UNIQUES[*]}" >&2
    echo "" >&2
    echo "Доустановите их одной командой:" >&2
    echo "  sudo apt-get update && sudo apt-get install -y ${UNIQUES[*]}" >&2
    echo "" >&2
    echo "Затем запустите скрипт снова." >&2
    exit 2
fi

# 5. Финальная проверка ВСЕГО заново: что не встало - то явная ошибка.
# Здесь уже никаких попыток чинить, только констатация
ERRORS=()
command -v g++ >/dev/null 2>&1    || ERRORS+=("нет g++ (build-essential)")
command -v make >/dev/null 2>&1   || ERRORS+=("нет make (build-essential)")
command -v qmake >/dev/null 2>&1  || ERRORS+=("нет qmake (qt5-qmake)")

QT_HEADERS="$(command -v qmake >/dev/null 2>&1 && qmake -query QT_INSTALL_HEADERS 2>/dev/null || true)"
QT_PLUGINS="$(command -v qmake >/dev/null 2>&1 && qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
[[ -n "${QT_HEADERS}" && -d "${QT_HEADERS}/QtWidgets" ]] || ERRORS+=("нет заголовков QtWidgets (qtbase5-dev)")
[[ -f "${QT_HEADERS}/QtSvg/QtSvg" ]]                     2>/dev/null || ERRORS+=("нет заголовков QtSvg (libqt5svg5-dev)")
[[ -f "${QT_HEADERS}/QtMultimedia/QtMultimedia" ]]       2>/dev/null || ERRORS+=("нет заголовков QtMultimedia (qtmultimedia5-dev)")
command -v lrelease >/dev/null 2>&1                      || ERRORS+=("нет lrelease (qttools5-dev)")
[[ -f "${QT_PLUGINS}/platforms/libqxcb.so" ]]            2>/dev/null || ERRORS+=("нет platforms/libqxcb.so (qtbase5-dev)")
[[ -f "${QT_PLUGINS}/platforms/libqoffscreen.so" ]]      2>/dev/null || ERRORS+=("нет platforms/libqoffscreen.so (qtbase5-dev)")
[[ -f "${QT_PLUGINS}/imageformats/libqsvg.so" ]]         2>/dev/null || ERRORS+=("нет imageformats/libqsvg.so (qtbase5-dev)")
[[ -f "${QT_PLUGINS}/imageformats/libqjpeg.so" ]]        2>/dev/null || ERRORS+=("нет imageformats/libqjpeg.so (qtbase5-dev)")
compgen -G "${QT_PLUGINS}/iconengines/*.so" >/dev/null   2>&1 || ERRORS+=("нет iconengines/*.so (qtbase5-dev)")
[[ -f "${QT_PLUGINS}/sqldrivers/libqsqlite.so" ]]        2>/dev/null || ERRORS+=("нет sqldrivers/libqsqlite.so (libqt5sql5-sqlite)")
compgen -G "${QT_PLUGINS}/mediaservice/*.so" >/dev/null 2>&1 || ERRORS+=("нет mediaservice/*.so (libqt5multimedia5-plugins)")
compgen -G "${QT_PLUGINS}/audio/*.so" >/dev/null 2>&1         || ERRORS+=("нет audio/*.so (libqt5multimedia5-plugins)")
compgen -G "/usr/share/qt5/translations/qt_*.qm" >/dev/null 2>&1 || ERRORS+=("нет переводов Qt (qttranslations5-l10n)")

if [[ "${#ERRORS[@]}" -gt 0 ]]; then
    echo "ОШИБКА: не хватает компонентов для полноценной сборки:" >&2
    printf ' - %s\n' "${ERRORS[@]}" >&2
    exit 3
fi

echo "Все компоненты на месте, запускаю сборку"

# 6. Делегирование общей сборке со всеми аргументами пользователя
[[ -x "${BASE_SCRIPT}" ]] || fail "нет базового скрипта ${BASE_SCRIPT}"

exec "${BASE_SCRIPT}" "$@"

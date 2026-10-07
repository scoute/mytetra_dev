#!/bin/bash
# Быстрая проверка кода перед коммитом: высокосигнальные находки cppcheck
# только по измененным файлам (полный прогон долгий и шумный).
# Использование:
#   ./check_code.sh          - проверить staged-файлы (*.cpp, *.h)
#   ./check_code.sh --all    - проверить весь app/src
# Ноль означает чисто, ненулевой код - есть что чинить.
# Шумодав: QString по значению это Qt-идиома, init-lists и перебор
# по значению - стиль кодовой базы, переопределения методов моделей
# (data(), getIndexByItem) - осознанный дизайн. Всё пропущено сознательно,
# править под cppcheck это значит шуметь в историю без пользы.

cd "$(dirname "$0")" || exit 2

if [ "$1" = "--all" ]; then
  # shellcheck disable=SC2207
  FILES=( $(find app/src -name "*.cpp" -o -name "*.h") )
else
  # shellcheck disable=SC2207
  FILES=( $(git diff --cached --name-only --diff-filter=ACM -- '*.cpp' '*.h') )
fi

if [ ${#FILES[@]} -eq 0 ]; then
  echo "check_code: no files to check"
  exit 0
fi

# Дубли инклудов в файле: всегда ошибка внимания, чинится удалением повтора
dup_failed=0
for f in "${FILES[@]}"; do
  dups=$(grep -oE '#include [<"][^>"]+[>"]' "$f" | xargs -n1 basename 2>/dev/null | sort | uniq -d)
  if [ -n "$dups" ]; then
    echo "check_code: duplicate includes in $f:"
    echo "$dups" | sed 's/^/  /'
    dup_failed=1
  fi
done
if [ "$dup_failed" -ne 0 ]; then
  exit 1
fi

# Путь к заголовкам Qt для точности разбора, без него только макросы из --library=qt
QT_HEADERS=""
if command -v qmake >/dev/null 2>&1; then
  QT_HEADERS="-I$(qmake -query QT_INSTALL_HEADERS 2>/dev/null)"
fi

exec cppcheck \
  --library=qt \
  --enable=warning,performance \
  --std=c++14 \
  --inline-suppr \
  --quiet \
  --error-exitcode=1 \
  --suppress=passedByValue \
  --suppress=useInitializationList \
  --suppress=iterateByValue \
  --suppress=duplInheritedMember \
  --suppress=missingInclude \
  -I app/src \
  $QT_HEADERS \
  "${FILES[@]}"

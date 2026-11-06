#!/bin/bash
# Перечень объектных файлов приложения для линковки проб (без main.o).
# Источник: app.pro + qmake-приоритеты; main_renamed.o собирается отдельно.
set -e
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
APP=$(cd "$SCRIPT_DIR/.." && pwd)
R=$(dirname "$APP")
BLD=$R/build/Desktop_Qt_5_15_2_GCC_64bit/app/build
O=/tmp/opencode/objs_tests.txt
SRCS=$(for f in $APP/app.pro $APP/../thirdParty/qcodeeditor/qcodeeditor.pri $APP/src/libraries/qtSingleApplication/qtsingleapplication.h $APP/src/libraries/qtSingleApplication5/*.pri; do
  [ -f "$f" ] && grep -o '[A-Za-z0-9_./-]*\.\(cpp\|h\)' "$f"
done | sort -u)
> $O
for obj in $BLD/*.o; do
  base=$(basename $obj .o)
  case $base in
    main) continue;;
    qrc_*) printf '%s ' "$obj" >> $O; continue;;
    moc_*)
      cls=${base#moc_}
      if echo "$SRCS" | grep -q "/${cls}\.\(cpp\|h\)$"; then printf '%s ' "$obj" >> $O; fi
      continue;;
  esac
  if echo "$SRCS" | grep -q "/${base}\.cpp$"; then printf '%s ' "$obj" >> $O; fi
done
echo "" >> $O
wc -w < $O

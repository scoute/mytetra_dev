#!/bin/bash
# Сборка и прогон QTest-тестов MyTetra (см. 514).
# Тесты живут в репо (app/tests/test_*.cpp), фикстуры программные
# или в app/tests/data/. Волатильный /tmp/opencode — только для артефактов.
# Вердикт — по строке Totals в выводе QTest ("0 failed"), а не только
# по exit-коду: teardown под offscreen может падать штатно (см. AGENTS.md).
# Использование:
#   export PATH=/media/user/m2data/Qt_deb11/5.15.2/gcc_64/bin:$PATH
#   ./app/tests/run_qtests.sh [test_smoke|...]  # по умолчанию все test_*.cpp
set -u
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
APP=$(dirname "$SCRIPT_DIR")
R=$(dirname "$APP")
BLD=$R/build/Desktop_Qt_5_15_2_GCC_64bit/app/build
OUT=/tmp/opencode
QT_BIN=$(dirname "$(command -v qmake)")
QT=${QT_BIN%/bin}
mkdir -p "$OUT" "$OUT/edprobe"

if [ $# -eq 0 ]; then
  set -- $(cd "$APP/tests" && ls test_*.cpp 2>/dev/null | sed 's/\.cpp$//')
  if [ -z "$1" ]; then echo "NO-QTESTS"; exit 1; fi
fi

# Актуальный список объектов (без main.o): main_renamed.o собирается отдельно
"$R/app/tests/regen_objs.sh" >/dev/null || exit 1

compile_qtest() {
  local name=$1
  "$QT_BIN/moc" -o "$OUT/${name}.moc" "$APP/tests/$name.cpp" || return 1
  g++ -fPIC -std=gnu++1y -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
    -DQT_TEST_LIB -Dprivate=public -Dprotected=protected \
    -DTEST_DATA_DIR=\"$APP/tests/data\" \
    -DANY_OS=1 -DMEEGO_OS=2 -DANDROID_OS=3 -DTARGET_OS=ANY_OS \
    -I$APP -I$APP/src -I$OUT -I$QT/include \
    -I$QT/include/QtCore -I$QT/include/QtWidgets -I$QT/include/QtGui \
    -I$QT/include/QtXml -I$QT/include/QtNetwork -I$QT/include/QtMultimedia \
    -I$QT/include/QtSql -I$QT/include/QtConcurrent -I$QT/include/QtTest \
    -I$APP/../thirdParty/ogg/include -I$APP/../thirdParty/opus/include \
    -I$APP/../thirdParty/monocypher -I$APP/../thirdParty/qcodeeditor/include \
    -c "$APP/tests/$name.cpp" -o "$OUT/$name.o" || return 1
  g++ -o "$OUT/${name}bin" "$OUT/$name.o" "$OUT/edprobe/main_renamed.o" $(cat "$OUT/objs_tests.txt") \
    -L$APP/../thirdParty/opus/build/lib -lopus \
    -L$APP/../thirdParty/ogg/build/lib -logg \
    -L$APP/../thirdParty/monocypher/build/lib -lmonocypher \
    -lm -L$QT/lib \
    -lQt5Test -lQt5Svg -lQt5PrintSupport -lQt5Widgets -lQt5Multimedia \
    -lQt5Sql -lQt5Concurrent \
    -lQt5Gui -lQt5Xml -lQt5Network -lQt5Core -lGL -lpthread || return 1
  echo "LINK-DONE $name"
}

run_qtest() {
  local name=$1 out totals failed bindir=$OUT
  export LD_LIBRARY_PATH=$QT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
  export HOME=/tmp/opencode DISPLAY=:98 QT_QPA_PLATFORM=offscreen GIT_CONFIG_NOSYSTEM=1
  # Тесты с живым MetaEditor: им нужна рабочая директория
  # (conf.ini + editorconf.ini) и запуск бинаря из нее (workdir
  # резолвится из applicationFilePath). Фикстуры — из репо.
  case "$name" in
    test_split|test_attachcut)
      mkdir -p "$OUT/${name}db"
      cp "$APP/tests/data/conf.ini" \
         "$APP/tests/data/editorconf.ini" "$OUT/${name}db/"
      cp "$OUT/${name}bin" "$OUT/${name}db/"
      bindir="$OUT/${name}db"
      ;;
  esac
  out=$(cd "$bindir" && timeout 300 "./${name}bin" 2>&1)
  echo "$out" | grep -E "^(PASS|FAIL!|Totals|XFAIL)|passed|failed" | tail -8
  # Вердикт по тексту Totals: teardown-креш не должен маскировать результат
  if echo "$out" | grep -q "FAIL!"; then echo "FAILED $name"; return 1; fi
  totals=$(echo "$out" | grep -E "Totals:" | tail -1)
  if [ -z "$totals" ]; then echo "FAILED $name (no Totals)"; return 1; fi
  echo "$totals"
  if ! echo "$totals" | grep -q "0 failed"; then echo "FAILED $name"; return 1; fi
  return 0
}

mkdir -p $OUT/edprobe
# main_renamed.o: переглобальные объекты должны соответствовать текущим заголовкам
g++ -fPIC -std=gnu++1y -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
  -Dprivate=public -Dprotected=protected \
  -DANY_OS=1 -DMEEGO_OS=2 -DANDROID_OS=3 -DTARGET_OS=ANY_OS -Dmain=main_renamed \
  -I$APP -I$APP/src -I$QT/include \
  -I$QT/include/QtCore -I$QT/include/QtWidgets -I$QT/include/QtGui \
  -I$QT/include/QtXml -I$QT/include/QtNetwork -I$QT/include/QtMultimedia \
  -I$QT/include/QtSql -I$QT/include/QtConcurrent \
  -I$APP/../thirdParty/ogg/include -I$APP/../thirdParty/opus/include \
  -I$APP/../thirdParty/monocypher \
  -c $APP/src/main.cpp -o $OUT/edprobe/main_renamed.o || exit 1
for t in "$@"; do
  compile_qtest "$t" || exit 1
  run_qtest "$t" || exit 1
done
echo "ALL-DONE"

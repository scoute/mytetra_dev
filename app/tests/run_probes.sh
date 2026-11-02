#!/bin/bash
# Сборка и прогон headless-стендов (проб) MyTetra.
# Пробы - volatile: проверяются выводом в stdout, НЕ кодом возврата
# (teardown под offscreen падает штатно, см. AGENTS.md).
# Использование:
#   export PATH=/media/user/m2data/Qt_deb11/5.15.2/gcc_64/bin:$PATH
#   ./app/tests/run_probes.sh [confprobe|refprobe|...]  # по умолчанию все
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
  set -- confprobe refprobe trayprobe menuprobe mcpbridgeprobe typefaceprobe mcpcatprobe bridgewriteprobe voiceqprobe htmlprobe splitprobe attachcutprobe settingstransferprobe
fi

# Актуальный список объектов (без main.o): main_renamed.o собирается отдельно
"$R/app/tests/regen_objs.sh" >/dev/null || exit 1

compile_probe() {
  local name=$1
  g++ -fPIC -std=gnu++1y -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
    -Dprivate=public -Dprotected=protected \
    -DANY_OS=1 -DMEEGO_OS=2 -DANDROID_OS=3 -DTARGET_OS=ANY_OS \
    -I$APP -I$APP/src -I$QT/include \
    -I$QT/include/QtCore -I$QT/include/QtWidgets -I$QT/include/QtGui \
    -I$QT/include/QtXml -I$QT/include/QtNetwork -I$QT/include/QtMultimedia \
    -I$APP/../thirdParty/ogg/include -I$APP/../thirdParty/opus/include \
    -I$APP/../thirdParty/monocypher -I$APP/../thirdParty/qcodeeditor/include \
    -c "$APP/tests/$name.cpp" -o "$OUT/$name.o" || return 1
  g++ -o "$OUT/${name}bin" "$OUT/$name.o" "$OUT/edprobe/main_renamed.o" $(cat "$OUT/objs_tests.txt") \
    -L$APP/../thirdParty/opus/build/lib -lopus \
    -L$APP/../thirdParty/ogg/build/lib -logg \
    -L$APP/../thirdParty/monocypher/build/lib -lmonocypher \
    -lm -L$QT/lib \
    -lQt5Test -lQt5Svg -lQt5PrintSupport -lQt5Widgets -lQt5Multimedia \
    -lQt5Gui -lQt5Xml -lQt5Network -lQt5Core -lGL -lpthread || return 1
  echo "LINK-DONE $name"
}

run_probe() {
  local name=$1 out fails bindir=$OUT
  export LD_LIBRARY_PATH=$QT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
  export HOME=/tmp/opencode DISPLAY=:98 QT_QPA_PLATFORM=offscreen
  # splitprobe и attachcutprobe крутят живой MetaEditor: им нужна
  # рабочая директория (conf.ini + editorconf.ini) и запуск бинаря
  # из нее (workdir резолвится из argv[0]). Фикстура собирается
  # из штатных конфигов
  if [ "$name" = "splitprobe" ] || [ "$name" = "attachcutprobe" ]; then
    mkdir -p "$OUT/${name}db"
    cp "$APP/bin/resource/standartconfig/any/conf.ini" \
       "$APP/bin/resource/standartconfig/any/editorconf.ini" "$OUT/${name}db/"
    cp "$OUT/${name}bin" "$OUT/${name}db/"
    bindir="$OUT/${name}db"
  fi
  out=$(cd "$bindir" && timeout 120 "./${name}bin" 2>&1)
  echo "$out" | grep -E "^(PASS|FAIL)|PROBE-OK|PROBE-BAD|VOICEQ" | tail -4
  fails=$(echo "$out" | grep -c "^FAIL" || true)
  bad=$(echo "$out" | grep -c "PROBE-BAD" || true)
  if [ "$fails" != "0" ] || [ "$bad" != "0" ]; then echo "FAILED $name"; return 1; fi
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
  -I$APP/../thirdParty/ogg/include -I$APP/../thirdParty/opus/include \
  -I$APP/../thirdParty/monocypher \
  -c $APP/src/main.cpp -o $OUT/edprobe/main_renamed.o || exit 1
for t in "$@"; do
  compile_probe "$t" || exit 1
  run_probe "$t" || exit 1
done
echo "ALL-DONE"

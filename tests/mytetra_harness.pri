# Общий harness для тестов второй очереди (модули с зависимостью
# от глобалов приложения: globalParameters, mytetraConfig и т.д.).
#
# Как это работает:
# - тест линкуется со ВСЕМИ объектными файлами приложения, кроме main.o
#   (там лежат определения глобалов и функция main);
# - глобалы определяются в .cpp тестового бинаря (копия из app/src/main.cpp);
# - main() теста повторяет начало main() приложения: setMainProgramFile,
#   globalParameters.init(), mytetraConfig.init();
# - выход через _exit(): деструктор глобального AppConfig делает sync()
#   уже после смерти QApplication и падает в Qt-кодеках.
#
# Требования к окружению:
# - приложение должно быть собрано ДО qmake тестов (нужны готовые .o).
#   Путь к сборке задается переменной APP_BUILD_DIR, по умолчанию
#   /tmp/opencode/mt-app-build/app. Переопределение:
#   qmake APP_BUILD_DIR=/путь/к/сборке/app tests/tests.pro
# - рядом с тестовым бинарем должен лежать conf.ini (иначе init откроет
#   модальный InstallDialog и тест зависнет). Каждый harness-тест копирует
#   эталонный конфиг туда сам через QMAKE_POST_LINK, вручную ничего
#   копировать не нужно.
# - прогон строго в offscreen, псевдодисплей включается в main() теста.

isEmpty(APP_BUILD_DIR): APP_BUILD_DIR = /tmp/opencode/mt-app-build/app

!exists($$APP_BUILD_DIR/build/main.o) {
    error("Сначала соберите приложение: qmake mytetra.pro && make. Нет $$APP_BUILD_DIR/build/main.o. Путь меняется через qmake APP_BUILD_DIR=...")
}

APP_OBJECTS = $$files($$APP_BUILD_DIR/build/*.o)
APP_OBJECTS -= $$APP_BUILD_DIR/build/main.o

LIBS += $$APP_OBJECTS

# Зависимость бинаря от объектных файлов приложения: после пересборки
# приложения тесты автоматически перелинкуются. Без этого make считает
# бинарь актуальным (в LIBS make зависимости не видит) и гоняет старый код.
PRE_TARGETDEPS += $$APP_OBJECTS

# Явная линковка libgcov для сброса покрытия при _exit().
# Тесты завершаются через _exit() (иначе падает деструктор AppConfig),
# а _exit() не вызывает atexit-обработчики gcov. Поэтому harness вызывает
# __gcov_dump() вручную. Слабой ссылки недостаточно: линкер не вытягивает
# _gcov_dump.o из статического архива под слабую ссылку, нужна сильная
# (объявлена в main() тестов) плюс явный -lgcov здесь.
# Только для linux-g++: у MSVC/Clang libgcov нет.
linux-g++*: {
    LIBS += -lgcov
    DEFINES += MYTETRA_HAS_GCOV_DUMP
}

# Эталонный conf.ini рядом с тестовым бинарем. Без него
# globalParameters.init() откроет модальный InstallDialog и тест зависнет.
# Копия в каталоге сборки изолирует прогон: sync() правит только ее,
# данные пользователя и репозиторий не трогаются.
# $$PWD здесь - каталог самого .pri (tests/), поэтому путь на уровень вверх.
STANDART_CONF = $$PWD/../app/bin/resource/standartconfig/any/conf.ini
!exists($$STANDART_CONF) {
    error("Нет эталонного конфига $$STANDART_CONF")
}
# Каждая команда POST_LINK обязана заканчиваться разделителем
# $$escape_expand(\n\t): иначе qmake склеит несколько команд в одну
# shell-строку и копирование молча не сработает (как уже было с фикстурой).
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$STANDART_CONF) $$quote($$OUT_PWD) $$escape_expand(\n\t)

TEMPLATE = subdirs

# Юнит-тесты первой очереди: модули без зависимости
# от globalParameters/mytetraConfig и без виджетов.
# Каждый тест собирается как отдельный бинарь QtTest.
# Запуск: qmake && make && make check
# Headless не требуется: тесты используют QTEST_APPLESS_MAIN
# и не создают виджетов. Для будущих GUI-тестов нужен
# QT_QPA_PLATFORM=offscreen.

# Вторая очередь: модули с зависимостью от глобалов приложения
# (testWalkHistory, testAppConfig). Линкуются со всеми .o приложения,
# см. mytetra_harness.pri. Приложение собрать ДО qmake тестов.

# Третья очередь: дерево знаний и экран дерева на фикстуре
# (testKnowTree, testTreeScreen). Фикстура tests/fixtures/knowtree
# копируется в каталог сборки, эталон в репозитории не трогается.

# Сборка только вне исходников (как приложение): фикстура и conf.ini
# копируются в каталог сборки, при сборке внутри репозитория
# они засорят дерево исходников.

SUBDIRS += testOrderedMap \
    testSortHelper \
    testHtmlHelper \
    testUniqueIdHelper \
    testPbkdf2Qt \
    testRc5Simple \
    testCryptService \
    testFixedParameters \
    testWalkHistory \
    testAppConfig \
    testKnowTree \
    testTreeScreen

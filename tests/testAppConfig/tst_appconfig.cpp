#include <QtTest>

#include <unistd.h>

#include <QApplication>
#include <QFileInfo>

// Заголовки глобальных объектов приложения
#include "libraries/FixedParameters.h"
#include "libraries/GlobalParameters.h"
#include "libraries/WalkHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/TrashMonitoring.h"
#include "libraries/ShortcutManager.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/InternalClipboard.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"


// Глобальные объекты приложения. В приложении они определены
// в app/src/main.cpp, который в тесты не линкуется (там функция main),
// поэтому определяются здесь один в один как в main.cpp.
FixedParameters fixedParameters;
GlobalParameters globalParameters;
AppConfig mytetraConfig;
AppFiles mytetraFiles;
DataBaseConfig dataBaseConfig;
TrashMonitoring trashMonitoring;
WalkHistory walkHistory;
ActionLogger actionLogger;
ShortcutManager shortcutManager;
PeriodicCheckBase periodicCheckBase;
PeriodicSyncro periodicSyncro;
InternalClipboard *internalClipboard = nullptr;
QObject *pMainWindow = nullptr;


// Тестовый класс для проверки конфигурации приложения.
// Проверяется инициализация на изолированном тестовом conf.ini,
// значения по умолчанию из эталона и круговые set/get для
// безопасных флагов (правки уходят только в копию конфига
// в каталоге сборки теста, данные пользователя не трогаются).

class TestAppConfig : public QObject
{
    Q_OBJECT

private slots:

    // Проверка изоляции окружения и факта инициализации конфига
    void initTestCase()
    {
        QString binaryDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
        QCOMPARE(globalParameters.getWorkDirectory(), binaryDir);
        QVERIFY(mytetraConfig.is_init());
    }

    // Проверка версии формата конфига. Эталонный conf.ini имеет
    // version=35, при init() миграция update_version_process() доводит
    // его до актуальной версии (таблица get_parameter_table_43).
    // Тест фиксирует конечную версию: молчаливое изменение миграции
    // или текущей версии будет замечено.
    void configVersion()
    {
        QCOMPARE(mytetraConfig.get_config_version(), 43);
    }

    // Проверка языковых и интерфейсных значений по умолчанию
    void interfaceDefaults()
    {
        QCOMPARE(mytetraConfig.get_interfacelanguage(), QString("en"));
        QCOMPARE(mytetraConfig.getInterfaceMode(), QString("desktop"));
        QCOMPARE(mytetraConfig.get_runinminimizedwindow(), false);
        QCOMPARE(mytetraConfig.getShowSplashScreen(), false);
    }

    // Проверка значений истории и курсора по умолчанию
    void historyDefaults()
    {
        QCOMPARE(mytetraConfig.getRememberCursorAtHistoryNavigation(), true);
    }

    // Проверка круговой записи флага сворачивания при старте
    void runMinimizedRoundTrip()
    {
        QCOMPARE(mytetraConfig.get_runinminimizedwindow(), false);

        mytetraConfig.set_runinminimizedwindow(true);
        QCOMPARE(mytetraConfig.get_runinminimizedwindow(), true);

        mytetraConfig.set_runinminimizedwindow(false);
        QCOMPARE(mytetraConfig.get_runinminimizedwindow(), false);
    }

    // Проверка круговой записи флага запоминания курсора
    void rememberCursorRoundTrip()
    {
        QCOMPARE(mytetraConfig.getRememberCursorAtHistoryNavigation(), true);

        mytetraConfig.setRememberCursorAtHistoryNavigation(false);
        QCOMPARE(mytetraConfig.getRememberCursorAtHistoryNavigation(), false);

        mytetraConfig.setRememberCursorAtHistoryNavigation(true);
        QCOMPARE(mytetraConfig.getRememberCursorAtHistoryNavigation(), true);
    }
};


// Сброс данных покрытия перед _exit(). _exit() не вызывает atexit-обработчики,
// поэтому без явного сброса gcov теряет данные прогона. libgcov линкуется явно
// через mytetra_harness.pri. Без флагов покрытия в объектах приложения вызов
// безопасен: сбрасывать нечего, функция просто ничего не пишет.
#ifdef MYTETRA_HAS_GCOV_DUMP
extern "C" void __gcov_dump(void);
#endif


// Собственная функция main вместо QTEST_MAIN.
// Повторяет начало main() приложения: запоминается имя бинаря
// (по нему ищется conf.ini), инициализируются глобальные параметры
// и конфиг. Платформа offscreen включается принудительно.
// Выход через _exit(): деструктор глобального AppConfig делает sync()
// уже после смерти QApplication и падает в Qt-кодеках.
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QApplication app(argc, argv);

    QString mainProgramFile = QString::fromLocal8Bit(argv[0]);
    globalParameters.setMainProgramFile(mainProgramFile);

    globalParameters.init();
    mytetraConfig.init();

    TestAppConfig test;
    int code = QTest::qExec(&test, argc, argv);

#ifdef MYTETRA_HAS_GCOV_DUMP
    __gcov_dump();
#endif

    _exit(code);
    return code;
}

#include "tst_appconfig.moc"

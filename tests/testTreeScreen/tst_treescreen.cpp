#include <QtTest>

#include <unistd.h>

#include <QApplication>
#include <QFile>
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
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "views/tree/TreeScreen.h"


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


// Сброс данных покрытия перед _exit(). _exit() не вызывает atexit-обработчики,
// поэтому без явного сброса gcov теряет данные прогона. libgcov линкуется явно
// через mytetra_harness.pri. Без флагов покрытия в объектах приложения вызов
// безопасен: сбрасывать нечего, функция просто ничего не пишет.
#ifdef MYTETRA_HAS_GCOV_DUMP
extern "C" void __gcov_dump(void);
#endif


// Тестовый класс для проверки экрана дерева в offscreen-режиме.
// TreeScreen сам загружает фикстуру через tetradir, модель подключается
// к виду в конструкторе. Проверяются загрузка, установка курсора
// по идентификатору и перечитывание с диска.

class TestTreeScreen : public QObject
{
    Q_OBJECT

private slots:

    // Проверка изоляции окружения и готовности базы
    void initTestCase()
    {
        QString binaryDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
        QCOMPARE(globalParameters.getWorkDirectory(), binaryDir);
        QVERIFY(mytetraConfig.is_init());
        QVERIFY(dataBaseConfig.is_init());
    }

    // Проверка загрузки фикстуры экраном дерева
    void screenLoadsFixture()
    {
        TreeScreen screen;

        QVERIFY(screen.knowTreeModel != nullptr);
        QCOMPARE(screen.knowTreeModel->rowCount(QModelIndex()), 2);
        QCOMPARE(screen.knowTreeModel->getAllRecordCount(), 3);
    }

    // Проверка установки курсора на ветку по идентификатору
    void cursorToId()
    {
        TreeScreen screen;

        screen.setCursorToId("branch-child-1");

        QModelIndex current = screen.getCurrentItemIndex();
        QVERIFY(current.isValid());

        TreeItem *item = screen.knowTreeModel->getItem(current);
        QVERIFY(item != nullptr);
        QCOMPARE(item->getField("id"), QString("branch-child-1"));
        QCOMPARE(item->getField("name"), QString("Child branch"));

        // Выделена ровно текущая ветка
        QVERIFY(screen.getFirstSelectedItemIndex() >= 0);
    }

    // Проверка перечитывания дерева с диска: без внешних изменений
    // перечитывание не требуется, внешнее изменение обнаруживается
    void reloadTree()
    {
        TreeScreen screen;

        // Запоминается исходное содержимое файла базы
        QString xmlName = globalParameters.getWorkDirectory() + "/data/mytetra.xml";
        QFile baseFile(xmlName);
        QVERIFY(baseFile.open(QIODevice::ReadOnly));
        QByteArray original = baseFile.readAll();
        baseFile.close();

        screen.saveKnowTree();

        // Без изменений на диске перечитывание не требуется
        QVERIFY(!screen.reloadKnowTree());

        // Внешнее изменение файла обнаруживается и перечитывается
        QVERIFY(baseFile.open(QIODevice::WriteOnly));
        QCOMPARE(baseFile.write(original), static_cast<qint64>(original.size()));
        baseFile.close();

        QVERIFY(screen.reloadKnowTree());
        QCOMPARE(screen.knowTreeModel->getAllRecordCount(), 3);

        // После перечитывания изменений снова нет
        QVERIFY(!screen.reloadKnowTree());
    }
};


// Собственная функция main вместо QTEST_MAIN.
// Повторяет начало main() приложения с добавлением инициализации
// конфига базы (автосоздает database.ini в фикстуре).
// Платформа offscreen включается принудительно.
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
    dataBaseConfig.init();

    // Экраны в конструкторе регистрируют действия в менеджере горячих
    // клавиш (как MainWindow в приложении), без init() там criticalError
    // с модальным окном. Порядок как в main(): после конфига.
    shortcutManager.init();

    TestTreeScreen test;
    int code = QTest::qExec(&test, argc, argv);

#ifdef MYTETRA_HAS_GCOV_DUMP
    __gcov_dump();
#endif

    _exit(code);
    return code;
}

#include "tst_treescreen.moc"

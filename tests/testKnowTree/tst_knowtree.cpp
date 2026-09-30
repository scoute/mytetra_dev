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
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/recordTable/Record.h"


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


// Тестовый класс для проверки дерева знаний на фикстуре.
// Фикстура tests/fixtures/knowtree: две корневые ветки (одна с подветкой),
// три незашифрованные записи с текстами, один аттач в первой записи.
// Проверяются загрузка, структура, запросы, тексты, аттачи и
// круговое сохранение-перечитывание.

class TestKnowTree : public QObject
{
    Q_OBJECT

    // Построение модели, загруженной из фикстуры
    void loadFixture(KnowTreeModel &model)
    {
        model.initFromXML("./data/mytetra.xml");
    }

private slots:

    // Проверка изоляции окружения и готовности базы:
    // рабочая директория совпадает с директорией бинаря,
    // конфиги инициализированы, база без шифрования
    void initTestCase()
    {
        QString binaryDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
        QCOMPARE(globalParameters.getWorkDirectory(), binaryDir);
        QVERIFY(mytetraConfig.is_init());
        QVERIFY(dataBaseConfig.is_init());
        QCOMPARE(dataBaseConfig.get_crypt_mode(), 0);
    }

    // Проверка структуры дерева из фикстуры
    void loadStructure()
    {
        KnowTreeModel model;
        loadFixture(model);

        // Две корневые ветки
        QCOMPARE(model.rowCount(QModelIndex()), 2);

        TreeItem *branch1 = model.getItemById("branch-root-1");
        QVERIFY(branch1 != nullptr);
        QCOMPARE(branch1->getField("name"), QString("Test branch one"));
        QCOMPARE(branch1->childCount(), 1);

        TreeItem *child = model.getItemById("branch-child-1");
        QVERIFY(child != nullptr);
        QCOMPARE(child->getField("name"), QString("Child branch"));
        QCOMPARE(child->childCount(), 0);

        // Пустая ветка без записей и подветок
        TreeItem *emptyBranch = model.getItemById("branch-root-2");
        QVERIFY(emptyBranch != nullptr);
        QCOMPARE(emptyBranch->childCount(), 0);
        QCOMPARE(emptyBranch->recordtableGetRowCount(), 0);

        // Неизвестный идентификатор не находится
        QVERIFY(model.getItemById("no-such-branch") == nullptr);
    }

    // Проверка подсчета записей
    void recordCounts()
    {
        KnowTreeModel model;
        loadFixture(model);

        QCOMPARE(model.getAllRecordCount(), 3);

        TreeItem *branch1 = model.getItemById("branch-root-1");
        // Две свои записи плюс одна в подветке
        QCOMPARE(model.getRecordCountForItem(branch1), 3);

        TreeItem *emptyBranch = model.getItemById("branch-root-2");
        QCOMPARE(model.getRecordCountForItem(emptyBranch), 0);
    }

    // Проверка пути к записи (список id от корня "0" до ветки,
    // тот же формат потребляют isItemValid() и getItem(), как в MainWindow)
    void recordPath()
    {
        KnowTreeModel model;
        loadFixture(model);

        QStringList path = model.getRecordPath("note-3");
        QCOMPARE(path, QStringList({"0", "branch-root-1", "branch-child-1"}));

        // Путь пригоден для получения ветки тем же способом, что в MainWindow
        QVERIFY(model.isItemValid(path));
        QCOMPARE(model.getItem(path)->getField("id"), QString("branch-child-1"));

        QCOMPARE(model.getRecordPath("note-1"),
                 QStringList({"0", "branch-root-1"}));

        // Неизвестная запись: пустой путь
        QVERIFY(model.getRecordPath("no-such-note").isEmpty());
    }

    // Проверка существования идентификаторов
    void idExists()
    {
        KnowTreeModel model;
        loadFixture(model);

        QVERIFY(model.isItemIdExists("branch-child-1"));
        QVERIFY(!model.isItemIdExists("no-such-branch"));

        QVERIFY(model.isRecordIdExists("note-2"));
        QVERIFY(!model.isRecordIdExists("no-such-note"));
    }

    // Проверка списка всех идентификаторов записей
    void allRecordsIdList()
    {
        KnowTreeModel model;
        loadFixture(model);

        QSharedPointer< QSet<QString> > ids = model.getAllRecordsIdList();
        QCOMPARE(ids->size(), 3);
        QVERIFY(ids->contains("note-1"));
        QVERIFY(ids->contains("note-2"));
        QVERIFY(ids->contains("note-3"));
    }

    // Проверка полей записей
    void recordFields()
    {
        KnowTreeModel model;
        loadFixture(model);

        TreeItem *branch1 = model.getItemById("branch-root-1");
        const RecordTableData *table = branch1->recordtableGetTableData();
        QCOMPARE(static_cast<int>(table->size()), 2);

        QCOMPARE(table->getField("name", 0), QString("First note"));
        QCOMPARE(table->getField("author", 0), QString("Tester"));
        QCOMPARE(table->getField("url", 0), QString("https://example.com"));
        QCOMPARE(table->getField("tags", 0), QString("alpha,beta"));
        QCOMPARE(table->getField("name", 1), QString("Second note"));
    }

    // Проверка текстов записей, читаемых из файлов фикстуры
    void recordTexts()
    {
        KnowTreeModel model;
        loadFixture(model);

        TreeItem *branch1 = model.getItemById("branch-root-1");
        const RecordTableData *table = branch1->recordtableGetTableData();
        QVERIFY(table->getText(0).contains("Hello from first note"));
        QVERIFY(table->getText(1).contains("Second note body"));

        TreeItem *child = model.getItemById("branch-child-1");
        QVERIFY(child->recordtableGetTableData()->getText(0).contains("Child note body"));

        // Недопустимый индекс возвращает пустую строку, а не падает
        QVERIFY(table->getText(-1).isEmpty());
        QVERIFY(table->getText(100).isEmpty());
    }

    // Проверка получения записи по идентификатору
    void getRecord()
    {
        KnowTreeModel model;
        loadFixture(model);

        Record *record = model.getRecord("note-1");
        QVERIFY(record != nullptr);
        QCOMPARE(record->getField("name"), QString("First note"));
        QCOMPARE(record->getField("author"), QString("Tester"));
    }

    // Проверка таблицы аттачей записи
    void attachFields()
    {
        KnowTreeModel model;
        loadFixture(model);

        Record *record = model.getRecord("note-1");
        QVERIFY(record != nullptr);

        AttachTableData attaches = record->getAttachTable();
        QCOMPARE(attaches.size(), 1);

        Attach attach = attaches.getAttach("attach-1");
        QVERIFY(!attach.isEmpty());
        QCOMPARE(attach.getField("fileName"), QString("doc.txt"));
        QCOMPARE(attach.getField("type"), QString("file"));

        // Запись без аттачей имеет пустую таблицу
        Record *plainRecord = model.getRecord("note-2");
        QVERIFY(plainRecord != nullptr);
        QCOMPARE(plainRecord->getAttachTable().size(), 0);
    }

    // Проверка кругового сохранения: save, clear, reload
    // возвращает ту же структуру. Пишет только в копию фикстуры
    // в каталоге сборки, эталон в репозитории не трогается.
    void saveReload()
    {
        KnowTreeModel model;
        loadFixture(model);
        QCOMPARE(model.getAllRecordCount(), 3);

        model.save();

        model.clear();
        QCOMPARE(model.rowCount(QModelIndex()), 0);

        model.reload();
        QCOMPARE(model.rowCount(QModelIndex()), 2);
        QCOMPARE(model.getAllRecordCount(), 3);
        QCOMPARE(model.getItemById("branch-root-1")->getField("name"),
                 QString("Test branch one"));
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

    TestKnowTree test;
    int code = QTest::qExec(&test, argc, argv);

#ifdef MYTETRA_HAS_GCOV_DUMP
    __gcov_dump();
#endif

    _exit(code);
    return code;
}

#include "tst_knowtree.moc"

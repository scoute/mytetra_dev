// Избранное: порядок сайдкара на QTest (см. 551).
// Посев порядка из gee12-базы (формат 1.3, числовой favor),
// переупорядочивание через setOrder и переживание перезагрузки.
// Без GUI: хранилище и модель дерева программные, tetradir во временном
// каталоге.
#include <QtTest>
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "libraries/AppContext.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/TrashMonitoring.h"
#include "libraries/WalkHistory.h"
#include "libraries/VisitHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/ShortcutManager.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/InternalClipboard.h"
#include "libraries/Favorites.h"
#include "libraries/BacklinkIndex.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/tree/KnowTreeModel.h"

extern AppConfig mytetraConfig;
extern ShortcutManager shortcutManager;

class SeedModel : public KnowTreeModel
{
public:

    explicit SeedModel(BacklinkIndex *backlinks, EditorShowTextDispatcher *dispatcher)
        : KnowTreeModel(backlinks, dispatcher) {}
};

class TestFavorites : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void orderRoundtrip(void);
    void favorSeeding(void);

private:

    QTemporaryDir m_tmp;
    QString m_baseDir;
    QString m_xmlPath;
    QSettings *m_settings=nullptr;
};

static const char *SEED_XML =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<root><format version=\"1\" subversion=\"3\" />"
    "<content>"
    "<node id=\"b1\" name=\"Branch\" ctime=\"20240101000000\" crypt=\"0\">"
    "<recordtable>"
    "<record id=\"rSecond\" name=\"Second\" author=\"\" url=\"\" tags=\"\" ctime=\"20240101000000\" dir=\"d2\" file=\"text.html\" crypt=\"0\" block=\"0\" favor=\"2\"/>"
    "<record id=\"rFirst\" name=\"First\" author=\"\" url=\"\" tags=\"\" ctime=\"20240101000000\" dir=\"d1\" file=\"text.html\" crypt=\"0\" block=\"0\" favor=\"1\"/>"
    "<record id=\"rFlag\" name=\"Flagged\" author=\"\" url=\"\" tags=\"\" ctime=\"20240101000000\" dir=\"d3\" file=\"text.html\" crypt=\"0\" block=\"0\" favorite=\"1\"/>"
    "<record id=\"rPlain\" name=\"Plain\" author=\"\" url=\"\" tags=\"\" ctime=\"20240101000000\" dir=\"d4\" file=\"text.html\" crypt=\"0\" block=\"0\"/>"
    "</recordtable>"
    "</node>"
    "</content></root>\n";

void TestFavorites::initTestCase(void)
{
    static FixedParameters *fp=new FixedParameters;
    static GlobalParameters *gp=new GlobalParameters;
    static AppFiles *af=new AppFiles;
    static DataBaseConfig *dbc=new DataBaseConfig;
    static TrashMonitoring *tm=new TrashMonitoring;
    static WalkHistory *wh=new WalkHistory;
    static VisitHistory *vh=new VisitHistory;
    static ActionLogger *al=new ActionLogger;
    static PeriodicCheckBase *pcb=new PeriodicCheckBase;
    static PeriodicSyncro *ps=new PeriodicSyncro;
    static InternalClipboard *ic=new InternalClipboard;
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                ic, nullptr, &gp->cryptHolder());

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_favorites.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;

    m_baseDir=m_tmp.filePath(QStringLiteral("base"));
    QVERIFY2(QDir().mkpath(m_baseDir), "tetradir must be created");
    m_settings->setValue(QStringLiteral("tetradir"), m_baseDir);

    m_xmlPath=m_baseDir+QStringLiteral("/mytetra.xml");
}

void TestFavorites::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestFavorites::orderRoundtrip(void)
{
    Favorites *favorites=AppContext::instance().favorites();
    QVERIFY2(favorites!=nullptr, "favorites must exist");

    // Добавление - в хвост
    favorites->setFavorite(QStringLiteral("a"), true);
    favorites->setFavorite(QStringLiteral("b"), true);
    favorites->setFavorite(QStringLiteral("c"), true);
    QCOMPARE(favorites->ids(),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("b"),
                          QStringLiteral("c")}));

    // Перестановка целиком
    favorites->setOrder(QStringList({QStringLiteral("c"),
                                     QStringLiteral("a"),
                                     QStringLiteral("b")}));
    QCOMPARE(favorites->ids(),
             QStringList({QStringLiteral("c"),
                          QStringLiteral("a"),
                          QStringLiteral("b")}));

    // Неупомянутые не теряются: дубли режутся, лишнее в хвост
    favorites->setOrder(QStringList({QStringLiteral("a"),
                                     QStringLiteral("a"),
                                     QStringLiteral("x")}));
    QCOMPARE(favorites->ids(),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("x"),
                          QStringLiteral("c"),
                          QStringLiteral("b")}));

    // Перезагрузка из файла держит порядок
    Favorites reloaded;
    QCOMPARE(reloaded.ids(), favorites->ids());

    // Снятие среднего
    favorites->setFavorite(QStringLiteral("x"), false);
    QCOMPARE(favorites->ids(),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("c"),
                          QStringLiteral("b")}));

    // Чистка
    favorites->setFavorite(QStringLiteral("a"), false);
    favorites->setFavorite(QStringLiteral("c"), false);
    favorites->setFavorite(QStringLiteral("b"), false);
    QVERIFY2(favorites->ids().isEmpty(), "favorites must be empty after cleanup");
}

void TestFavorites::favorSeeding(void)
{
    // Чистый старт как при первом открытии чужой базы: сайдкара нет.
    // Предыдущий кейс оставил пустой файл - посев его бы не тронул
    QFile::remove(m_baseDir+QStringLiteral("/favorites.xml"));

    QFile xml(m_xmlPath);
    QVERIFY2(xml.open(QIODevice::WriteOnly | QIODevice::Truncate),
             "seed mytetra.xml must be writable");
    xml.write(SEED_XML);
    xml.close();

    QFile before(m_xmlPath);
    QVERIFY2(before.open(QIODevice::ReadOnly), "seed xml must be readable");
    const QByteArray beforeBytes=before.readAll();
    before.close();

    SeedModel model(AppContext::instance().backlinkIndex(),
                    AppContext::instance().dispatcher());
    model.initFromXML(m_xmlPath);
    QCOMPARE(model.baseSubVersion(), 3);

    Favorites *favorites=AppContext::instance().favorites();
    QVERIFY2(favorites!=nullptr, "favorites must exist");

    // Сайдкара еще нет: порядок забирается из favor (1,2),
    // затем наш favorite="1", plain-запись не избранная
    favorites->seedFromFavorOrder(&model);
    QCOMPARE(favorites->ids(),
             QStringList({QStringLiteral("rFirst"),
                          QStringLiteral("rSecond"),
                          QStringLiteral("rFlag")}));

    // mytetra.xml не правится посевом
    QFile after(m_xmlPath);
    QVERIFY2(after.open(QIODevice::ReadOnly), "seed xml must be readable after seed");
    QCOMPARE(after.readAll(), beforeBytes);
    after.close();

    // Повторный посев при существующем сайдкаре - пустышка
    favorites->seedFromFavorOrder(&model);
    QCOMPARE(favorites->ids(),
             QStringList({QStringLiteral("rFirst"),
                          QStringLiteral("rSecond"),
                          QStringLiteral("rFlag")}));

    // Чистка
    favorites->setFavorite(QStringLiteral("rFirst"), false);
    favorites->setFavorite(QStringLiteral("rSecond"), false);
    favorites->setFavorite(QStringLiteral("rFlag"), false);
}

QTEST_MAIN(TestFavorites)

#include "test_favorites.moc"

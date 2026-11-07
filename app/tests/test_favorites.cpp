// Избранное: порядок сайдкара на QTest, линия v2 (см. 551).
// Посев порядка из gee12-базы (формат 1.3, числовой favor),
// переупорядочивание через setOrder и переживание перезагрузки.
// Без GUI: синглтон Favorites на временных ini, модель дерева программная.
#include <QtTest>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "libraries/Favorites.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"

extern AppConfig mytetraConfig;
extern DataBaseConfig dataBaseConfig;

class SeedModel : public KnowTreeModel
{
public:

    explicit SeedModel(void) : KnowTreeModel(nullptr) {}
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
    // Добавление - в хвост
    Favorites::instance().setFavorite(QStringLiteral("a"), true);
    Favorites::instance().setFavorite(QStringLiteral("b"), true);
    Favorites::instance().setFavorite(QStringLiteral("c"), true);
    QCOMPARE(Favorites::instance().ids(),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("b"),
                          QStringLiteral("c")}));

    // Перестановка целиком
    Favorites::instance().setOrder(QStringList({QStringLiteral("c"),
                                                QStringLiteral("a"),
                                                QStringLiteral("b")}));
    QCOMPARE(Favorites::instance().ids(),
             QStringList({QStringLiteral("c"),
                          QStringLiteral("a"),
                          QStringLiteral("b")}));

    // Неупомянутые не теряются: дубли режутся, лишнее в хвост
    Favorites::instance().setOrder(QStringList({QStringLiteral("a"),
                                                QStringLiteral("a"),
                                                QStringLiteral("x")}));
    QCOMPARE(Favorites::instance().ids(),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("x"),
                          QStringLiteral("c"),
                          QStringLiteral("b")}));

    // Снятие среднего
    Favorites::instance().setFavorite(QStringLiteral("x"), false);
    QCOMPARE(Favorites::instance().ids(),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("c"),
                          QStringLiteral("b")}));

    // Чистка
    Favorites::instance().setFavorite(QStringLiteral("a"), false);
    Favorites::instance().setFavorite(QStringLiteral("c"), false);
    Favorites::instance().setFavorite(QStringLiteral("b"), false);
    QVERIFY2(Favorites::instance().ids().isEmpty(), "favorites must be empty after cleanup");
}

void TestFavorites::favorSeeding(void)
{
    // Чистый старт как при первом открытии чужой базы: сайдкара нет.
    // Предыдущий кейс оставил файл - посев его бы не тронул
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

    SeedModel model;
    model.initFromXML(m_xmlPath);
    QCOMPARE(model.baseSubVersion(), 3);

    // Сайдкара еще нет: порядок забирается из favor (1,2),
    // затем наш favorite="1", plain-запись не избранная
    Favorites::instance().seedFromFavorOrder(&model);
    QCOMPARE(Favorites::instance().ids(),
             QStringList({QStringLiteral("rFirst"),
                          QStringLiteral("rSecond"),
                          QStringLiteral("rFlag")}));

    // mytetra.xml не правится посевом
    QFile after(m_xmlPath);
    QVERIFY2(after.open(QIODevice::ReadOnly), "seed xml must be readable after seed");
    QCOMPARE(after.readAll(), beforeBytes);
    after.close();

    // Повторный посев при существующем сайдкаре - пустышка
    Favorites::instance().seedFromFavorOrder(&model);
    QCOMPARE(Favorites::instance().ids(),
             QStringList({QStringLiteral("rFirst"),
                          QStringLiteral("rSecond"),
                          QStringLiteral("rFlag")}));

    // Чистка
    Favorites::instance().setFavorite(QStringLiteral("rFirst"), false);
    Favorites::instance().setFavorite(QStringLiteral("rSecond"), false);
    Favorites::instance().setFavorite(QStringLiteral("rFlag"), false);
}

QTEST_APPLESS_MAIN(TestFavorites)

#include "test_favorites.moc"

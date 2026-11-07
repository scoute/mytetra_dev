// Имена каталогов записей на QTest, линия v2 (см. 555).
// Чистые функции имен (system/hybrid) плюс интеграция: создание записи
// в гибридном режиме и конвертер туда-обратно на временной базе.
// Глобалы — extern из main.cpp (в v2 нет AppContext).
#include <QtTest>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "libraries/GlobalParameters.h"

extern AppConfig mytetraConfig;
extern DataBaseConfig dataBaseConfig;
extern GlobalParameters globalParameters;

class RecdirModel : public KnowTreeModel
{
public:

    explicit RecdirModel(void) : KnowTreeModel(nullptr) {}

    TreeItem *root(void) { return rootItem; }
};

class TestRecdir : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void dirNameFunctions(void);
    void createAndConvert(void);

private:

    QTemporaryDir m_tmp;
    QString m_baseDir;
    QString m_xmlPath;
    QSettings *m_settings=nullptr;
};

static const char *SKELETON =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<root><format version=\"1\" subversion=\"3\" />"
    "<content></content></root>\n";

void TestRecdir::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_recdir.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;

    m_baseDir=m_tmp.filePath(QStringLiteral("base"));
    QVERIFY2(QDir().mkpath(m_baseDir+QStringLiteral("/base")),
             "tetradir/base must be created");
    m_settings->setValue(QStringLiteral("tetradir"), m_baseDir);

    m_xmlPath=m_baseDir+QStringLiteral("/mytetra.xml");

    QFile skeleton(m_xmlPath);
    QVERIFY2(skeleton.open(QIODevice::WriteOnly | QIODevice::Truncate),
             "skeleton mytetra.xml must be writable");
    skeleton.write(SKELETON);
    skeleton.close();
}

void TestRecdir::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestRecdir::dirNameFunctions(void)
{
    // Системная схема: голый id, как в обычной MyTetra
    QCOMPARE(Record::constructLegacyDirName(QStringLiteral("abc123")),
             QStringLiteral("abc123"));
    QCOMPARE(Record::dirNameForMode(AppConfig::RecordDirSystem,
                                    QStringLiteral("abc123"),
                                    QStringLiteral("My Note")),
             QStringLiteral("abc123"));

    // Гибрид: id + санитизированное имя. Пробелы - подчеркивания
    // (консоль Linux без квотирования), регистр и кириллица целы
    QCOMPARE(Record::constructHumanDirName(QStringLiteral("abc123"),
                                           QStringLiteral("My Note")),
             QStringLiteral("abc123_My_Note"));
    QCOMPARE(Record::constructHumanDirName(QStringLiteral("abc123"),
                                           QStringLiteral("Договор 12")),
             QStringLiteral("abc123_Договор_12"));

    // Мусор в имени: слеши, NUL и Windows-запрещенные символы чистятся
    QCOMPARE(Record::constructHumanDirName(QStringLiteral("abc123"),
                                           QStringLiteral("a/b\\c:d")),
             QStringLiteral("abc123_a_b_c_d"));

    // Пустое и нечистое имя дает системный вид
    QCOMPARE(Record::constructHumanDirName(QStringLiteral("abc123"), QString()),
             QStringLiteral("abc123"));
    QCOMPARE(Record::constructHumanDirName(QStringLiteral("abc123"),
                                           QStringLiteral("...")),
             QStringLiteral("abc123"));

    // Дефолт режима - системный (как в обычной MyTetra)
    QCOMPARE(mytetraConfig.get_recordDirNameMode(),
             AppConfig::RecordDirSystem);
}

void TestRecdir::createAndConvert(void)
{
    // Гибридный режим: новые записи сразу именуются id_имя
    mytetraConfig.set_recordDirNameMode(AppConfig::RecordDirHybrid);

    RecdirModel model;
    model.initFromXML(m_xmlPath);

    QMap<QString, QString> branchFields;
    branchFields[QStringLiteral("id")]=QStringLiteral("recdirB1");
    branchFields[QStringLiteral("name")]=QStringLiteral("RecdirBranch");
    model.addNewBranch(model.root(), branchFields);

    TreeItem *branch=model.root()->child(0);
    QVERIFY2(branch!=nullptr, "branch must exist");

    Record record;
    record.switchToFat();
    record.setField(QStringLiteral("name"), QStringLiteral("My Note"));
    record.setField(QStringLiteral("file"), QStringLiteral("text.html"));
    record.setText(QStringLiteral("<p>Hello recdir</p>"));

    const int pos=branch->recordtableGetTableData()->insertNewRecord(
        GlobalParameters::AddNewRecordBehavior::ADD_TO_END, 0, record);
    QVERIFY2(pos>=0, "insertNewRecord must return valid position");

    RecordTableData *table=branch->recordtableGetTableData();
    const QString recordId=table->getField(QStringLiteral("id"), pos);
    const QString recordDir=table->getField(QStringLiteral("dir"), pos);
    QCOMPARE(recordDir, recordId+QStringLiteral("_My_Note"));

    model.save();
    QVERIFY2(QDir(m_baseDir+QStringLiteral("/base/")+recordDir).exists(),
             "hybrid record dir must exist on disk");

    // Конвертер в системный вид
    int notFound=0;
    int locked=0;
    QStringList errors;
    const int renamed=model.convertRecordDirsToMode(AppConfig::RecordDirSystem,
                                                    notFound, locked, errors);
    QCOMPARE(renamed, 1);
    QVERIFY2(errors.isEmpty(), "converter must have no errors");
    QCOMPARE(table->getField(QStringLiteral("dir"), pos), recordId);
    QVERIFY2(QDir(m_baseDir+QStringLiteral("/base/")+recordId).exists(),
             "system record dir must exist on disk");
    QVERIFY2(!QDir(m_baseDir+QStringLiteral("/base/")+recordDir).exists(),
             "old hybrid dir must be gone");

    // Конвертер обратно в гибридный вид
    const int renamedBack=model.convertRecordDirsToMode(AppConfig::RecordDirHybrid,
                                                        notFound, locked, errors);
    QCOMPARE(renamedBack, 1);
    QVERIFY2(errors.isEmpty(), "back converter must have no errors");
    QCOMPARE(table->getField(QStringLiteral("dir"), pos),
             recordId+QStringLiteral("_My_Note"));
}

QTEST_APPLESS_MAIN(TestRecdir)

#include "test_recdir.moc"

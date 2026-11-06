// Сквозной смоук на QTest, линия v2 (см. 497).
// Ветка -> запись -> save -> перечитать -> сверить. Без GUI: только
// модель, файлы и QSettings-фикстура. Глобалы — extern из main.cpp
// (в v2 нет AppContext).
#include <QtTest>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/recordTable/Record.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"

extern AppConfig mytetraConfig;
extern DataBaseConfig dataBaseConfig;

class SmokeModel : public KnowTreeModel
{
public:

    explicit SmokeModel(void) : KnowTreeModel(nullptr) {}

    TreeItem *root(void) { return rootItem; }
};

class TestSmoke : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void emptyStart(void);
    void branchRecordRoundtrip(void);
    void crossCryptMoveRefused(void);

private:

    QTemporaryDir m_tmp;
    QString m_baseDir;
    QString m_xmlPath;
    QSettings *m_settings=nullptr;

    QString m_savedId;
    QString m_savedDir;
};

static const char *SKELETON =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<root><format version=\"1\" subversion=\"1\" />"
    "<content></content></root>\n";

void TestSmoke::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    // Программная фикстура: ini внутри временного каталога теста
    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_smoke.ini")),
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

void TestSmoke::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestSmoke::emptyStart(void)
{
    SmokeModel model;
    model.initFromXML(m_xmlPath);
    QCOMPARE(model.getAllRecordCount(), 0);
}

void TestSmoke::branchRecordRoundtrip(void)
{
    SmokeModel model;
    model.initFromXML(m_xmlPath);

    // Ветка
    QMap<QString, QString> branchFields;
    branchFields[QStringLiteral("id")]=QStringLiteral("smokeB1");
    branchFields[QStringLiteral("name")]=QStringLiteral("SmokeBranch");
    model.addNewBranch(model.root(), branchFields);

    TreeItem *branch=model.root()->child(0);
    QVERIFY2(branch!=nullptr, "branch must exist after addNewBranch");

    // Запись с текстом
    Record record;
    record.switchToFat();
    record.setField(QStringLiteral("dir"), QStringLiteral("smokeD1"));
    record.setField(QStringLiteral("file"), QStringLiteral("text.html"));
    record.setField(QStringLiteral("id"), QStringLiteral("smokeR1"));
    record.setField(QStringLiteral("name"), QStringLiteral("SmokeNote"));
    record.setField(QStringLiteral("tags"), QStringLiteral("t1, t2"));
    record.setText(QStringLiteral("<p>Hello smoke test</p>"));

    const int pos=branch->recordtableGetTableData()->insertNewRecord(
        GlobalParameters::AddNewRecordBehavior::ADD_TO_END, 0, record);
    QVERIFY2(pos>=0, "insertNewRecord must return valid position");

    // В headless-режиме insertNewRecord всегда генерирует свежие id/dir
    // (вида дерева нет). Дальше сверяем фактические значения
    const RecordTableData *table=branch->recordtableGetTableData();
    m_savedId=table->getField(QStringLiteral("id"), pos);
    m_savedDir=table->getField(QStringLiteral("dir"), pos);
    QVERIFY2(!m_savedId.isEmpty(), "record id must be assigned");
    QVERIFY2(!m_savedDir.isEmpty(), "record dir must be assigned");

    // Сохранение
    model.save();
    QVERIFY2(QFile::exists(m_xmlPath), "mytetra.xml must be written");
    QVERIFY2(QFile::exists(m_baseDir+QStringLiteral("/base/")+m_savedDir+QStringLiteral("/text.html")),
             "record text file must be written");

    // Перечитывание новой моделью
    SmokeModel reloaded;
    reloaded.initFromXML(m_xmlPath);
    QCOMPARE(reloaded.getAllRecordCount(), 1);

    TreeItem *reBranch=reloaded.root()->child(0);
    QVERIFY2(reBranch!=nullptr, "reloaded branch must exist");
    QCOMPARE(reBranch->getField(QStringLiteral("name")), QStringLiteral("SmokeBranch"));

    const RecordTableData *reTable=reBranch->recordtableGetTableData();
    QVERIFY2(reTable!=nullptr, "reloaded record table must exist");
    QCOMPARE(reTable->size(), 1);
    QCOMPARE(reTable->getField(QStringLiteral("name"), 0), QStringLiteral("SmokeNote"));
    QCOMPARE(reTable->getField(QStringLiteral("id"), 0), m_savedId);
    QVERIFY2(reTable->getText(0).contains(QStringLiteral("Hello smoke test")),
             "reloaded text must contain inserted content");
}

void TestSmoke::crossCryptMoveRefused(void)
{
    // Смешанный перенос открытая<->шифрованная запрещен,
    // без ключа - любой перенос внутри шифра
    SmokeModel model;
    model.initFromXML(m_xmlPath);

    QMap<QString, QString> openFields;
    openFields[QStringLiteral("id")]=QStringLiteral("smokeOpen");
    openFields[QStringLiteral("name")]=QStringLiteral("Open");
    model.addNewBranch(model.root(), openFields);

    QMap<QString, QString> cryptFields;
    cryptFields[QStringLiteral("id")]=QStringLiteral("smokeCrypt");
    cryptFields[QStringLiteral("name")]=QStringLiteral("Crypt");
    model.addNewBranch(model.root(), cryptFields);
    TreeItem *cryptBranch=model.getItemById(QStringLiteral("smokeCrypt"));
    QVERIFY2(cryptBranch!=nullptr, "crypt branch must exist");
    cryptBranch->setField(QStringLiteral("crypt"), QStringLiteral("1"));

    QVERIFY2(!model.moveBranch(QStringLiteral("smokeOpen"),
                               QStringLiteral("smokeCrypt"), true),
             "open into crypt must be refused");
    QVERIFY2(!model.moveBranch(QStringLiteral("smokeCrypt"),
                               QStringLiteral("smokeOpen"), true),
             "crypt into open must be refused");
}

QTEST_APPLESS_MAIN(TestSmoke)

#include "test_smoke.moc"

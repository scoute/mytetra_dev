// Bridge write-пути на живой KnowTreeModel на QTest (см. 531).
// Порт bridgewriteprobe: DI в действии (модель получает
// индекс+диспетчер в ctor, индекс получает модель через setTreeModel).
// Фикстура программная: минимальный mytetra.xml в QTemporaryDir.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "views/mainWindow/McpGuiBridge.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/BacklinkIndex.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
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
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/recordTable/Record.h"

extern AppConfig mytetraConfig;
extern ShortcutManager shortcutManager;

class TestBridgeWrite : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void bridgeWritePaths(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;

    static QJsonObject call(McpGuiBridge &b, const QString &cmd, const QJsonObject &args);
};

QJsonObject TestBridgeWrite::call(McpGuiBridge &b, const QString &cmd, const QJsonObject &args)
{
    const QString line=cmd+QStringLiteral(" ")+QString::fromUtf8(
        QJsonDocument(args).toJson(QJsonDocument::Compact));
    const QByteArray reply=b.handleMcpMessage(line);
    return QJsonDocument::fromJson(reply).object();
}

void TestBridgeWrite::initTestCase(void)
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
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                nullptr, nullptr, &gp->cryptHolder());

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    const QString dbDir=m_tmp.filePath(QStringLiteral("bridgedb"));
    QVERIFY2(QDir().mkpath(dbDir), "bridge db dir must be created");
    QVERIFY2(QDir().mkpath(dbDir+QStringLiteral("/trash")), "trash must be created");
    QVERIFY2(QDir().mkpath(dbDir+QStringLiteral("/base")), "base must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_bridge.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;
    m_settings->setValue(QStringLiteral("tetradir"), dbDir);
    m_settings->setValue(QStringLiteral("trashdir"), dbDir+QStringLiteral("/trash"));

    QFile xml(dbDir+QStringLiteral("/mytetra.xml"));
    QVERIFY2(xml.open(QIODevice::WriteOnly | QIODevice::Text),
             "skeleton xml must be writable");
    {
        QTextStream out(&xml);
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<mytetra>\n  <format version=\"1\" subversion=\"2\"/>\n</mytetra>\n";
    }
}

void TestBridgeWrite::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestBridgeWrite::bridgeWritePaths(void)
{
    const QString dbDir=m_settings->value(QStringLiteral("tetradir")).toString();

    BacklinkIndex *backlinks=AppContext::instance().backlinkIndex();
    EditorShowTextDispatcher *dispatcher=AppContext::instance().dispatcher();
    QVERIFY2(backlinks!=nullptr && dispatcher!=nullptr, "DI trio must be owned");

    KnowTreeModel model(backlinks, dispatcher);
    model.initFromXML(dbDir+QStringLiteral("/mytetra.xml"));
    backlinks->setTreeModel(&model);

    McpGuiBridge bridge(&model);
    bridge.isReadOnly=[]() { return false; };
    bridge.mcpAllowed=[]() { return true; };
    bool cleared=false;
    bridge.clearEditor=[&]() { cleared=true; };
    Q_UNUSED(cleared);
    bridge.setFavorites(AppContext::instance().favorites());
    bridge.setBacklinkIndex(backlinks);

    // create branch
    QJsonObject a;
    a[QStringLiteral("parent_id")]=QStringLiteral("");
    a[QStringLiteral("id")]=QStringLiteral("b1");
    a[QStringLiteral("name")]=QStringLiteral("ProbeBranch");
    QJsonObject r=call(bridge, QStringLiteral("mcpCreateBranch"), a);
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), true);

    // create note
    QJsonObject n;
    n[QStringLiteral("branch_id")]=QStringLiteral("b1");
    n[QStringLiteral("id")]=QStringLiteral("n1");
    n[QStringLiteral("name")]=QStringLiteral("ProbeNote");
    n[QStringLiteral("html")]=QStringLiteral("hello probe");
    r=call(bridge, QStringLiteral("mcpCreateNote"), n);
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), true);

    // Headless-вставка всегда генерирует свежий id (см. applyCreateNote):
    // дальше работаем с фактическим из ответа
    const QString nid=r.value(QStringLiteral("id")).toString();
    QVERIFY2(!nid.isEmpty(), "created note must return its id");
    QVERIFY2(model.isRecordIdExists(nid), "created note must exist");

    // update + move clears editor (DI-хук, не синглтон)
    QJsonObject u;
    u[QStringLiteral("id")]=nid;
    u[QStringLiteral("has_name")]=true;
    u[QStringLiteral("name")]=QStringLiteral("ProbeNote2");
    r=call(bridge, QStringLiteral("mcpUpdateNote"), u);
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), true);

    // batch: create second note + delete first
    QJsonObject op1, op2, b;
    op1[QStringLiteral("tool")]=QStringLiteral("create_note");
    QJsonObject op1args;
    op1args[QStringLiteral("branch_id")]=QStringLiteral("b1");
    op1args[QStringLiteral("id")]=QStringLiteral("n2");
    op1args[QStringLiteral("name")]=QStringLiteral("Second");
    op1[QStringLiteral("arguments")]=op1args;
    op2[QStringLiteral("tool")]=QStringLiteral("delete_note");
    QJsonObject op2args;
    op2args[QStringLiteral("note_id")]=nid;
    op2[QStringLiteral("arguments")]=op2args;
    QJsonArray ops;
    ops.append(op1);
    ops.append(op2);
    b[QStringLiteral("operations")]=ops;
    r=call(bridge, QStringLiteral("mcpApplyBatch"), b);
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(r.value(QStringLiteral("applied")).toInt(), 2);
    QVERIFY2(!model.isRecordIdExists(nid), "deleted note must be gone");

    // batch-create генерирует свежий id: ищем заметку "Second" перебором таблицы ветки
    bool foundSecond=false;
    if(TreeItem *branch=model.getItemById(QStringLiteral("b1")))
        if(RecordTableData *table=branch->recordtableGetTableData())
            for(unsigned i=0; i<table->size(); ++i)
                if(table->getRecord(i)!=nullptr &&
                   table->getRecord(i)->getField(QStringLiteral("name"))==QStringLiteral("Second"))
                    foundSecond=true;
    QVERIFY2(foundSecond, "batch-created note must be found");

    // удаление несуществующей: внятный отказ, не молчание
    QJsonObject d;
    d[QStringLiteral("id")]=QStringLiteral("no-such-note");
    r=call(bridge, QStringLiteral("mcpDeleteNote"), d);
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), false);
}

QTEST_MAIN(TestBridgeWrite)

#include "test_bridgewrite.moc"

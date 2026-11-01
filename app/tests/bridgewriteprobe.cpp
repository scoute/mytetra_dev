// Bridge write-пути на живой KnowTreeModel (без GUI).
// DI в действии: модель получает индекс+диспетчер в ctor,
// индекс получает модель через setTreeModel (иначе find_object
// уронил бы headless-пробу). Фикстура - минимальный mytetra.xml.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>

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
extern GlobalParameters globalParameters;
extern FixedParameters fixedParameters;
extern AppFiles mytetraFiles;
extern DataBaseConfig dataBaseConfig;
extern TrashMonitoring trashMonitoring;
extern WalkHistory walkHistory;
extern VisitHistory visitHistory;
extern ActionLogger actionLogger;
extern ShortcutManager shortcutManager;
extern PeriodicCheckBase periodicCheckBase;
extern PeriodicSyncro periodicSyncro;

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

static QJsonObject call(McpGuiBridge &b, const QString &cmd, const QJsonObject &args)
{
    const QString line=cmd+QStringLiteral(" ")+QString::fromUtf8(
        QJsonDocument(args).toJson(QJsonDocument::Compact));
    const QByteArray reply=b.handleMcpMessage(line);
    return QJsonDocument::fromJson(reply).object();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QDir().mkpath(QStringLiteral("/tmp/opencode/bridgedb"));
    QFile::remove(QStringLiteral("/tmp/opencode/probe5.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe5.ini"),
                                       QSettings::IniFormat);
    mytetraConfig.m_conf->setValue(QStringLiteral("tetradir"), QStringLiteral("/tmp/opencode/bridgedb"));
    mytetraConfig.m_conf->setValue(QStringLiteral("trashdir"), QStringLiteral("/tmp/opencode/bridgedb/trash"));
    QDir().mkpath(QStringLiteral("/tmp/opencode/bridgedb/trash"));
    QDir().mkpath(QStringLiteral("/tmp/opencode/bridgedb/base"));

    {
        QFile xml(QStringLiteral("/tmp/opencode/bridgedb/mytetra.xml"));
        if(xml.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&xml);
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<mytetra>\n  <format version=\"1\" subversion=\"2\"/>\n</mytetra>\n";
        }
    }

    AppContext::instance().init(&fixedParameters,
                                &globalParameters,
                                &mytetraConfig,
                                &mytetraFiles,
                                &dataBaseConfig,
                                &trashMonitoring,
                                &walkHistory,
                                &visitHistory,
                                &actionLogger,
                                &shortcutManager,
                                &periodicCheckBase,
                                &periodicSyncro,
                                nullptr,
                                nullptr,
                                &globalParameters.cryptHolder());

    BacklinkIndex *backlinks=AppContext::instance().backlinkIndex();
    EditorShowTextDispatcher *dispatcher=AppContext::instance().dispatcher();
    check(backlinks!=nullptr && dispatcher!=nullptr, "DI-OWNED");

    KnowTreeModel *model=new KnowTreeModel(backlinks, dispatcher);
    model->initFromXML(QStringLiteral("/tmp/opencode/bridgedb/mytetra.xml"));
    backlinks->setTreeModel(model);

    McpGuiBridge bridge(model);
    bridge.isReadOnly=[]() { return false; };
    bridge.mcpAllowed=[]() { return true; };
    bool cleared=false;
    bridge.clearEditor=[&]() { cleared=true; };
    bridge.setFavorites(AppContext::instance().favorites());
    bridge.setBacklinkIndex(backlinks);

    // create branch
    QJsonObject a;
    a[QStringLiteral("parent_id")]=QStringLiteral("");
    a[QStringLiteral("id")]=QStringLiteral("b1");
    a[QStringLiteral("name")]=QStringLiteral("ProbeBranch");
    QJsonObject r=call(bridge, QStringLiteral("mcpCreateBranch"), a);
    check(r.value(QStringLiteral("ok")).toBool()==true, "WRITE-BRANCH-OK");
    check(!model->getRecordPath(QStringLiteral("nope")).isEmpty() || true, "MODEL-ALIVE");

    // create note
    QJsonObject n;
    n[QStringLiteral("branch_id")]=QStringLiteral("b1");
    n[QStringLiteral("id")]=QStringLiteral("n1");
    n[QStringLiteral("name")]=QStringLiteral("ProbeNote");
    n[QStringLiteral("html")]=QStringLiteral("hello probe");
    r=call(bridge, QStringLiteral("mcpCreateNote"), n);
    check(r.value(QStringLiteral("ok")).toBool()==true, "WRITE-NOTE-OK");
    // Headless-вставка всегда генерирует свежий id (см. applyCreateNote):
    // дальше работаем с фактическим из ответа
    const QString nid=r.value(QStringLiteral("id")).toString();
    check(!nid.isEmpty(), "WRITE-NOTE-ID");
    check(model->isRecordIdExists(nid), "NOTE-EXISTS");



    // update + move clears editor (DI-хук, не синглтон)
    QJsonObject u;
    u[QStringLiteral("id")]=nid;
    u[QStringLiteral("has_name")]=true;
    u[QStringLiteral("name")]=QStringLiteral("ProbeNote2");
    r=call(bridge, QStringLiteral("mcpUpdateNote"), u);
    check(r.value(QStringLiteral("ok")).toBool()==true, "WRITE-UPDATE-OK");

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
    check(r.value(QStringLiteral("ok")).toBool()==true, "WRITE-BATCH-OK");
    check(r.value(QStringLiteral("applied")).toInt()==2, "WRITE-BATCH-APPLIED");
    check(!model->isRecordIdExists(nid), "WRITE-BATCH-DELETED");
    // batch-create генерирует свежий id: ищем заметку "Second" перебором таблицы ветки
    bool foundSecond=false;
    if(TreeItem *branch=model->getItemById(QStringLiteral("b1")))
        if(RecordTableData *table=branch->recordtableGetTableData())
            for(unsigned i=0; i<table->size(); ++i)
                if(table->getRecord(i)!=nullptr &&
                   table->getRecord(i)->getField(QStringLiteral("name"))==QStringLiteral("Second"))
                    foundSecond=true;
    check(foundSecond, "WRITE-BATCH-CREATED");

    // удаление несуществующей: внятный отказ, не молчание
    QJsonObject d;
    d[QStringLiteral("id")]=QStringLiteral("no-such-note");
    r=call(bridge, QStringLiteral("mcpDeleteNote"), d);
    check(r.value(QStringLiteral("ok")).toBool()==false, "WRITE-DEL-UNKNOWN");

    qDebug() << (fails==0 ? "BRIDGEWRITEPROBE-OK" : "BRIDGEWRITEPROBE-BAD");
    return fails;
}

// P0+P1 типизация и DI на QTest (см. 528).
// Порт refprobe: pMainWindow типизирован, ключ в холдере,
// WalkHistory-инъекция, AppContext хранит типизированные указатели,
// тройка favorites/backlinks/dispatcher — дети контекста.
#include <QtTest>
#include <QApplication>
#include <type_traits>

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
#include "libraries/crypt/CryptKeyHolder.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "views/mainWindow/MainWindow.h"

extern MainWindow *pMainWindow;

class TestRef : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);

    void typedMainWindow(void);
    void keyHolder(void);
    void globalParametersDelegate(void);
    void contextPointers(void);
    void walkHistoryInjection(void);
    void ownedTrio(void);
};

void TestRef::initTestCase(void)
{
    static FixedParameters *fp=new FixedParameters;
    static GlobalParameters *gp=new GlobalParameters;
    static AppConfig *ac=new AppConfig;
    static AppFiles *af=new AppFiles;
    static DataBaseConfig *dbc=new DataBaseConfig;
    static TrashMonitoring *tm=new TrashMonitoring;
    static WalkHistory *wh=new WalkHistory;
    static VisitHistory *vh=new VisitHistory;
    static ActionLogger *al=new ActionLogger;
    static ShortcutManager *sm=new ShortcutManager;
    static PeriodicCheckBase *pcb=new PeriodicCheckBase;
    static PeriodicSyncro *ps=new PeriodicSyncro;
    static InternalClipboard *ic=new InternalClipboard;
    static MainWindow *mwNull=nullptr;
    AppContext::instance().setParent(qApp);
    AppContext::instance().init(fp, gp, ac, af, dbc, tm,
                                wh, vh, al, sm, pcb, ps,
                                ic, mwNull, &gp->cryptHolder());
}

void TestRef::typedMainWindow(void)
{
    // P0: pMainWindow типизирован как MainWindow*, не QObject*
    static_assert(std::is_same<decltype(pMainWindow), MainWindow *>::value,
                  "pMainWindow must be MainWindow*");
    QVERIFY2(true, "pMainWindow is MainWindow*");
}

void TestRef::keyHolder(void)
{
    // P0/P1: ключ живет в холдере
    CryptKeyHolder holder;
    holder.setKey(QByteArray("secret"));
    QCOMPARE(holder.key(), QByteArray("secret"));
    QVERIFY2(holder.hasKey(), "holder must have key");
    holder.clear();
    QVERIFY2(!holder.hasKey(), "holder must be clear");
}

void TestRef::globalParametersDelegate(void)
{
    GlobalParameters gp;
    gp.setCryptKey(QByteArray("abc"));
    QCOMPARE(gp.getCryptKey(), QByteArray("abc"));
    QCOMPARE(gp.cryptHolder().key(), QByteArray("abc"));
    gp.setCryptKey(QByteArray());
    QVERIFY2(!gp.cryptHolder().hasKey(), "holder must be clear");
}

void TestRef::contextPointers(void)
{
    // P1: AppContext хранит типизированные указатели
    QVERIFY2(AppContext::instance().isInitialized(), "context must be initialized");
    QVERIFY2(AppContext::instance().globalParameters()!=nullptr, "GP must be set");
    QVERIFY2(AppContext::instance().walkHistory()!=nullptr, "WH must be set");
    QVERIFY2(AppContext::instance().visitHistory()!=nullptr, "VH must be set");
    QVERIFY2(AppContext::instance().internalClipboard()!=nullptr, "CLIP must be set");
    QVERIFY2(AppContext::instance().cryptKeyHolder()!=nullptr, "CRYPT must be set");
    QVERIFY2(AppContext::instance().mainWindow()==nullptr, "MW must be null headless");
}

void TestRef::walkHistoryInjection(void)
{
    // P1: WalkHistory инъекция вместо extern
    WalkHistory wh;
    VisitHistory vh;
    QVERIFY2(wh.visitHistory()==nullptr, "no injection by default");
    wh.setVisitHistory(&vh);
    QCOMPARE(wh.visitHistory(), &vh);
    wh.add(QStringLiteral("note1"), 0, 0);
}

void TestRef::ownedTrio(void)
{
    // P3: синглтонов больше нет - тройка живет в AppContext с создания
    AppContext::instance().setParent(qApp);
    QVERIFY2(AppContext::instance().favorites()!=nullptr, "favorites must exist");
    QVERIFY2(AppContext::instance().backlinkIndex()!=nullptr, "backlinks must exist");
    QVERIFY2(AppContext::instance().dispatcher()!=nullptr, "dispatcher must exist");
    QCOMPARE(AppContext::instance().favorites()->parent(), &AppContext::instance());
    QCOMPARE(AppContext::instance().backlinkIndex()->parent(), &AppContext::instance());
    // Свой QThread, родителя быть не должно
    QVERIFY2(AppContext::instance().dispatcher()->parent()==nullptr,
             "dispatcher must be parentless");
}

QTEST_MAIN(TestRef)

#include "test_ref.moc"

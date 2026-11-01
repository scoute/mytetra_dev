// P0+P1 стенд: типизация pMainWindow, CryptKeyHolder, WalkHistory-инъекция, AppContext
#include <QApplication>
#include <QDebug>
#include <QTimer>
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

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // P0: pMainWindow типизирован как MainWindow*, не QObject*
    static_assert(std::is_same<decltype(pMainWindow), MainWindow *>::value,
                  "pMainWindow must be MainWindow*");
    check(true, "TYPED-PMAINWINDOW");

    // P0/P1: ключ живет в холдере, GlobalParameters делегирует
    CryptKeyHolder holder;
    holder.setKey(QByteArray("secret"));
    check(holder.key()==QByteArray("secret"), "HOLDER-SETGET");
    check(holder.hasKey(), "HOLDER-HASKEY");
    holder.clear();
    check(!holder.hasKey(), "HOLDER-CLEAR");

    GlobalParameters gp;
    gp.setCryptKey(QByteArray("abc"));
    check(gp.getCryptKey()==QByteArray("abc"), "GP-DELEGATE");
    check(gp.cryptHolder().key()==QByteArray("abc"), "GP-HOLDER");
    gp.setCryptKey(QByteArray());
    check(!gp.cryptHolder().hasKey(), "GP-CLEAR");

    WalkHistory wh;
    VisitHistory vh;

    // P1: AppContext хранит типизированные указатели
    FixedParameters fp;
    AppConfig ac;
    AppFiles af;
    DataBaseConfig dbc;
    TrashMonitoring tm;
    ActionLogger al;
    ShortcutManager sm;
    PeriodicCheckBase pcb;
    PeriodicSyncro ps;
    InternalClipboard ic(&app);
    MainWindow *mwNull=nullptr;
    AppContext::instance().setParent(&app);
    AppContext::instance().init(&fp, &gp, &ac, &af, &dbc, &tm,
                                &wh, &vh, &al, &sm, &pcb, &ps,
                                &ic, mwNull, &gp.cryptHolder());
    check(AppContext::instance().isInitialized(), "CTX-INIT");
    check(AppContext::instance().globalParameters()==&gp, "CTX-GP");
    check(AppContext::instance().walkHistory()==&wh, "CTX-WH");
    check(AppContext::instance().visitHistory()==&vh, "CTX-VH");
    check(AppContext::instance().internalClipboard()==&ic, "CTX-CLIP");
    check(AppContext::instance().cryptKeyHolder()==&gp.cryptHolder(), "CTX-CRYPT");
    check(AppContext::instance().mainWindow()==nullptr, "CTX-MWNULL");

    // P1: WalkHistory инъекция вместо extern
    check(wh.visitHistory()==nullptr, "WH-NOINJECT");
    wh.setVisitHistory(&vh);
    check(wh.visitHistory()==&vh, "WH-INJECT");
    wh.add(QStringLiteral("note1"), 0, 0);
    check(true, "WH-ADD-NOCRASH");


    // P3: синглтонов больше нет - тройка живет в AppContext с создания.
    // setParent(app) в main переподчиняет их вместе с ядром
    AppContext::instance().setParent(&app);
    check(AppContext::instance().favorites()!=nullptr, "CTX-FAV");
    check(AppContext::instance().backlinkIndex()!=nullptr, "CTX-BACK");
    check(AppContext::instance().dispatcher()!=nullptr, "CTX-DISP");
    check(AppContext::instance().favorites()->parent()==&AppContext::instance(), "FAV-OWNED");
    check(AppContext::instance().backlinkIndex()->parent()==&AppContext::instance(), "BACK-OWNED");
    check(AppContext::instance().dispatcher()->parent()==nullptr, "DISP-NOPARENT"); // свой QThread, родителя быть не должно

    qDebug() << (fails==0 ? "REFPROBE-OK" : "REFPROBE-BAD");
    return fails;
}

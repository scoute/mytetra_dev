// P2 стенд: фасады AppConfig пишут те же ключи QSettings,
// секции читают обратно. Ключи не переименованы.
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QFile>

#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigStorage.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/appConfig/AppConfigFind.h"
#include "models/appConfig/AppConfigSync.h"
#include "models/appConfig/AppConfigCrypto.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigClipper.h"
#include "models/appConfig/AppConfigServices.h"
#include "models/appConfig/AppConfigGeneral.h"
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
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"

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

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QFile::remove(QStringLiteral("/tmp/opencode/probe2.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe2.ini"),
                                       QSettings::IniFormat);
    // P3: секции напрямую, контекст как в main()
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
    QSettings *raw=mytetraConfig.m_conf;

    // Storage
    AppContext::instance().appConfig()->storageSection()->set_trashmaxfilecount(42);
    check(AppContext::instance().appConfig()->storageSection()->get_trashmaxfilecount()==42, "STORAGE-FILECOUNT");
    check(raw->value("trashmaxfilecount").toInt()==42, "STORAGE-KEY");
    AppContext::instance().appConfig()->storageSection()->setEnableLogging(true);
    check(AppContext::instance().appConfig()->storageSection()->getEnableLogging()==true, "STORAGE-LOGGING");
    AppContext::instance().appConfig()->storageSection()->set_attachNameMode(1);
    check(AppContext::instance().appConfig()->storageSection()->get_attachNameMode()==1, "STORAGE-NAMEMODE");

    // Ui
    AppContext::instance().appConfig()->uiSection()->setInterfaceMode(QStringLiteral("mobile"));
    check(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()==QStringLiteral("mobile"), "UI-MODE");
    check(raw->value("interfaceMode").toString()==QStringLiteral("mobile"), "UI-KEY");
    AppContext::instance().appConfig()->uiSection()->set_treeFilterVisible(true);
    check(AppContext::instance().appConfig()->uiSection()->get_treeFilterVisible()==true, "UI-TREEFILTER");
    AppContext::instance().appConfig()->uiSection()->set_themeCustomTableRowHeight(33);
    check(AppContext::instance().appConfig()->uiSection()->get_themeCustomTableRowHeight()==33, "UI-ROWHEIGHT");
    AppContext::instance().appConfig()->uiSection()->setEditorCursorPosition(123);
    check(AppContext::instance().appConfig()->uiSection()->getEditorCursorPosition()==123, "UI-CURSOR");
    AppContext::instance().appConfig()->uiSection()->setShowSplashScreen(false);
    check(AppContext::instance().appConfig()->uiSection()->getShowSplashScreen()==false, "UI-SPLASH");

    // Find
    AppContext::instance().appConfig()->findSection()->set_findscreen_wordregard(2);
    check(AppContext::instance().appConfig()->findSection()->get_findscreen_wordregard()==2, "FIND-WORD");
    AppContext::instance().appConfig()->findSection()->set_findscreen_show(true);
    check(AppContext::instance().appConfig()->findSection()->get_findscreen_show()==true, "FIND-SHOW");

    // Sync
    AppContext::instance().appConfig()->syncSection()->set_synchrocommand(QStringLiteral("echo hi"));
    check(AppContext::instance().appConfig()->syncSection()->get_synchrocommand()==QStringLiteral("echo hi"), "SYNC-CMD");
    AppContext::instance().appConfig()->syncSection()->set_historySquashKeep(77);
    check(AppContext::instance().appConfig()->syncSection()->get_historySquashKeep()==77, "SYNC-SQUASH");
    AppContext::instance().appConfig()->syncSection()->set_autosaveEnable(true);
    check(AppContext::instance().appConfig()->syncSection()->get_autosaveEnable()==true, "SYNC-AUTOSAVE");

    // Crypto
    AppContext::instance().appConfig()->cryptoSection()->set_howpassrequest(QStringLiteral("atStartProgram"));
    check(AppContext::instance().appConfig()->cryptoSection()->get_howpassrequest()==QStringLiteral("atStartProgram"), "CRYPTO-HOW");
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
    check(AppContext::instance().appConfig()->cryptoSection()->get_cryptMethod()==QStringLiteral("xchacha"), "CRYPTO-METHOD");
    check(raw->value("cryptMethod").toString()==QStringLiteral("xchacha"), "CRYPTO-KEY");

    // RecordTable (readOnly с рантайм-локом)
    AppContext::instance().appConfig()->recordTableSection()->set_readOnly(false);
    mytetraConfig.setMcpUiLock(false);
    check(AppContext::instance().appConfig()->recordTableSection()->get_readOnly()==false, "RT-RW");
    mytetraConfig.setMcpUiLock(true);
    check(AppContext::instance().appConfig()->recordTableSection()->get_readOnly()==true, "RT-LOCK");
    check(mytetraConfig.isMcpUiLocked()==true, "RT-ISLOCK");
    check(AppContext::instance().appConfig()->recordTableSection()->isGlobalReadOnly()==false, "RT-GLOBAL");
    mytetraConfig.setMcpUiLock(false);
    AppContext::instance().appConfig()->recordTableSection()->setRecordTableShowVerticalHeaders(true);
    check(AppContext::instance().appConfig()->recordTableSection()->getRecordTableShowVerticalHeaders()==true, "RT-VHEADER");

    // Clipper
    AppContext::instance().appConfig()->clipperSection()->set_clipperMaxImages(5);
    check(AppContext::instance().appConfig()->clipperSection()->get_clipperMaxImages()==5, "CLIP-IMAGES");
    check(raw->value("clipperMaxImages").toInt()==5, "CLIP-KEY");
    AppContext::instance().appConfig()->clipperSection()->set_clipperbranchmode(2);
    check(AppContext::instance().appConfig()->clipperSection()->get_clipperbranchmode()==2, "CLIP-BRANCHMODE");

    // Services (голос)
    AppContext::instance().appConfig()->servicesSection()->set_voiceNoteQuality(5);
    check(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality()==5, "SVC-QUALITY");
    check(raw->value("voiceNoteQuality").toInt()==5, "SVC-KEY");
    AppContext::instance().appConfig()->servicesSection()->set_voiceNoteFormat(QStringLiteral("opus"));
    check(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteFormat()==QStringLiteral("opus"), "SVC-FORMAT");
    AppContext::instance().appConfig()->servicesSection()->set_mcpAllow(true);
    check(AppContext::instance().appConfig()->servicesSection()->get_mcpAllow()==true, "SVC-MCP");
    AppContext::instance().appConfig()->servicesSection()->set_telegramImportMode(QStringLiteral("day"));
    check(AppContext::instance().appConfig()->servicesSection()->get_telegramImportMode()==QStringLiteral("day"), "SVC-TG");

    // get_parameter фолбэк на дефолт известной таблицы
    QString v=mytetraConfig.get_parameter("voiceNoteQuality");
    check(v==QStringLiteral("5"), "CORE-PARAM");

    // Счетчик фолбэков: свежий ini без известных ключей
    QFile::remove(QStringLiteral("/tmp/opencode/probe3.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe3.ini"),
                                       QSettings::IniFormat);
    raw=mytetraConfig.m_conf;
    const int fb0=mytetraConfig.get_configFallbackCount();
    AppContext::instance().appConfig()->generalSection()->get_interfacelanguage();
    check(mytetraConfig.get_configFallbackCount()==fb0+1, "FB-COUNT-1");
    check(mytetraConfig.get_configFallbackHits().contains(QStringLiteral("interfacelanguage")), "FB-NAME");
    AppContext::instance().appConfig()->generalSection()->get_interfacelanguage();
    check(mytetraConfig.get_configFallbackCount()==fb0+2, "FB-COUNT-2");
    check(mytetraConfig.get_configFallbackHits().value(QStringLiteral("interfacelanguage"))==2, "FB-DEDUP");
    AppContext::instance().appConfig()->cryptoSection()->get_howpassrequest();
    check(mytetraConfig.get_configFallbackCount()==fb0+3, "FB-COUNT-3");
    check(mytetraConfig.get_configFallbackHits().size()>=2, "FB-TWOKEYS");

    // Актуальная таблица: единый источник вместо захардкоженного номера.
    // Регрессия stale-60: ключ 61-й таблицы обязан быть известен
    QStringList actualTable=mytetraConfig.get_actual_parameter_table(true);
    check(actualTable.contains(QStringLiteral("attachViewMode")), "TABLE-HAS-NEWEST");
    check(actualTable.contains(QStringLiteral("tetradir")), "TABLE-HAS-OLD");
    check(actualTable.contains(QStringLiteral("mcpAllow")), "TABLE-HAS-MID");
    check(actualTable.size()>=294, "TABLE-SIZE"); // 97 троек + сигнатура; миграция только растит

    qDebug() << (fails==0 ? "CONFPROBE-OK" : "CONFPROBE-BAD");
    return fails;
}

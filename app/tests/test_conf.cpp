// Секции AppConfig на QTest (см. 519).
// Порт confprobe: фасады пишут те же ключи QSettings,
// секции читают обратно. Ключи не переименованы.
// Полностью программный: свежие ini в QTemporaryDir, без фикстур.
#include <QtTest>
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QTemporaryDir>
#include <QPageSize>
#include <QPageLayout>

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
#include "libraries/wyedit/formatters/ReferenceFormatter.h"
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
extern ShortcutManager shortcutManager;

class TestConf : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void storageSection(void);
    void folderBadgeHref(void);
    void uiSection(void);
    void findSection(void);
    void syncSection(void);
    void cryptoSection(void);
    void recordTableSection(void);
    void clipperSection(void);
    void servicesSection(void);
    void fallbackCounters(void);
    void actualTable(void);
    void tableBoundsSafe(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;
    QSettings *m_raw=nullptr;
};

void TestConf::initTestCase(void)
{
    static FixedParameters *fp=new FixedParameters;
    static GlobalParameters *gp=new GlobalParameters;
    static AppFiles *af=new AppFiles;
    static DataBaseConfig *dbc=new DataBaseConfig;
    static TrashMonitoring *tm=new TrashMonitoring;
    static WalkHistory *wh=new WalkHistory;
    static VisitHistory *vh=new VisitHistory;
    static ActionLogger *al=new ActionLogger;
    static ShortcutManager *sm=new ShortcutManager;
    static PeriodicCheckBase *pcb=new PeriodicCheckBase;
    static PeriodicSyncro *ps=new PeriodicSyncro;
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, sm, pcb, ps,
                                nullptr, nullptr, &gp->cryptHolder());

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_conf.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;
    m_raw=m_settings;
}

void TestConf::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestConf::storageSection(void)
{
    AppContext::instance().appConfig()->storageSection()->set_trashmaxfilecount(42);
    QCOMPARE(AppContext::instance().appConfig()->storageSection()->get_trashmaxfilecount(), 42);
    QCOMPARE(m_raw->value("trashmaxfilecount").toInt(), 42);
    AppContext::instance().appConfig()->storageSection()->setEnableLogging(true);
    QCOMPARE(AppContext::instance().appConfig()->storageSection()->getEnableLogging(), true);
    AppContext::instance().appConfig()->storageSection()->set_attachNameMode(1);
    QCOMPARE(AppContext::instance().appConfig()->storageSection()->get_attachNameMode(), 1);
    // Дефолт ссылок на папки — спрашивать (ключ еще не выставлялся)
    QCOMPARE(AppContext::instance().appConfig()->storageSection()->get_attachLinkMode(), AppConfig::AttachLinkAsk);
    AppContext::instance().appConfig()->storageSection()->set_attachLinkMode(AppConfig::AttachLinkRelative);
    QCOMPARE(AppContext::instance().appConfig()->storageSection()->get_attachLinkMode(), AppConfig::AttachLinkRelative);
    QCOMPARE(m_raw->value("attachLinkMode").toInt(), AppConfig::AttachLinkRelative);
    // Мусор в ключе покрывается дефолтом спрашивать
    AppContext::instance().appConfig()->storageSection()->set_attachLinkMode(99);
    QCOMPARE(AppContext::instance().appConfig()->storageSection()->get_attachLinkMode(), AppConfig::AttachLinkAsk);
}

void TestConf::folderBadgeHref(void)
{
    // Канал меток папок attachdir:// не пересекается с файловым attach://
    QVERIFY(ReferenceFormatter::isFolderLinkHref(QStringLiteral("attachdir://abc123")));
    QVERIFY(!ReferenceFormatter::isFolderLinkHref(QStringLiteral("attach://file.txt")));
    QVERIFY(!ReferenceFormatter::isFolderLinkHref(QString()));
    QCOMPARE(ReferenceFormatter::folderRowIdFromHref(QStringLiteral("attachdir://abc123")),
             QStringLiteral("abc123"));
    QVERIFY(ReferenceFormatter::folderRowIdFromHref(QStringLiteral("attach://file.txt")).isEmpty());
    QVERIFY(!ReferenceFormatter::isAttachHref(QStringLiteral("attachdir://abc123")));
}

void TestConf::uiSection(void)
{
    AppContext::instance().appConfig()->uiSection()->setInterfaceMode(QStringLiteral("mobile"));
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->getInterfaceMode(),
             QStringLiteral("mobile"));
    QCOMPARE(m_raw->value("interfaceMode").toString(), QStringLiteral("mobile"));
    AppContext::instance().appConfig()->uiSection()->set_treeFilterVisible(true);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_treeFilterVisible(), true);
    AppContext::instance().appConfig()->uiSection()->set_themeCustomTableRowHeight(33);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_themeCustomTableRowHeight(), 33);
    AppContext::instance().appConfig()->uiSection()->setEditorCursorPosition(123);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->getEditorCursorPosition(), 123);
    AppContext::instance().appConfig()->uiSection()->setShowSplashScreen(false);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->getShowSplashScreen(), false);
    // Дефолт положения номеров — справа (ключ еще не выставлялся)
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printFooterAlign(), 2);
    AppContext::instance().appConfig()->uiSection()->set_printFooterAlign(2);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printFooterAlign(), 2);
    QCOMPARE(m_raw->value("printFooterAlign").toInt(), 2);
    AppContext::instance().appConfig()->uiSection()->set_printFooterEnabled(false);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printFooterEnabled(), false);
    QCOMPARE(m_raw->value("printFooterEnabled").toBool(), false);
    // Поля страницы: дефолтные 20/5/5/5, автосейв безусловный
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printMargins(),
             QMarginsF(20.0, 5.0, 5.0, 5.0));
    AppContext::instance().appConfig()->uiSection()->set_printMargins(QMarginsF(10.0, 11.0, 12.0, 13.0));
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printMargins(),
             QMarginsF(10.0, 11.0, 12.0, 13.0));
    QCOMPARE(m_raw->value("printMarginLeft").toDouble(), 10.0);
    QCOMPARE(m_raw->value("printMarginBottom").toDouble(), 13.0);
    // Отрицательные поля отрезаются в ноль и на записи, и на чтении
    AppContext::instance().appConfig()->uiSection()->set_printMargins(QMarginsF(-5.0, 5.0, 5.0, 5.0));
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printMargins().left(), 0.0);
    // Бумага и ориентация: дефолты A4/портрет, мусор отрезается
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printPageSizeId(),
             static_cast<int>(QPageSize::A4));
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printPageOrientation(),
             static_cast<int>(QPageLayout::Portrait));
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->has_savedPrintPageSetup(), true);
    AppContext::instance().appConfig()->uiSection()->set_printPageSizeId(QPageSize::Letter);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printPageSizeId(),
             static_cast<int>(QPageSize::Letter));
    AppContext::instance().appConfig()->uiSection()->set_printPageOrientation(QPageLayout::Landscape);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printPageOrientation(),
             static_cast<int>(QPageLayout::Landscape));
    AppContext::instance().appConfig()->uiSection()->set_printPageSizeId(999999);
    QCOMPARE(AppContext::instance().appConfig()->uiSection()->get_printPageSizeId(),
             static_cast<int>(QPageSize::A4));
}

void TestConf::findSection(void)
{
    AppContext::instance().appConfig()->findSection()->set_findscreen_wordregard(2);
    QCOMPARE(AppContext::instance().appConfig()->findSection()->get_findscreen_wordregard(), 2);
    AppContext::instance().appConfig()->findSection()->set_findscreen_show(true);
    QCOMPARE(AppContext::instance().appConfig()->findSection()->get_findscreen_show(), true);
}

void TestConf::syncSection(void)
{
    AppContext::instance().appConfig()->syncSection()->set_synchrocommand(QStringLiteral("echo hi"));
    QCOMPARE(AppContext::instance().appConfig()->syncSection()->get_synchrocommand(),
             QStringLiteral("echo hi"));
    AppContext::instance().appConfig()->syncSection()->set_historySquashKeep(77);
    QCOMPARE(AppContext::instance().appConfig()->syncSection()->get_historySquashKeep(), 77);
    AppContext::instance().appConfig()->syncSection()->set_autosaveEnable(true);
    QCOMPARE(AppContext::instance().appConfig()->syncSection()->get_autosaveEnable(), true);
}

void TestConf::cryptoSection(void)
{
    AppContext::instance().appConfig()->cryptoSection()->set_howpassrequest(QStringLiteral("atStartProgram"));
    QCOMPARE(AppContext::instance().appConfig()->cryptoSection()->get_howpassrequest(),
             QStringLiteral("atStartProgram"));
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
    QCOMPARE(AppContext::instance().appConfig()->cryptoSection()->get_cryptMethod(),
             QStringLiteral("xchacha"));
    QCOMPARE(m_raw->value("cryptMethod").toString(), QStringLiteral("xchacha"));
}

void TestConf::recordTableSection(void)
{
    // readOnly с рантайм-локом
    AppContext::instance().appConfig()->recordTableSection()->set_readOnly(false);
    mytetraConfig.setMcpUiLock(false);
    QCOMPARE(AppContext::instance().appConfig()->recordTableSection()->get_readOnly(), false);
    mytetraConfig.setMcpUiLock(true);
    QCOMPARE(AppContext::instance().appConfig()->recordTableSection()->get_readOnly(), true);
    QCOMPARE(mytetraConfig.isMcpUiLocked(), true);
    QCOMPARE(AppContext::instance().appConfig()->recordTableSection()->isGlobalReadOnly(), false);
    mytetraConfig.setMcpUiLock(false);
    AppContext::instance().appConfig()->recordTableSection()->setRecordTableShowVerticalHeaders(true);
    QCOMPARE(AppContext::instance().appConfig()->recordTableSection()->getRecordTableShowVerticalHeaders(),
             true);
}

void TestConf::clipperSection(void)
{
    AppContext::instance().appConfig()->clipperSection()->set_clipperMaxImages(5);
    QCOMPARE(AppContext::instance().appConfig()->clipperSection()->get_clipperMaxImages(), 5);
    QCOMPARE(m_raw->value("clipperMaxImages").toInt(), 5);
    AppContext::instance().appConfig()->clipperSection()->set_clipperbranchmode(2);
    QCOMPARE(AppContext::instance().appConfig()->clipperSection()->get_clipperbranchmode(), 2);
}

void TestConf::servicesSection(void)
{
    AppContext::instance().appConfig()->servicesSection()->set_voiceNoteQuality(5);
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteQuality(), 5);
    QCOMPARE(m_raw->value("voiceNoteQuality").toInt(), 5);
    AppContext::instance().appConfig()->servicesSection()->set_voiceNoteFormat(QStringLiteral("opus"));
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_voiceNoteFormat(),
             QStringLiteral("opus"));
    AppContext::instance().appConfig()->servicesSection()->set_mcpAllow(true);
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_mcpAllow(), true);
    AppContext::instance().appConfig()->servicesSection()->set_telegramImportMode(QStringLiteral("day"));
    QCOMPARE(AppContext::instance().appConfig()->servicesSection()->get_telegramImportMode(),
             QStringLiteral("day"));

    // get_parameter фолбэк на дефолт известной таблицы
    QCOMPARE(mytetraConfig.get_parameter(QStringLiteral("voiceNoteQuality")),
             QStringLiteral("5"));
}

void TestConf::fallbackCounters(void)
{
    // Свежий ini без известных ключей
    QSettings *fresh=new QSettings(m_tmp.filePath(QStringLiteral("test_conf_fresh.ini")),
                                   QSettings::IniFormat, this);
    mytetraConfig.m_conf=fresh;
    m_raw=fresh;

    const int fb0=mytetraConfig.get_configFallbackCount();
    AppContext::instance().appConfig()->generalSection()->get_interfacelanguage();
    QCOMPARE(mytetraConfig.get_configFallbackCount(), fb0+1);
    QVERIFY2(mytetraConfig.get_configFallbackHits().contains(QStringLiteral("interfacelanguage")),
             "fallback hit must be named");
    AppContext::instance().appConfig()->generalSection()->get_interfacelanguage();
    QCOMPARE(mytetraConfig.get_configFallbackCount(), fb0+2);
    QCOMPARE(mytetraConfig.get_configFallbackHits().value(QStringLiteral("interfacelanguage")), 2);
    AppContext::instance().appConfig()->cryptoSection()->get_howpassrequest();
    QCOMPARE(mytetraConfig.get_configFallbackCount(), fb0+3);
    QVERIFY2(mytetraConfig.get_configFallbackHits().size()>=2,
             "two distinct keys must be counted");
}

void TestConf::actualTable(void)
{
    // Актуальная таблица: единый источник вместо захардкоженного номера.
    // Регрессия stale-60: ключ 61-й таблицы обязан быть известен
    QStringList actualTable=mytetraConfig.get_actual_parameter_table(true);
    QVERIFY2(actualTable.contains(QStringLiteral("attachViewMode")),
             "table must contain newest key");
    QVERIFY2(actualTable.contains(QStringLiteral("tetradir")),
             "table must contain old key");
    QVERIFY2(actualTable.contains(QStringLiteral("mcpAllow")),
             "table must contain mid key");
    QVERIFY2(actualTable.size()>=294,
             "table must hold 97 triples + signature; migration only grows");
}

void TestConf::tableBoundsSafe(void)
{
    // fix/537: helpers не должны падать на неизвестном имени и на
    // таблице без сигнатуры конца (раньше table.at() за границей)
    QStringList noSig;
    noSig << QStringLiteral("a") << QStringLiteral("int") << QStringLiteral("1")
          << QStringLiteral("b") << QStringLiteral("bool") << QStringLiteral("true");

    QCOMPARE(mytetraConfig.getParameterTypeFromTable(QStringLiteral("missing"), noSig),
             QString());
    QCOMPARE(mytetraConfig.getParameterDefaultValueFromTable(QStringLiteral("missing"), noSig),
             QString());
    QCOMPARE(mytetraConfig.getParameterTypeFromTable(QStringLiteral("b"), noSig),
             QStringLiteral("bool"));

    QStringList stripped=mytetraConfig.removeParameterFromTable(QStringLiteral("missing"), noSig);
    QCOMPARE(stripped.size(), noSig.size());

    QStringList replaced=mytetraConfig.replaceParameterInTable(QStringLiteral("missing"),
                                                               QStringLiteral("int"),
                                                               QStringLiteral("0"),
                                                               noSig);
    QCOMPARE(replaced.size(), noSig.size());
}

QTEST_MAIN(TestConf)

#include "test_conf.moc"

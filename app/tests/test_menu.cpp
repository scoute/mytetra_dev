// MenuBuilder на QTest (см. 526).
// Порт menuprobe: DI против QMenuBar, без MainWindow.
// buildAll с фейковыми тогглами, проверка меню/экшенов/сигналов.
#include <QtTest>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QFile>
#include <QTemporaryDir>

#include "views/mainWindow/MenuBuilder.h"
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
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/appConfig/AppConfig.h"

extern ShortcutManager shortcutManager;

extern AppConfig mytetraConfig;

class TestMenu : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void desktopBuild(void);
    void menuOrder(void);
    void actionsDesktop(void);
    void mobileBuild(void);
    void menuSignals(void);
    void retranslate(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;

    using M=MenuBuilder::MenuId;
    using A=MenuBuilder::ActionId;
};

void TestMenu::initTestCase(void)
{
    shortcutManager.retranslate();

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
    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_menu.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;
}

void TestMenu::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestMenu::desktopBuild(void)
{
    QMenuBar bar;
    MenuBuilder builder(&bar);

    QAction tagsToggle(QStringLiteral("Tags"), nullptr);
    QAction histToggle(QStringLiteral("Hist"), nullptr);
    QAction backToggle(QStringLiteral("Back"), nullptr);

    builder.buildAll(&tagsToggle, &histToggle, &backToggle, true);

    QVERIFY2(builder.menu(M::File)!=nullptr, "File menu must exist");
    QVERIFY2(builder.menu(M::Tools)!=nullptr, "Tools menu must exist");
    QVERIFY2(builder.menu(M::Themes)!=nullptr, "Themes menu must exist");
    QVERIFY2(builder.menu(M::View)!=nullptr, "View menu must exist");
    QVERIFY2(builder.menu(M::Help)!=nullptr, "Help menu must exist");
    QCOMPARE(bar.actions().size(), 5);
}

void TestMenu::menuOrder(void)
{
    QMenuBar bar;
    MenuBuilder builder(&bar);

    QAction tagsToggle(QStringLiteral("Tags"), nullptr);
    QAction histToggle(QStringLiteral("Hist"), nullptr);
    QAction backToggle(QStringLiteral("Back"), nullptr);

    builder.buildAll(&tagsToggle, &histToggle, &backToggle, true);

    // Порядок в баре: File, Tools, View, Themes, Help.
    // Справка последняя по конвенции, Вид между Tools и Themes
    const QList<QAction *> top=bar.actions();
    QCOMPARE(top.value(0)->menu(), builder.menu(M::File));
    QCOMPARE(top.value(1)->menu(), builder.menu(M::Tools));
    QCOMPARE(top.value(2)->menu(), builder.menu(M::View));
    QCOMPARE(top.value(3)->menu(), builder.menu(M::Themes));
    QCOMPARE(top.value(4)->menu(), builder.menu(M::Help));
}

void TestMenu::actionsDesktop(void)
{
    QMenuBar bar;
    MenuBuilder builder(&bar);

    QAction tagsToggle(QStringLiteral("Tags"), nullptr);
    QAction histToggle(QStringLiteral("Hist"), nullptr);
    QAction backToggle(QStringLiteral("Back"), nullptr);

    builder.buildAll(&tagsToggle, &histToggle, &backToggle, true);

    QVERIFY2(builder.action(A::FileQuit)!=nullptr, "quit must exist");
    QVERIFY2(builder.action(A::FilePrint)!=nullptr, "print must exist");
    QVERIFY2(builder.action(A::ToolsFindInBase)!=nullptr, "find must exist");
    QVERIFY2(builder.action(A::ToolsPreferences)!=nullptr, "prefs must exist on desktop");
    QVERIFY2(builder.action(A::DirectMain)==nullptr, "no direct action on desktop");
    QVERIFY2(builder.action(A::HelpTechnicalInfo)!=nullptr, "tech info must exist");
    QVERIFY2(builder.action(A::ThemeConfigure)!=nullptr, "theme configure must exist");
    QVERIFY2(builder.action(A::ViewHeader)!=nullptr, "view header must exist");
    QVERIFY2(builder.action(A::ViewReadOnly)!=nullptr, "readonly must exist");
    QVERIFY2(builder.action(A::ViewSecretColor)!=nullptr, "secret color must exist");
    QVERIFY2(builder.action(A::ViewHeader)->isCheckable(), "view header must be checkable");
}

void TestMenu::mobileBuild(void)
{
    QMenuBar bar;
    MenuBuilder builder(&bar);

    QAction tagsToggle(QStringLiteral("Tags"), nullptr);
    QAction histToggle(QStringLiteral("Hist"), nullptr);
    QAction backToggle(QStringLiteral("Back"), nullptr);

    // Мобайл: подменю вместо пункта
    builder.buildAll(&tagsToggle, &histToggle, &backToggle, false);
    QVERIFY2(builder.action(A::ToolsPreferences)==nullptr, "no prefs on mobile");
    QVERIFY2(builder.action(A::DirectMain)!=nullptr, "direct action must exist on mobile");
}

void TestMenu::menuSignals(void)
{
    QMenuBar bar;
    MenuBuilder builder(&bar);

    QAction tagsToggle(QStringLiteral("Tags"), nullptr);
    QAction histToggle(QStringLiteral("Hist"), nullptr);
    QAction backToggle(QStringLiteral("Back"), nullptr);

    builder.buildAll(&tagsToggle, &histToggle, &backToggle, true);

    // Сигналы вида/темы
    bool hdr=false, theme=false, shown=false, conf=false;
    connect(&builder, &MenuBuilder::viewHeaderToggled, [&](bool){ hdr=true; });
    connect(&builder, &MenuBuilder::themeChosen, [&](QAction*){ theme=true; });
    connect(&builder, &MenuBuilder::viewMenuShown, [&](){ shown=true; });
    connect(&builder, &MenuBuilder::themeConfigureRequested, [&](){ conf=true; });
    builder.action(A::ViewHeader)->toggle();
    QVERIFY2(hdr, "view header signal must fire");
    builder.action(A::ThemeConfigure)->trigger();
    QVERIFY2(conf, "theme configure signal must fire");
    builder.menu(M::View)->aboutToShow();
    QVERIFY2(shown, "view shown signal must fire");
    Q_UNUSED(theme);
}

void TestMenu::retranslate(void)
{
    QMenuBar bar;
    MenuBuilder builder(&bar);

    QAction tagsToggle(QStringLiteral("Tags"), nullptr);
    QAction histToggle(QStringLiteral("Hist"), nullptr);
    QAction backToggle(QStringLiteral("Back"), nullptr);

    builder.buildAll(&tagsToggle, &histToggle, &backToggle, true);

    // retranslate не падает, тексты не пустые
    builder.retranslate();
    QVERIFY2(!builder.action(A::FileQuit)->text().isEmpty(), "quit text must survive");
    QVERIFY2(!builder.menu(M::File)->title().isEmpty(), "file title must survive");
}

QTEST_MAIN(TestMenu)

#include "test_menu.moc"

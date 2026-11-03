// MenuBuilder стенд: DI против QMenuBar, без MainWindow.
// buildAll с фейковыми тогглами, проверка меню/экшенов/сигналов.
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QSettings>
#include <QFile>

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
#include "libraries/ShortcutManager.h"

extern ShortcutManager shortcutManager;

extern AppConfig mytetraConfig;

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

    shortcutManager.retranslate();

    // P1: MenuBuilder ходит через AppContext - контекст как в main()
    static FixedParameters fp;
    static GlobalParameters gp;
    static AppFiles af;
    static DataBaseConfig dbc;
    static TrashMonitoring tm;
    static WalkHistory wh;
    static VisitHistory vh;
    static ActionLogger al;
    static PeriodicCheckBase pcb;
    static PeriodicSyncro ps;
    static InternalClipboard ic;
    AppContext::instance().init(&fp, &gp, &mytetraConfig, &af, &dbc, &tm,
                                &wh, &vh, &al, &shortcutManager, &pcb, &ps,
                                &ic, nullptr, &gp.cryptHolder());

    QFile::remove(QStringLiteral("/tmp/opencode/probe3.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe3.ini"),
                                       QSettings::IniFormat);

    QMenuBar bar;
    MenuBuilder builder(&bar);

    QAction tagsToggle(QStringLiteral("Tags"), nullptr);
    QAction histToggle(QStringLiteral("Hist"), nullptr);
    QAction backToggle(QStringLiteral("Back"), nullptr);

    builder.buildAll(&tagsToggle, &histToggle, &backToggle, true);

    using M=MenuBuilder::MenuId;
    using A=MenuBuilder::ActionId;
    check(builder.menu(M::File)!=nullptr, "MENU-FILE");
    check(builder.menu(M::Tools)!=nullptr, "MENU-TOOLS");
    check(builder.menu(M::Themes)!=nullptr, "MENU-THEMES");
    check(builder.menu(M::View)!=nullptr, "MENU-VIEW");
    check(builder.menu(M::Help)!=nullptr, "MENU-HELP");
    check(bar.actions().size()==5, "MENUBAR-COUNT");

    // Порядок в баре: File, Tools, View, Themes, Help.
    // Справка последняя по конвенции, Вид между Tools и Themes
    {
        const QList<QAction *> top=bar.actions();
        check(top.value(0)->menu()==builder.menu(M::File), "ORDER-FILE");
        check(top.value(1)->menu()==builder.menu(M::Tools), "ORDER-TOOLS");
        check(top.value(2)->menu()==builder.menu(M::View), "ORDER-VIEW");
        check(top.value(3)->menu()==builder.menu(M::Themes), "ORDER-THEMES");
        check(top.value(4)->menu()==builder.menu(M::Help), "ORDER-HELP-LAST");
    }

    check(builder.action(A::FileQuit)!=nullptr, "ACT-QUIT");
    check(builder.action(A::FilePrint)!=nullptr, "ACT-PRINT");
    check(builder.action(A::ToolsFindInBase)!=nullptr, "ACT-FIND");
    check(builder.action(A::ToolsPreferences)!=nullptr, "ACT-PREFS-DESKTOP");
    check(builder.action(A::DirectMain)==nullptr, "ACT-NODIRECT-DESKTOP");
    check(builder.action(A::HelpTechnicalInfo)!=nullptr, "ACT-TECH");
    check(builder.action(A::ThemeConfigure)!=nullptr, "ACT-THEMECONF");
    check(builder.action(A::ViewHeader)!=nullptr, "ACT-VIEWHEADER");
    check(builder.action(A::ViewReadOnly)!=nullptr, "ACT-READONLY");
    check(builder.action(A::ViewSecretColor)!=nullptr, "ACT-SECRETCOLOR");
    check(builder.action(A::ViewHeader)->isCheckable(), "ACT-CHECKABLE");

    // Мобайл: подменю вместо пункта
    QMenuBar bar2;
    MenuBuilder builder2(&bar2);
    builder2.buildAll(&tagsToggle, &histToggle, &backToggle, false);
    check(builder2.action(A::ToolsPreferences)==nullptr, "MOBILE-NOPREFS");
    check(builder2.action(A::DirectMain)!=nullptr, "MOBILE-DIRECT");

    // Сигналы вида/темы
    bool hdr=false, theme=false, shown=false, conf=false;
    QObject::connect(&builder, &MenuBuilder::viewHeaderToggled, [&](bool){ hdr=true; });
    QObject::connect(&builder, &MenuBuilder::themeChosen, [&](QAction*){ theme=true; });
    QObject::connect(&builder, &MenuBuilder::viewMenuShown, [&](){ shown=true; });
    QObject::connect(&builder, &MenuBuilder::themeConfigureRequested, [&](){ conf=true; });
    builder.action(A::ViewHeader)->toggle();
    check(hdr, "SIG-VIEWHEADER");
    builder.action(A::ThemeConfigure)->trigger();
    check(conf, "SIG-THEMECONF");
    builder.menu(M::View)->aboutToShow();
    check(shown, "SIG-VIEWSHOWN");

    // retranslate не падает, тексты не пустые
    builder.retranslate();
    check(!builder.action(A::FileQuit)->text().isEmpty(), "RETR-TEXT");
    check(!builder.menu(M::File)->title().isEmpty(), "RETR-TITLE");

    qDebug() << (fails==0 ? "MENUPROBE-OK" : "MENUPROBE-BAD");
    return fails;
}

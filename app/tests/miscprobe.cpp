// Misc settings page probe: stats group texts must be set
// at construction (retranslateUi in ctor, Theme/Voice pattern).
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QLabel>
#include <QRadioButton>
#include <QGroupBox>

#include "views/appConfigWindow/AppConfigPage_Misc.h"
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

static int fails=0;

static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;

    if(!ok)
        ++fails;
}

// Доступ к protected-членам страницы без макросов:
// наследник видит protected родителя
class MiscProbePage : public AppConfigPage_Misc
{
public:

    explicit MiscProbePage(QWidget *parent=nullptr) : AppConfigPage_Misc(parent) {}

    QString boxTitle(void) const { return statsBox->title(); }
    QString askText(void) const { return statsAsk->text(); }
    QString alwaysText(void) const { return statsAlways->text(); }
    QString neverText(void) const { return statsNever->text(); }
    QString limitText(void) const { return statsLimitLabel->text(); }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

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

    // Полный конфиг из фикстуры: странице нужны все секции.
    // Копия: AppConfig может дописать миграцию обратно
    QFile::remove(QStringLiteral("/tmp/opencode/probe_misc.ini"));
    QFile::copy(QStringLiteral("/tmp/opencode/bktinystand/conf.ini"),
                QStringLiteral("/tmp/opencode/probe_misc.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe_misc.ini"),
                                       QSettings::IniFormat);

    MiscProbePage page(nullptr);

    check(!page.boxTitle().isEmpty(), "MISC-BOX-TITLE");
    check(!page.askText().isEmpty(), "MISC-ASK-TEXT");
    check(!page.alwaysText().isEmpty(), "MISC-ALWAYS-TEXT");
    check(!page.neverText().isEmpty(), "MISC-NEVER-TEXT");
    check(!page.limitText().isEmpty(), "MISC-LIMIT-TEXT");

    if(fails==0)
        qDebug() << "PROBE-OK misc";

    return 0;
}

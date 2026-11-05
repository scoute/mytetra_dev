// Search settings page probe: FTS group texts must be set
// at construction (setupUi sets texts directly, see 504).
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

#include "views/appConfigWindow/AppConfigPage_Search.h"
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

class SearchProbePage : public AppConfigPage_Search
{
public:

    explicit SearchProbePage(QWidget *parent=nullptr) : AppConfigPage_Search(parent) {}

    QString boxTitle(void) const { return ftsIndexBox->title(); }
    QString buttonText(void) const { return ftsVacuumButton->text(); }
    QString sizeText(void) const { return ftsIndexSizeLabel->text(); }
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

    QFile::remove(QStringLiteral("/tmp/opencode/probe_search.ini"));
    QFile::copy(QStringLiteral("/tmp/opencode/bktinystand/conf.ini"),
                QStringLiteral("/tmp/opencode/probe_search.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe_search.ini"),
                                       QSettings::IniFormat);

    SearchProbePage page(nullptr);

    check(!page.boxTitle().isEmpty(), "SEARCH-BOX-TITLE");
    check(!page.buttonText().isEmpty(), "SEARCH-VACUUM-TEXT");
    check(!page.sizeText().isEmpty(), "SEARCH-SIZE-TEXT");

    if(fails==0)
        qDebug() << "PROBE-OK search";

    return 0;
}

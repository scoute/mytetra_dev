// EditorConfig: отсутствующий или пустой известный параметр
// подменяется дефолтом из актуальной таблицы вместо падения (см. 512).
// Неизвестные имена по-прежнему роняют программу и здесь не гоняются.
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QDir>

#include "libraries/wyedit/EditorConfig.h"
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

// Дефолт ключа из актуальной таблицы (тройки имя/тип/значение)
static QString tableDefault(const QString &key)
{
    QStringList table=EditorConfig::get_parameter_table_47(false);

    for(int i=0; i<table.size()/3; ++i)
        if(table.at(i*3)==key)
            return table.at(i*3+2);

    return QStringLiteral("<no-such-key>");
}

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

    QFile::remove(QStringLiteral("/tmp/opencode/probe_editor.ini"));
    QFile::copy(QStringLiteral("/tmp/opencode/bktinystand/editorconf.ini"),
                QStringLiteral("/tmp/opencode/probe_editor.ini"));

    // Известный ключ с пустым значением: раньше criticalError, теперь дефолт
    {
        QSettings fix(QStringLiteral("/tmp/opencode/probe_editor.ini"), QSettings::IniFormat);
        fix.setValue(QStringLiteral("default_font"), QString());
        fix.sync();
    }

    EditorConfig ed(QStringLiteral("/tmp/opencode/probe_editor.ini"));

    const QString expectedDefault=tableDefault(QStringLiteral("default_font"));
    check(!expectedDefault.isEmpty(), "EDCONF-DEFAULT-KNOWN");
    check(ed.get_default_font()==expectedDefault, "EDCONF-EMPTY-FALLBACK");
    check(ed.get_configFallbackCount()==1, "EDCONF-COUNT-ONE");

    // Удаленный после загрузки ключ: то же поведение
    ed.settingsHandle()->remove(QStringLiteral("default_font"));
    ed.settingsHandle()->sync();
    check(ed.get_default_font()==expectedDefault, "EDCONF-MISSING-FALLBACK");
    check(ed.get_configFallbackCount()==2, "EDCONF-COUNT-TWO");
    check(ed.get_configFallbackHits().value(QStringLiteral("default_font"))==2, "EDCONF-HITS");

    if(fails==0)
        qDebug() << "PROBE-OK editorconf";

    return 0;
}

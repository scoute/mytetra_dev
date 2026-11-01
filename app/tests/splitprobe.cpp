// Сплит редактор+превью: двусторонняя синхронизация MetaEditor.
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QSettings>
#include <QTimer>
#include <QEventLoop>

#include "views/record/MetaEditor.h"
#include "libraries/wyedit/HtmlSourceEdit.h"
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

    QFile::remove(QStringLiteral("/tmp/opencode/probesplit.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probesplit.ini"),
                                       QSettings::IniFormat);

    globalParameters.setMainProgramFile(QString::fromLocal8Bit(argv[0]));

    if(!globalParameters.initHeadless())
    {
        qDebug() << QStringLiteral("NOWORKDIR");
        return 1;
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

    mytetraConfig.init();
    shortcutManager.init();

    QWidget win;
    win.show();

    MetaEditor *ed=new MetaEditor(&win);
    ed->show();
    QApplication::processEvents();
    check(ed!=nullptr, "SPLIT-CREATE");

    HtmlSourceEdit *pane=ed->findChild<HtmlSourceEdit *>();
    check(pane!=nullptr, "SPLIT-PANE-EXISTS");
    check(!ed->isHtmlSourceVisible(), "SPLIT-HIDDEN-DEFAULT");

    ed->setTextarea(QStringLiteral("<p>hello</p>"));
    ed->setHtmlSourceVisible(true);
    check(ed->isHtmlSourceVisible(), "SPLIT-SHOWN");
    check(pane->toPlainText().contains(QStringLiteral("hello")), "SPLIT-NOTE-TO-SOURCE");

    // Правка заметки долетает в панель (прямой вызов слота, без ожидания дебаунса)
    ed->setTextarea(QStringLiteral("<p>second</p>"));
    ed->syncNoteToSource();
    check(pane->toPlainText().contains(QStringLiteral("second")), "SPLIT-NOTE-UPDATE");

    // Правка панели долетает в заметку
    pane->setPlainText(QStringLiteral("<p>frompane</p>"));
    ed->syncSourceToNote();
    check(ed->getTextareaStable().contains(QStringLiteral("frompane")), "SPLIT-SOURCE-TO-NOTE");
    check(ed->getTextareaModified(), "SPLIT-MODIFIED");

    // Холостой прогон: одинаковый текст ничего не ломает
    ed->syncSourceToNote();
    ed->syncNoteToSource();
    check(ed->getTextareaStable().contains(QStringLiteral("frompane")), "SPLIT-IDEMPOTENT");

    ed->setHtmlSourceVisible(false);
    check(!ed->isHtmlSourceVisible(), "SPLIT-HIDE");

    qDebug() << (fails==0 ? "SPLITPROBE-OK" : "SPLITPROBE-BAD");
    return fails;
}

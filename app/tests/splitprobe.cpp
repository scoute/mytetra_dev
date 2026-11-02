// Сплит редактор+превью: живой MetaEditor, двусторонняя синхронизация
// исходника и зеркало курсора. Нужна рабочая директория (conf.ini +
// editorconf.ini) и запуск бинаря из нее: раннер стейджит фикстуру
// из штатных конфигов, см. run_probes.sh
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QSettings>
#include <QTextCursor>
#include <QTextBlock>

#include "views/record/MetaEditor.h"
#include "libraries/wyedit/HtmlSourceEdit.h"
#include "libraries/wyedit/EditorTextArea.h"
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

    globalParameters.setMainProgramFile(QString::fromLocal8Bit(argv[0]));

    if(!globalParameters.initHeadless())
    {
        qDebug() << QStringLiteral("NOWORKDIR");
        return 1;
    }

    QFile::remove(QStringLiteral("/tmp/opencode/probesplit.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probesplit.ini"),
                                       QSettings::IniFormat);

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

    // Зеркало курсора: блок заметки -> позиция сниппета в исходнике
    ed->setTextarea(QStringLiteral("<p>one</p>\n<p>two</p>\n<p>three</p>"));
    ed->syncNoteToSource();
    {
        QTextCursor c(ed->textArea->document());
        c.setPosition(ed->textArea->document()->findBlockByNumber(2).position());
        ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    check(pane->toPlainText().mid(pane->textCursor().position(), 5)==QStringLiteral("three"),
          "CURSOR-NOTE-TO-SOURCE");

    // Обратно: строка исходника -> блок заметки
    {
        const int at=pane->toPlainText().indexOf(QStringLiteral("two"));
        QTextCursor c(pane->document());
        c.setPosition(at);
        pane->setTextCursor(c);
        QApplication::processEvents();
    }
    check(ed->textArea->textCursor().blockNumber()==1, "CURSOR-SOURCE-TO-NOTE");

    // Зеркало выделения: диапазон заметки -> диапазон исходника
    ed->setTextarea(QStringLiteral("<p>one</p>\n<p>two</p>\n<p>three</p>"));
    ed->syncNoteToSource();
    {
        QTextCursor c(ed->textArea->document());
        const int start=ed->textArea->document()->toPlainText().indexOf(QStringLiteral("two"));
        c.setPosition(start);
        c.setPosition(start+3, QTextCursor::KeepAnchor);
        ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    check(pane->textCursor().hasSelection(), "SEL-NOTE-HAS");
    check(pane->textCursor().selectedText().contains(QStringLiteral("two")), "SEL-NOTE-TO-SOURCE");

    // Обратно: диапазон исходника -> диапазон заметки
    {
        const int at=pane->toPlainText().indexOf(QStringLiteral("three"));
        QTextCursor c(pane->document());
        c.setPosition(at);
        c.setPosition(at+5, QTextCursor::KeepAnchor);
        pane->setTextCursor(c);
        QApplication::processEvents();
    }
    check(ed->textArea->textCursor().hasSelection(), "SEL-SOURCE-HAS");
    check(ed->textArea->textCursor().selectedText().contains(QStringLiteral("three")), "SEL-SOURCE-TO-NOTE");

    ed->setHtmlSourceVisible(false);
    check(!ed->isHtmlSourceVisible(), "SPLIT-HIDE");

    qDebug() << (fails==0 ? "SPLITPROBE-OK" : "SPLITPROBE-BAD");
    return fails;
}

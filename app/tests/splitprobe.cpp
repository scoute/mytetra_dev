// Сплит редактор+превью: живой MetaEditor, двусторонняя синхронизация
// исходника и зеркало курсора. Нужна рабочая директория (conf.ini +
// editorconf.ini) и запуск бинаря из нее: раннер стейджит фикстуру
// из штатных конфигов, см. run_probes.sh
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QSettings>
#include <QTextCursor>
#include <QPoint>
#include "libraries/wyedit/EditorIndentSliderAssistant.h"
#include "libraries/wyedit/indentslider/IndentSlider.h"
#include <QTextBlock>
#include <QSplitter>
#include <QDialog>

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

    // Слово из середины строки: якоря first/last указывают на то же
    // вхождение, начало последнего лежит раньше старта первого.
    // Без запаса назад поиск last его пропускал и диапазон не строился
    ed->setTextarea(QStringLiteral("<p>This program release uder GPL v.3 and BSD licenses.</p>"));
    ed->syncNoteToSource();
    {
        const int at=ed->textArea->document()->toPlainText().indexOf(QStringLiteral("program"));
        QTextCursor c(ed->textArea->document());
        c.setPosition(at);
        c.setPosition(at+7, QTextCursor::KeepAnchor);
        ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    check(pane->textCursor().hasSelection(), "SEL-MID-HAS");
    check(pane->textCursor().selectedText().contains(QStringLiteral("program")), "SEL-MID-TO-SOURCE");

    // Обратно из середины строки исходника
    {
        const int at=pane->toPlainText().indexOf(QStringLiteral("release"));
        QTextCursor c(pane->document());
        c.setPosition(at);
        c.setPosition(at+7, QTextCursor::KeepAnchor);
        pane->setTextCursor(c);
        QApplication::processEvents();
    }
    check(ed->textArea->textCursor().hasSelection(), "SEL-MID-SOURCE-HAS");
    check(ed->textArea->textCursor().selectedText().contains(QStringLiteral("release")), "SEL-MID-SOURCE-TO-NOTE");

    // URL документа переживает применение исходника: иначе
    // относительные картинки гасли бы до перезахода в заметку
    ed->textArea->document()->setMetaInformation(QTextDocument::DocumentUrl,
                                                 QStringLiteral("file:/tmp/opencode/"));
    pane->setPlainText(QStringLiteral("<p>urlcheck</p>"));
    ed->syncSourceToNote();
    check(ed->textArea->document()->metaInformation(QTextDocument::DocumentUrl)
          ==QStringLiteral("file:/tmp/opencode/"), "SPLIT-URL-KEPT");
    check(ed->getTextareaStable().contains(QStringLiteral("urlcheck")), "SPLIT-URL-APPLIED");

    // Небрежное выделение с пробелами по краям все равно зеркалится
    // (в исходнике отступы - &nbsp;, точного совпадения не будет)
    ed->setTextarea(QStringLiteral("<p>    indented line here</p>"));
    ed->syncNoteToSource();
    {
        const int at=ed->textArea->document()->toPlainText().indexOf(QStringLiteral("indented"));
        QTextCursor c(ed->textArea->document());
        c.setPosition(at-2);
        c.setPosition(at+8, QTextCursor::KeepAnchor);
        ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    check(pane->textCursor().selectedText().contains(QStringLiteral("indented")),
          "SEL-TRIMMED");

    // Дубли: якорь неоднозначен - вместо неверного диапазона
    // только курсор (выделения в чужой панели нет)
    ed->setTextarea(QStringLiteral("<p>aaa</p>\n<p>aaa</p>"));
    ed->syncNoteToSource();
    {
        QTextBlock second=ed->textArea->document()->findBlockByNumber(1);
        QTextCursor c(ed->textArea->document());
        c.setPosition(second.position());
        c.setPosition(second.position()+3, QTextCursor::KeepAnchor);
        ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    check(!pane->textCursor().hasSelection(), "SEL-AMBIGUOUS-NO-RANGE");

    // Размещение исходника: низ, слева, справа, окно, скрыть
    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Bottom);
    check(ed->htmlSourcePane->isVisible(), "PLACE-BOTTOM-VISIBLE");
    check(ed->sourceSplitter->orientation()==Qt::Vertical, "PLACE-BOTTOM-VERT");
    check(ed->sourceSplitter->indexOf(ed->htmlSourcePane)==1, "PLACE-BOTTOM-ORDER");

    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Left);
    check(ed->sourceSplitter->orientation()==Qt::Horizontal, "PLACE-LEFT-HORZ");
    check(ed->sourceSplitter->indexOf(ed->htmlSourcePane)==0, "PLACE-LEFT-ORDER");
    check(ed->sourceSplitter->indexOf(ed->textArea)==1, "PLACE-LEFT-TEXT");

    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Right);
    check(ed->sourceSplitter->orientation()==Qt::Horizontal, "PLACE-RIGHT-HORZ");
    check(ed->sourceSplitter->indexOf(ed->textArea)==0, "PLACE-RIGHT-TEXT");
    check(ed->sourceSplitter->indexOf(ed->htmlSourcePane)==1, "PLACE-RIGHT-ORDER");

    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Window);
    check(ed->htmlSourceWindow!=nullptr && ed->htmlSourceWindow->isVisible(), "PLACE-WINDOW-SHOWN");
    check(ed->htmlSourcePane->parent()==ed->htmlWindowContainer, "PLACE-WINDOW-MOVED");

    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Hidden);
    check(!ed->htmlSourcePane->isVisible(), "PLACE-HIDDEN");

    ed->toggleHtmlSource();
    check(ed->htmlSourcePlacement()==MetaEditor::HtmlPlacement::Right, "PLACE-TOGGLE-RESTORES");
    check(ed->htmlSourcePane->isVisible(), "PLACE-TOGGLE-VISIBLE");
    ed->toggleHtmlSource();
    check(ed->htmlSourcePlacement()==MetaEditor::HtmlPlacement::Hidden, "PLACE-TOGGLE-HIDES");

    // Линейка отступов всегда над текстом: координаты шкалы
    // сдвигаются на смещение текста относительно линейки
    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Left);
    QApplication::processEvents();
    QApplication::processEvents();
    {
        IndentSlider *slider=qobject_cast<IndentSlider *>(
          ed->indentSliderAssistant->getIndentSlider());
        check(slider!=nullptr, "PLACE-SLIDER-EXISTS");
        // Свежий документ без отступов: маркеры стоят ровно на краях
        // текста, абсолютные координаты сверяются со сдвигом
        const int shift=ed->textArea->mapTo(slider->parentWidget(), QPoint(0, 0)).x()-
                       slider->x();
        check(slider->get_geometry_leftindent_pos()==
              ed->textArea->getIndentStartedLeft()+shift,
              "PLACE-SLIDER-SHIFT-LEFT");
        check(slider->get_geometry_rightindent_pos()==
              ed->textArea->getIndentStartedRight()+shift,
              "PLACE-SLIDER-SHIFT-LEFT-R");
    }

    // Исходник всегда уже текста: отложенные размеры успели встать
    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Left);
    QApplication::processEvents();
    QApplication::processEvents();
    {
        const QList<int> sizes=ed->sourceSplitter->sizes();
        check(sizes.size()==2 && sizes.at(0)>0 &&
              sizes.at(ed->sourceSplitter->indexOf(ed->htmlSourcePane))<=
              sizes.at(ed->sourceSplitter->indexOf(ed->textArea)),
              "PLACE-LEFT-NARROWER");
    }

    ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Right);
    QApplication::processEvents();
    QApplication::processEvents();
    {
        const QList<int> sizes=ed->sourceSplitter->sizes();
        check(sizes.size()==2 && sizes.at(1)>0 &&
              sizes.at(ed->sourceSplitter->indexOf(ed->htmlSourcePane))<=
              sizes.at(ed->sourceSplitter->indexOf(ed->textArea)),
              "PLACE-RIGHT-NARROWER");
    }

    ed->setHtmlSourceVisible(false);
    check(!ed->isHtmlSourceVisible(), "SPLIT-HIDE");

    qDebug() << (fails==0 ? "SPLITPROBE-OK" : "SPLITPROBE-BAD");
    return fails;
}

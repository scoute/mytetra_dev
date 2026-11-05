// Сплит редактор+превью на QTest (см. 533).
// Порт splitprobe: живой MetaEditor, двусторонняя синхронизация
// исходника и зеркало курсора. Нужна рабочая директория (conf.ini +
// editorconf.ini) и запуск бинаря из нее: раннер стейджит фикстуры
// из репо (app/tests/data), см. run_qtests.sh.
#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QSettings>
#include <QTextCursor>
#include <QPoint>
#include <QTextBlock>
#include <QSplitter>
#include <QDialog>
#include <QTemporaryDir>

#include "libraries/wyedit/EditorIndentSliderAssistant.h"
#include "libraries/wyedit/indentslider/IndentSlider.h"
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
extern ShortcutManager shortcutManager;

class TestSplit : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void createSync(void);
    void cursorMirror(void);
    void tagTextMirror(void);
    void placement(void);
    void fold(void);
    void normalize(void);
    void sliderSizes(void);

private:

    QTemporaryDir m_tmp;

    struct EditorKit
    {
        QWidget *win=nullptr;
        MetaEditor *ed=nullptr;
        HtmlSourceEdit *pane=nullptr;
    };

    EditorKit makeEditor(void);
    static void dropEditor(EditorKit &kit);
};

void TestSplit::initTestCase(void)
{
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
    gp->setMainProgramFile(QCoreApplication::applicationFilePath());
    QVERIFY2(gp->initHeadless(), "headless workdir must init");

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
    mytetraConfig.m_conf=new QSettings(m_tmp.filePath(QStringLiteral("test_split.ini")),
                                       QSettings::IniFormat, this);

    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                nullptr, nullptr, &gp->cryptHolder());

    mytetraConfig.init();
    shortcutManager.init();
}

void TestSplit::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

TestSplit::EditorKit TestSplit::makeEditor(void)
{
    EditorKit kit;
    kit.win=new QWidget;
    kit.win->show();
    kit.ed=new MetaEditor(kit.win);
    kit.ed->show();
    QApplication::processEvents();
    kit.pane=kit.ed->findChild<HtmlSourceEdit *>();
    return kit;
}

void TestSplit::dropEditor(EditorKit &kit)
{
    delete kit.win;
    kit.win=nullptr;
    kit.ed=nullptr;
    kit.pane=nullptr;
}

void TestSplit::createSync(void)
{
    EditorKit k=makeEditor();
    QVERIFY2(k.ed!=nullptr, "editor must be created");
    QVERIFY2(k.pane!=nullptr, "source pane must exist");
    QVERIFY2(!k.ed->isHtmlSourceVisible(), "source must be hidden by default");

    k.ed->setTextarea(QStringLiteral("<p>hello</p>"));
    k.ed->setHtmlSourceVisible(true);
    QVERIFY2(k.ed->isHtmlSourceVisible(), "source must be shown");
    QVERIFY2(k.pane->toPlainText().contains(QStringLiteral("hello")),
             "note must reach source");

    // Правка заметки долетает в панель (прямой вызов слота, без ожидания дебаунса)
    k.ed->setTextarea(QStringLiteral("<p>second</p>"));
    k.ed->syncNoteToSource();
    QVERIFY2(k.pane->toPlainText().contains(QStringLiteral("second")),
             "note update must reach source");

    // Правка панели долетает в заметку
    k.pane->setPlainText(QStringLiteral("<p>frompane</p>"));
    k.ed->syncSourceToNote();
    QVERIFY2(k.ed->getTextareaStable().contains(QStringLiteral("frompane")),
             "source must reach note");
    QVERIFY2(k.ed->getTextareaModified(), "note must be modified");

    // Холостой прогон: одинаковый текст ничего не ломает
    k.ed->syncSourceToNote();
    k.ed->syncNoteToSource();
    QVERIFY2(k.ed->getTextareaStable().contains(QStringLiteral("frompane")),
             "idle sync must be harmless");

    dropEditor(k);
}

void TestSplit::cursorMirror(void)
{
    EditorKit k=makeEditor();
    k.ed->setHtmlSourceVisible(true);

    // Зеркало курсора: блок заметки -> позиция сниппета в исходнике
    k.ed->setTextarea(QStringLiteral("<p>one</p>\n<p>two</p>\n<p>three</p>"));
    k.ed->syncNoteToSource();
    {
        QTextCursor c(k.ed->textArea->document());
        c.setPosition(k.ed->textArea->document()->findBlockByNumber(2).position());
        k.ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    QCOMPARE(k.pane->toPlainText().mid(k.pane->textCursor().position(), 5),
             QStringLiteral("three"));

    // Обратно: строка исходника -> блок заметки
    {
        const int at=k.pane->toPlainText().indexOf(QStringLiteral("two"));
        QTextCursor c(k.pane->document());
        c.setPosition(at);
        k.pane->setTextCursor(c);
        QApplication::processEvents();
    }
    QCOMPARE(k.ed->textArea->textCursor().blockNumber(), 1);

    // Зеркало выделения: диапазон заметки -> диапазон исходника
    k.ed->setTextarea(QStringLiteral("<p>one</p>\n<p>two</p>\n<p>three</p>"));
    k.ed->syncNoteToSource();
    {
        QTextCursor c(k.ed->textArea->document());
        const int start=k.ed->textArea->document()->toPlainText().indexOf(QStringLiteral("two"));
        c.setPosition(start);
        c.setPosition(start+3, QTextCursor::KeepAnchor);
        k.ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(k.pane->textCursor().hasSelection(), "source must select");
    QVERIFY2(k.pane->textCursor().selectedText().contains(QStringLiteral("two")),
             "selection must mirror to source");

    // Обратно: диапазон исходника -> диапазон заметки
    {
        const int at=k.pane->toPlainText().indexOf(QStringLiteral("three"));
        QTextCursor c(k.pane->document());
        c.setPosition(at);
        c.setPosition(at+5, QTextCursor::KeepAnchor);
        k.pane->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(k.ed->textArea->textCursor().hasSelection(), "note must select");
    QVERIFY2(k.ed->textArea->textCursor().selectedText().contains(QStringLiteral("three")),
             "selection must mirror to note");

    // Слово из середины строки: якоря first/last указывают на то же
    // вхождение, начало последнего лежит раньше старта первого.
    // Без запаса назад поиск last его пропускал и диапазон не строился
    k.ed->setTextarea(QStringLiteral("<p>This program release uder GPL v.3 and BSD licenses.</p>"));
    k.ed->syncNoteToSource();
    {
        const int at=k.ed->textArea->document()->toPlainText().indexOf(QStringLiteral("program"));
        QTextCursor c(k.ed->textArea->document());
        c.setPosition(at);
        c.setPosition(at+7, QTextCursor::KeepAnchor);
        k.ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(k.pane->textCursor().hasSelection(), "mid-word must select");
    QVERIFY2(k.pane->textCursor().selectedText().contains(QStringLiteral("program")),
             "mid-word must mirror");

    // Обратно из середины строки исходника
    {
        const int at=k.pane->toPlainText().indexOf(QStringLiteral("release"));
        QTextCursor c(k.pane->document());
        c.setPosition(at);
        c.setPosition(at+7, QTextCursor::KeepAnchor);
        k.pane->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(k.ed->textArea->textCursor().hasSelection(), "mid-word back must select");
    QVERIFY2(k.ed->textArea->textCursor().selectedText().contains(QStringLiteral("release")),
             "mid-word back must mirror");

    // URL документа переживает применение исходника: иначе
    // относительные картинки гасли бы до перезахода в заметку
    k.ed->textArea->document()->setMetaInformation(QTextDocument::DocumentUrl,
                                                   QStringLiteral("file:/tmp/opencode/"));
    k.pane->setPlainText(QStringLiteral("<p>urlcheck</p>"));
    k.ed->syncSourceToNote();
    QCOMPARE(k.ed->textArea->document()->metaInformation(QTextDocument::DocumentUrl),
             QStringLiteral("file:/tmp/opencode/"));
    QVERIFY2(k.ed->getTextareaStable().contains(QStringLiteral("urlcheck")),
             "url must survive apply");

    // Небрежное выделение с пробелами по краям все равно зеркалится
    k.ed->setTextarea(QStringLiteral("<p>    indented line here</p>"));
    k.ed->syncNoteToSource();
    {
        const int at=k.ed->textArea->document()->toPlainText().indexOf(QStringLiteral("indented"));
        QTextCursor c(k.ed->textArea->document());
        c.setPosition(at-2);
        c.setPosition(at+8, QTextCursor::KeepAnchor);
        k.ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(k.pane->textCursor().selectedText().contains(QStringLiteral("indented")),
             "sloppy selection must mirror");

    // Дубли: якорь неоднозначен - вместо неверного диапазона
    // только курсор (выделения в чужой панели нет)
    k.ed->setTextarea(QStringLiteral("<p>aaa</p>\n<p>aaa</p>"));
    k.ed->syncNoteToSource();
    {
        QTextBlock second=k.ed->textArea->document()->findBlockByNumber(1);
        QTextCursor c(k.ed->textArea->document());
        c.setPosition(second.position());
        c.setPosition(second.position()+3, QTextCursor::KeepAnchor);
        k.ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(!k.pane->textCursor().hasSelection(), "ambiguous anchor must not select");

    dropEditor(k);
}

void TestSplit::tagTextMirror(void)
{
    EditorKit k=makeEditor();
    k.ed->setHtmlSourceVisible(true);

    // Теги не якоря: текст "H6" не путается с тегом <h6> (поиск Qt
    // по умолчанию регистронезависим), диапазон строится по тексту,
    // а не встает курсором на тег
    k.ed->setTextarea(QStringLiteral("<h1>H1</h1>\n<h6>H6</h6>\n<p style=\"margin-top:5px;\">Plain text</p>"));
    k.ed->syncNoteToSource();
    {
        QTextCursor c(k.ed->textArea->document()->find(QStringLiteral("H6")));
        k.ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(k.pane->textCursor().hasSelection() &&
             k.pane->textCursor().selectedText()==QStringLiteral("H6"),
             "tag text must select");

    // Слово из атрибутов (margin) зеркалится в текст, который тег
    // оборачивает, а не теряется
    {
        const int at=k.pane->toPlainText().indexOf(QStringLiteral("margin"));
        QTextCursor c(k.pane->document());
        c.setPosition(at);
        c.setPosition(at+6, QTextCursor::KeepAnchor);
        k.pane->setTextCursor(c);
        QApplication::processEvents();
    }
    QVERIFY2(k.ed->textArea->textCursor().selectedText().contains(QStringLiteral("Plain text")),
             "attribute word must mirror to wrapped text");

    // Реальный документ: пустые абзацы дают в шапке правило p.mtempty
    // с "margin", а текст абзаца тоже "margin".
    // Слово margin в тексте не должно теряться из-за margin в CSS
    k.ed->setTextarea(QStringLiteral(
        "<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px;\"><br /></p>"
        "<p style=\"margin-top:0px;margin-bottom:0px;\">margin</p>"));
    k.ed->syncNoteToSource();
    QVERIFY2(k.pane->toPlainText().contains(QStringLiteral("mtempty")),
             "mtempty rule must be present");
    {
        const int at=k.pane->toPlainText().indexOf(QStringLiteral(">margin<"));
        QVERIFY2(at>=0, "margin text must be present");
        // Сбросить курсор заметки: иначе выбор с прошлого шага мог
        // остаться и создать ложное впечатление работы зеркала
        QTextCursor zeroNote(k.ed->textArea->document());
        zeroNote.setPosition(0);
        k.ed->textArea->setTextCursor(zeroNote);
        QApplication::processEvents();
        QTextCursor c(k.pane->document());
        c.setPosition(at+1);
        c.setPosition(at+7, QTextCursor::KeepAnchor);
        k.pane->setTextCursor(c);
        QApplication::processEvents();
    }
    QCOMPARE(k.ed->textArea->textCursor().selectedText(), QStringLiteral("margin"));
    // Сбросить курсор исходника
    {
        QTextCursor zero(k.pane->document());
        zero.setPosition(0);
        k.pane->setTextCursor(zero);
        QApplication::processEvents();
    }
    {
        QTextCursor c(k.ed->textArea->document()->find(QStringLiteral("margin")));
        k.ed->textArea->setTextCursor(c);
        QApplication::processEvents();
    }
    // Выбран должен быть текст абзаца (margin</p>), а не margin из
    // правила p.mtempty в шапке (margin-top и т.п.)
    QVERIFY2(k.pane->textCursor().selectedText()==QStringLiteral("margin") &&
             k.pane->toPlainText().mid(k.pane->textCursor().selectionEnd(), 4)==QStringLiteral("</p>"),
             "paragraph margin must win over CSS margin");

    dropEditor(k);
}

void TestSplit::placement(void)
{
    EditorKit k=makeEditor();
    k.ed->setHtmlSourceVisible(true);

    // Размещение исходника: низ, слева, справа, окно, скрыть
    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Bottom);
    QVERIFY2(k.ed->htmlSourcePane->isVisible(), "bottom must be visible");
    QCOMPARE(k.ed->sourceSplitter->orientation(), Qt::Vertical);
    QCOMPARE(k.ed->sourceSplitter->indexOf(k.ed->htmlSourcePane), 1);

    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Left);
    QCOMPARE(k.ed->sourceSplitter->orientation(), Qt::Horizontal);
    QCOMPARE(k.ed->sourceSplitter->indexOf(k.ed->htmlSourcePane), 0);
    QCOMPARE(k.ed->sourceSplitter->indexOf(k.ed->textArea), 1);

    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Right);
    QCOMPARE(k.ed->sourceSplitter->orientation(), Qt::Horizontal);
    QCOMPARE(k.ed->sourceSplitter->indexOf(k.ed->textArea), 0);
    QCOMPARE(k.ed->sourceSplitter->indexOf(k.ed->htmlSourcePane), 1);

    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Window);
    QVERIFY2(k.ed->htmlSourceWindow!=nullptr && k.ed->htmlSourceWindow->isVisible(),
             "window must be shown");
    QCOMPARE(k.ed->htmlSourcePane->parent(), k.ed->htmlWindowContainer);

    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Hidden);
    QVERIFY2(!k.ed->htmlSourcePane->isVisible(), "hidden must hide");

    k.ed->toggleHtmlSource();
    QCOMPARE(k.ed->htmlSourcePlacement(), MetaEditor::HtmlPlacement::Right);
    QVERIFY2(k.ed->htmlSourcePane->isVisible(), "toggle must restore");
    k.ed->toggleHtmlSource();
    QCOMPARE(k.ed->htmlSourcePlacement(), MetaEditor::HtmlPlacement::Hidden);

    dropEditor(k);
}

void TestSplit::fold(void)
{
    EditorKit k=makeEditor();
    k.ed->setHtmlSourceVisible(true);
    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Bottom);

    // Пустые абзацы сворачиваются в просмотре исходника
    QVERIFY2(HtmlSourceEdit::isEmptyParagraphLine(
                 QStringLiteral("<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><br /></p>")),
             "verbose empty must detect");
    QVERIFY2(HtmlSourceEdit::isEmptyParagraphLine(QStringLiteral("  <P><BR></P>  ")),
             "short empty must detect");
    QVERIFY2(HtmlSourceEdit::isEmptyParagraphLine(QStringLiteral("<p><br/></p>")),
             "nospace empty must detect");
    QVERIFY2(HtmlSourceEdit::isEmptyParagraphLine(QStringLiteral("<p  style=\"x\"><br  /></p>")),
             "spaced empty must detect");
    QVERIFY2(HtmlSourceEdit::isFoldableLine(
                 QStringLiteral("<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><br /></p>")),
             "verbose must be foldable");
    QVERIFY2(!HtmlSourceEdit::isFoldableLine(
                 QStringLiteral("<p class=\"mtempty\"><br /></p>")),
             "mtempty class must stay visible");
    QVERIFY2(!HtmlSourceEdit::isEmptyParagraphLine(
                 QStringLiteral("<p style=\" margin-top:0px;\">Text</p>")),
             "text must not be empty");
    QVERIFY2(!HtmlSourceEdit::isEmptyParagraphLine(
                 QStringLiteral("<ul><li><br /></li></ul>")),
             "list must not be empty");

    // Вернуть документ как был: FOLD-тест ниже дописывает пустые
    // абзацы в текущий документ и ждет их свернутыми
    k.ed->setTextarea(QStringLiteral("<p>aaa</p>\n<p>aaa</p>"));
    k.ed->syncNoteToSource();
    {
        QTextCursor c(k.ed->textArea->document());
        c.movePosition(QTextCursor::End);
        c.insertBlock();
        c.insertBlock();
        c.insertText(QStringLiteral("Last"));
        k.ed->setTextarea(k.ed->textArea->document()->toHtml());
    }
    k.ed->syncNoteToSource();
    QApplication::processEvents();
    {
        bool foundFolded=false;
        bool foundVisible=true;
        int foldedCount=0;
        for(QTextBlock b=k.ed->htmlSourcePane->document()->firstBlock();
            b.isValid(); b=b.next())
        {
            if(b.text().contains(QStringLiteral("-qt-paragraph-type:empty")))
            {
                ++foldedCount;
                if(!b.isVisible())
                    foundFolded=true;
            }
            else if(!b.text().trimmed().isEmpty() && !b.isVisible())
                foundVisible=false;
        }
        QVERIFY2(foldedCount>0, "empty blocks must exist");
        QVERIFY2(foundFolded, "empty must be folded");
        QVERIFY2(foundVisible, "text must stay visible");
    }

    // Клик в гуттере разворачивает, повторный сворачивает обратно
    {
        QTextBlock folded;
        for(QTextBlock b=k.ed->htmlSourcePane->document()->firstBlock();
            b.isValid(); b=b.next())
            if(!b.isVisible())
            {
                folded=b;
                break;
            }
        QVERIFY2(folded.isValid(), "hidden block must be found");

        // Строка выше свернутого куска: ее гуттер несет маркер [+]
        QTextBlock prev=folded.previous();
        QVERIFY2(prev.isValid(), "prev block must exist");

        QTextCursor c(k.ed->htmlSourcePane->document());
        c.setPosition(prev.position());
        k.ed->htmlSourcePane->setTextCursor(c);

        const int y=k.ed->htmlSourcePane->cursorRect(c).center().y();
        QVERIFY2(k.ed->htmlSourcePane->toggleFoldAt(y), "unfold click must work");
        QVERIFY2(folded.isVisible(), "block must unfold");

        // Клик по самой строке маркера [-] сворачивает обратно
        QTextCursor c2(k.ed->htmlSourcePane->document());
        c2.setPosition(folded.position());
        k.ed->htmlSourcePane->setTextCursor(c2);

        const int y2=k.ed->htmlSourcePane->cursorRect(c2).center().y();
        QVERIFY2(k.ed->htmlSourcePane->toggleFoldAt(y2), "refold click must work");
        QVERIFY2(!folded.isVisible(), "block must refold");
    }

    // Поиск в свернутой строке показывает ее
    {
        k.ed->htmlSourcePane->findNext(QStringLiteral("-qt-paragraph-type"));

        bool anyVisible=false;
        for(QTextBlock b=k.ed->htmlSourcePane->document()->firstBlock();
            b.isValid(); b=b.next())
            if(b.text().contains(QStringLiteral("-qt-paragraph-type")) && b.isVisible())
                anyVisible=true;

        QVERIFY2(anyVisible, "find must unhide folded line");
    }

    // Пустая заметка целиком: первую строку оставить видимой,
    // иначе панель станет полностью пустой и непонятной
    {
        k.ed->setTextarea(QStringLiteral("<p><br /></p>"));
        k.ed->syncNoteToSource();
        QApplication::processEvents();

        bool anyVisible=false;
        for(QTextBlock b=k.ed->htmlSourcePane->document()->firstBlock();
            b.isValid(); b=b.next())
            if(b.isVisible())
                anyVisible=true;

        QVERIFY2(anyVisible, "empty doc must keep one line visible");
    }

    dropEditor(k);
}

void TestSplit::normalize(void)
{
    const QString verbose=QStringLiteral("<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><br /></p>");
    const QString head=QStringLiteral("<html><head><style type=\"text/css\">\np, li { white-space: pre-wrap; }\n</style></head><body>");
    const QString tail=QStringLiteral("<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Text</p>");

    const QString normalized=Editor::normalizeEmptyParagraphs(head+verbose+tail);

    QVERIFY2(normalized.contains(QStringLiteral("<p class=\"mtempty\"><br /></p>")),
             "empty must become mtempty class");
    QCOMPARE(normalized.count(QStringLiteral("p.mtempty{")), 1);
    QCOMPARE(Editor::normalizeEmptyParagraphs(normalized), normalized);

    // Исключение с ненулевыми отступами: класс не лепится, инлайн остается
    const QString wide=QString(verbose).replace(QStringLiteral("margin-top:0px"),
                                                QStringLiteral("margin-top:12px"));
    QVERIFY2(!Editor::normalizeEmptyParagraphs(head+wide+tail).contains(QStringLiteral("mtempty")),
             "nonzero indent must stay inline");
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<p>hi</p>")),
             QStringLiteral("<p>hi</p>"));

    // Обычный абзац с дефолтными отступами: все сносится в голый <p>
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Text</p>")),
             QStringLiteral("<p>Text</p>"));

    // Ненулевые и неизвестные остаются, дефолтные рядом выкидываются
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<p style=\"margin-top:5px; margin-bottom:12px; -qt-block-indent:0;\">Text</p>")),
             QStringLiteral("<p style=\"margin-top:5px\">Text</p>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<p style=\"margin-top:5px; -qt-foo:bar;\">Text</p>")),
             QStringLiteral("<p style=\"margin-top:5px; -qt-foo:bar;\">Text</p>"));

    // Посторонние атрибуты тега целы, класс не лепится поверх них
    QCOMPARE(Editor::normalizeEmptyParagraphs(
             QStringLiteral("<p align=\"center\" style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px;\"><br /></p>")),
             QStringLiteral("<p align=\"center\" style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px;\"><br /></p>"));

    // Заголовки: свои дефолты у каждого - сносятся в голый тег
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h1 style=\" margin-top:18px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</h1>")),
             QStringLiteral("<h1>T</h1>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h2 style=\" margin-top:16px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</h2>")),
             QStringLiteral("<h2>T</h2>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h3 style=\" margin-top:14px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</h3>")),
             QStringLiteral("<h3>T</h3>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h4 style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</h4>")),
             QStringLiteral("<h4>T</h4>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h5 style=\" margin-top:12px; margin-bottom:4px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</h5>")),
             QStringLiteral("<h5>T</h5>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h6 style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</h6>")),
             QStringLiteral("<h6>T</h6>"));

    // Частые блоки: li, pre (12/12), div (0/0), blockquote (12/12+40px бока)
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<ul><li style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</li></ul>")),
             QStringLiteral("<ul><li>T</li></ul>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<pre style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</pre>")),
             QStringLiteral("<pre>T</pre>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<div style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</div>")),
             QStringLiteral("<div>T</div>"));
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<blockquote style=\" margin-top:12px; margin-bottom:12px; margin-left:40px; margin-right:40px; -qt-block-indent:0; text-indent:0px;\">T</blockquote>")),
             QStringLiteral("<blockquote>T</blockquote>"));

    // Пустой заголовок: маркер хранится инлайн, остальное сносится
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h1 style=\"-qt-paragraph-type:empty; margin-top:18px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><br /></h1>")),
             QStringLiteral("<h1 style=\"-qt-paragraph-type:empty\"><br /></h1>"));

    // Чужие отступы у заголовка остаются, дефолтные рядом выкидываются
    QCOMPARE(Editor::normalizeEmptyParagraphs(QStringLiteral("<h1 style=\"margin-top:5px; margin-bottom:12px; -qt-block-indent:0;\">T</h1>")),
             QStringLiteral("<h1 style=\"margin-top:5px\">T</h1>"));

    // Рендер заголовка: голый тег грузится в те же отступы и текст
    {
        QTextDocument docV, docB;
        docV.setHtml(QStringLiteral("<h1 style=\" margin-top:18px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">T</h1>"));
        docB.setHtml(QStringLiteral("<h1>T</h1>"));

        const QTextBlockFormat fv=docV.firstBlock().blockFormat();
        const QTextBlockFormat fb=docB.firstBlock().blockFormat();

        QVERIFY2(fv.topMargin()==fb.topMargin() &&
                 fv.bottomMargin()==fb.bottomMargin() &&
                 fv.leftMargin()==fb.leftMargin() &&
                 fv.rightMargin()==fb.rightMargin() &&
                 docV.toPlainText()==docB.toPlainText(),
                 "bare H1 must render like verbose");
    }

    // Эквивалентность рендера: класс с маркером в правиле грузится
    // в побайтово тот же пустой блок
    QTextDocument docVerbose, docNorm;
    docVerbose.setHtml(head+verbose+tail);
    docNorm.setHtml(normalized);

    bool sameFormat=(docVerbose.blockCount()==docNorm.blockCount());

    for(QTextBlock bv=docVerbose.firstBlock(), bn=docNorm.firstBlock();
        sameFormat && bv.isValid() && bn.isValid();
        bv=bv.next(), bn=bn.next())
    {
        const QTextBlockFormat fv=bv.blockFormat();
        const QTextBlockFormat fn=bn.blockFormat();

        sameFormat=(fv.topMargin()==fn.topMargin() &&
                    fv.bottomMargin()==fn.bottomMargin() &&
                    fv.leftMargin()==fn.leftMargin() &&
                    fv.rightMargin()==fn.rightMargin() &&
                    fv.indent()==fn.indent() &&
                    fv.textIndent()==fn.textIndent());
    }

    QVERIFY2(sameFormat, "verbose and normalized must render alike");
    QCOMPARE(docVerbose.toPlainText(), docNorm.toPlainText());

    // Короткий класс виден всегда, не схлопывается фолдингом
    QVERIFY2(HtmlSourceEdit::isFoldableLine(QStringLiteral("<p class=\"mtempty\"><br /></p>"))==false,
             "mtempty class must not fold");

    // Непустые абзацы с ровно нулевыми верхом/низом: общий класс
    // в шапке вместо инлайна margin-top:0px;margin-bottom:0px
    {
        const QString zhead=QStringLiteral("<html><head><style type=\"text/css\">\np, li { white-space: pre-wrap; }\n</style></head><body>");
        const QString ztail=QStringLiteral("</body></html>");
        const QString zpara=QStringLiteral("<p style=\"margin-top:0px;margin-bottom:0px;\">Text</p>");
        const QString znorm=Editor::normalizeEmptyParagraphs(zhead+zpara+ztail);

        QVERIFY2(znorm.contains(QStringLiteral("<p class=\"mt0\">Text</p>")),
                 "zero margins must become mt0 class");
        QCOMPARE(znorm.count(QStringLiteral(".mt0{")), 1);
        QCOMPARE(Editor::normalizeEmptyParagraphs(znorm), znorm);

        // Ненулевой отступ - исключение, остается инлайном
        const QString nonzero=Editor::normalizeEmptyParagraphs(
            zhead+QStringLiteral("<p style=\"margin-top:5px;margin-bottom:0px;\">Text</p>")+ztail);
        QVERIFY2(nonzero.contains(QStringLiteral("margin-top:5px")) &&
                 !nonzero.contains(QStringLiteral("class=\"mt0\"")),
                 "nonzero must stay inline");

        // Посторонние атрибуты тега целы: класс не лепится поверх
        QVERIFY2(!Editor::normalizeEmptyParagraphs(
                     zhead+QStringLiteral("<p align=\"center\" style=\"margin-top:0px;margin-bottom:0px;\">Text</p>")+ztail)
                     .contains(QStringLiteral("class=\"mt0\"")),
                 "attributes must block mt0 class");

        // Несколько нулевых - класс на каждом, правило одно
        const QString zmulti=Editor::normalizeEmptyParagraphs(zhead+zpara+zpara+ztail);
        QVERIFY2(zmulti.count(QStringLiteral("class=\"mt0\""))==2 &&
                 zmulti.count(QStringLiteral(".mt0{"))==1,
                 "each zero block gets the class, rule stays single");

        // Рендер: класс грузится в те же отступы, что инлайн
        QTextDocument docC, docI;
        docC.setHtml(znorm);
        docI.setHtml(zhead+zpara+ztail);
        const QTextBlockFormat fc=docC.firstBlock().blockFormat();
        const QTextBlockFormat fi=docI.firstBlock().blockFormat();
        QVERIFY2(fc.topMargin()==fi.topMargin() &&
                 fc.bottomMargin()==fi.bottomMargin() &&
                 docC.toPlainText()==docI.toPlainText(),
                 "mt0 class must render like inline");
    }
}

void TestSplit::sliderSizes(void)
{
    EditorKit k=makeEditor();
    k.ed->setHtmlSourceVisible(true);

    // Линейка отступов всегда над текстом
    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Left);
    QApplication::processEvents();
    QApplication::processEvents();
    {
        IndentSlider *slider=qobject_cast<IndentSlider *>(
            k.ed->indentSliderAssistant->getIndentSlider());
        QVERIFY2(slider!=nullptr, "indent slider must exist");
        // Свежий документ без отступов: маркеры стоят ровно на краях
        // текста, абсолютные координаты сверяются со сдвигом
        const int shift=k.ed->textArea->mapTo(slider->parentWidget(), QPoint(0, 0)).x()-
                       slider->x();
        QCOMPARE(slider->get_geometry_leftindent_pos(),
                 k.ed->textArea->getIndentStartedLeft()+shift);
        QCOMPARE(slider->get_geometry_rightindent_pos(),
                 k.ed->textArea->getIndentStartedRight()+shift);
    }

    // Исходник всегда уже текста: отложенные размеры успели встать
    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Left);
    QApplication::processEvents();
    QApplication::processEvents();
    {
        const QList<int> sizes=k.ed->sourceSplitter->sizes();
        QVERIFY2(sizes.size()==2 && sizes.at(0)>0 &&
                 sizes.at(k.ed->sourceSplitter->indexOf(k.ed->htmlSourcePane))<=
                 sizes.at(k.ed->sourceSplitter->indexOf(k.ed->textArea)),
                 "source must be narrower than text");
    }

    k.ed->setHtmlSourcePlacement(MetaEditor::HtmlPlacement::Right);
    QApplication::processEvents();
    QApplication::processEvents();
    {
        const QList<int> sizes=k.ed->sourceSplitter->sizes();
        QVERIFY2(sizes.size()==2 && sizes.at(1)>0 &&
                 sizes.at(k.ed->sourceSplitter->indexOf(k.ed->htmlSourcePane))<=
                 sizes.at(k.ed->sourceSplitter->indexOf(k.ed->textArea)),
                 "source must be narrower than text on right");
    }

    k.ed->setHtmlSourceVisible(false);
    QVERIFY2(!k.ed->isHtmlSourceVisible(), "source must hide");

    dropEditor(k);
}

QTEST_MAIN(TestSplit)

#include "test_split.moc"

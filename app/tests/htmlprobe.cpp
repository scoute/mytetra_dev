// HtmlSourceEdit: поиск/замена/гуттер без GUI-зависимостей.
// Виджет AppContext не трогает, инициализация контекста не нужна
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QKeyEvent>
#include <QPalette>
#include <QSettings>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>

#include <QSyntaxStyle>

#include "libraries/wyedit/HtmlSourceEdit.h"
#include "libraries/wyedit/EditorMultiLineInputDialog.h"
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

    HtmlSourceEdit edit;
    edit.setPlainText(QStringLiteral("<p>one</p>\n<div>two one</div>\n<p>three one</p>"));
    edit.show();
    QApplication::processEvents();

    // Поиск вперед шагает по строкам
    check(edit.findNext(QStringLiteral("one")), "FIND-FIRST");
    check(edit.textCursor().blockNumber()==0, "FIND-LINE1");
    check(edit.findNext(QStringLiteral("one")), "FIND-SECOND");
    check(edit.textCursor().blockNumber()==1, "FIND-LINE2");

    // Назад с текущей позиции
    check(edit.findPrevious(QStringLiteral("one")), "FIND-PREV");
    check(edit.textCursor().blockNumber()==0, "FIND-PREV-LINE1");

    // Зацикливание: с начала назад - последнее вхождение
    edit.moveCursor(QTextCursor::Start);
    check(edit.findPrevious(QStringLiteral("one")), "FIND-WRAP");
    check(edit.textCursor().blockNumber()==2, "FIND-WRAP-LINE3");

    // Нет совпадений
    check(!edit.findNext(QStringLiteral("zzz")), "FIND-NONE");
    check(!edit.findNext(QStringLiteral("")), "FIND-EMPTY");

    // Подсветка считает все вхождения
    check(edit.highlightMatches(QStringLiteral("one"))==3, "HIGHLIGHT-THREE");
    check(edit.highlightMatches(QStringLiteral(""))==0, "HIGHLIGHT-EMPTY");
    edit.clearMatchHighlight();

    // Регистр: без флага находит, с флагом - нет
    check(edit.findNext(QStringLiteral("ONE"))==true, "FIND-NOCASE");
    edit.moveCursor(QTextCursor::Start);
    check(edit.findNext(QStringLiteral("ONE"), QTextDocument::FindCaseSensitively)==false, "FIND-CASE");

    // Замена всех одной правкой
    edit.setPlainText(QStringLiteral("<p>one</p>\n<div>two one</div>\n<p>three one</p>"));
    check(edit.replaceAll(QStringLiteral("one"), QStringLiteral("1"))==3, "REPLACE-COUNT");
    check(!edit.toPlainText().contains(QStringLiteral("one")), "REPLACE-GONE");
    check(edit.toPlainText().count(QStringLiteral("1"))==3, "REPLACE-THREE");

    // Пустой запрос не вешает и ничего не трогает
    check(edit.replaceAll(QStringLiteral(""), QStringLiteral("x"))==0, "REPLACE-EMPTY");

    // Замена текущего: курсор не на вхождении - ищет и меняет
    edit.setPlainText(QStringLiteral("aaa bbb aaa"));
    edit.moveCursor(QTextCursor::Start);
    check(edit.replaceCurrent(QStringLiteral("bbb"), QStringLiteral("B")), "REPLACE-ONE");
    check(edit.toPlainText()==QStringLiteral("aaa B aaa"), "REPLACE-ONE-TEXT");

    // Переход к строке с клампом
    edit.setPlainText(QStringLiteral("l1\nl2\nl3\nl4\nl5"));
    edit.goToLine(3);
    check(edit.textCursor().blockNumber()==2, "GOTO-MID");
    edit.goToLine(999);
    check(edit.textCursor().blockNumber()==4, "GOTO-CLAMP-END");
    edit.goToLine(0);
    check(edit.textCursor().blockNumber()==0, "GOTO-CLAMP-START");

    // Подсветка реально работает: у тегов есть форматы отличные от plain.
    // Подсветка асинхронна (таймер QSyntaxHighlighter), ждем ее циклом
    edit.setPlainText(QStringLiteral("<p>one</p>\n<div>two one</div>"));

    // Диагностика окружения: ресурс стиля на месте, грузится
    check(QFile::exists(QStringLiteral(":/styles/html.xml")), "STYLE-RESOURCE");
    {
        QFile f(QStringLiteral(":/styles/html.xml"));
        QSyntaxStyle probe;
        check(f.open(QIODevice::ReadOnly) && probe.load(QString::fromUtf8(f.readAll())), "STYLE-LOAD");
        check(probe.getFormat(QStringLiteral("Keyword")).foreground().style()!=Qt::NoBrush, "STYLE-KEYWORD");
    }

    // Выбор палитры: светлая база - html, темная - html-dark
    check(edit.syntaxStyleName()==QStringLiteral("Mytetra-html"), "STYLE-LIGHT");
    {
        const QPalette saved=qApp->palette();
        QPalette dark=saved;
        dark.setColor(QPalette::Base, QColor(30, 30, 30));
        dark.setColor(QPalette::Text, QColor(232, 232, 232));
        qApp->setPalette(dark);

        HtmlSourceEdit darkEdit;
        check(darkEdit.syntaxStyleName()==QStringLiteral("Mytetra-html-dark"), "STYLE-DARK");

        qApp->setPalette(saved);
    }
    bool colored=false;
    QApplication::processEvents();

    for(QTextBlock block=edit.document()->begin(); block.isValid(); block=block.next())
        // Именно formats(): additionalFormats() в 5.15 deprecated-заглушка
        if(!block.layout()->formats().isEmpty())
        {
            colored=true;
            break;
        }
    check(colored, "HIGHLIGHT-ACTIVE");

    // Гуттер: ширина ненулевая и растет с разрядностью номеров
    edit.setPlainText(QStringLiteral("a\nb\nc"));
    const int narrow=edit.lineNumberAreaWidth();
    QString many;
    for(int i=0; i<150; ++i)
        many+=QStringLiteral("line\n");
    edit.setPlainText(many);
    QApplication::processEvents();
    const int wide=edit.lineNumberAreaWidth();
    check(narrow>0, "GUTTER-VISIBLE");
    check(wide>narrow, "GUTTER-GROWS");

    // Пара тегов: курсор внутри <div> - подсвечены оба тега
    edit.clearMatchHighlight();
    edit.setPlainText(QStringLiteral("<div>\n<p>x</p>\n</div>"));
    {
        QTextCursor c(edit.document());
        c.setPosition(1);
        edit.setTextCursor(c);
    }
    check(edit.extraSelections().size()==2, "PAIR-FOUND");

    // Без пары: тревожная подсветка одного тега
    edit.setPlainText(QStringLiteral("<div>\n<p>"));
    {
        QTextCursor c(edit.document());
        c.setPosition(1);
        edit.setTextCursor(c);
    }
    check(edit.extraSelections().size()==1, "PAIR-MISMATCH");

    // Вне тегов - пусто
    edit.moveCursor(QTextCursor::End);
    check(edit.extraSelections().size()==0, "PAIR-NONE");

    // Enter раскрывает контейнер, курсор остается внутри
    edit.setPlainText(QStringLiteral("<div>"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&edit, &enter);
    }
    check(edit.toPlainText()==QStringLiteral("<div>\n    \n</div>"), "ENTER-SPLIT");
    check(edit.textCursor().blockNumber()==1, "ENTER-CURSOR-MID");

    // Enter копирует отступ обычной строки
    edit.setPlainText(QStringLiteral("    abc"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&edit, &enter);
    }
    check(edit.toPlainText()==QStringLiteral("    abc\n    "), "ENTER-INDENT");

    // Автозакрытие: <div + > дает пару, курсор между
    edit.setPlainText(QStringLiteral("<div"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent greater(QEvent::KeyPress, Qt::Key_Greater, Qt::NoModifier, QStringLiteral(">"));
        QApplication::sendEvent(&edit, &greater);
    }
    check(edit.toPlainText()==QStringLiteral("<div></div>"), "AUTOCLOSE");
    check(edit.textCursor().position()==5, "AUTOCLOSE-CURSOR");

    // Закрывающий уже стоит - дубликата нет (лишний '>' остается как набран)
    edit.setPlainText(QStringLiteral("<div></div>"));
    {
        QTextCursor c(edit.document());
        c.setPosition(4);
        edit.setTextCursor(c);
        QKeyEvent greater(QEvent::KeyPress, Qt::Key_Greater, Qt::NoModifier, QStringLiteral(">"));
        QApplication::sendEvent(&edit, &greater);
    }
    check(edit.toPlainText()==QStringLiteral("<div>></div>"), "AUTOCLOSE-NODUP");

    // Пустые элементы не закрываются
    edit.setPlainText(QStringLiteral("<br"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent greater(QEvent::KeyPress, Qt::Key_Greater, Qt::NoModifier, QStringLiteral(">"));
        QApplication::sendEvent(&edit, &greater);
    }
    check(edit.toPlainText()==QStringLiteral("<br>"), "AUTOCLOSE-VOID");

    // Диалог: конструктор ходит в ShortcutManager через AppContext,
    // панель и движок связаны через findChild
    QFile::remove(QStringLiteral("/tmp/opencode/probehtml.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probehtml.ini"),
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

    EditorMultiLineInputDialog dlg;
    dlg.setText(QStringLiteral("<p>alpha</p>"));
    dlg.show();
    QApplication::processEvents();

    HtmlSourceEdit *area=dlg.findChild<HtmlSourceEdit *>();
    check(area!=nullptr, "DLG-AREA");
    check(area->findNext(QStringLiteral("alpha")), "DLG-FIND");
    check(dlg.getText().contains(QStringLiteral("alpha")), "DLG-GETTEXT");
    check(area->replaceAll(QStringLiteral("alpha"), QStringLiteral("beta"))==1, "DLG-REPLACE");
    check(dlg.getText().contains(QStringLiteral("beta")), "DLG-REPLACED");
    check(dlg.isModified(), "DLG-MODIFIED");
    check(area->lineNumberAreaWidth()>0, "DLG-GUTTER");

    qDebug() << (fails==0 ? "HTMLPROBE-OK" : "HTMLPROBE-BAD");
    return fails;
}

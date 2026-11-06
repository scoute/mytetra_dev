// HtmlSourceEdit на QTest, линия v2 (см. 498).
// Поиск/замена/гуттер/дерево тегов/пары/Enter/
// автозакрытие/стили без GUI-зависимостей; диалог - с extern ShortcutManager.
// Порт htmlprobe: поиск/замена/гуттер/дерево тегов/пары/Enter/
// автозакрытие/стили без GUI-зависимостей; диалог - с AppContext.
// Виджет AppContext не трогает кроме диалога.
#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QKeyEvent>
#include <QPalette>
#include <QSettings>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QTemporaryDir>

#include <QSyntaxStyle>

#include "libraries/wyedit/HtmlSourceEdit.h"
#include "libraries/wyedit/HtmlTagTree.h"
#include "libraries/wyedit/EditorMultiLineInputDialog.h"
#include "libraries/ShortcutManager.h"
#include "models/appConfig/AppConfig.h"

extern AppConfig mytetraConfig;
extern ShortcutManager shortcutManager;

class TestHtml : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void findReplace(void);
    void highlightCounts(void);
    void gotoLine(void);
    void styles(void);
    void gutter(void);
    void tagTree(void);
    void pairHighlight(void);
    void enterIndent(void);
    void autoclose(void);
    void dialog(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;
};

void TestHtml::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");
    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_html.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;

    shortcutManager.init();
}

void TestHtml::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestHtml::findReplace(void)
{
    HtmlSourceEdit edit;
    edit.setPlainText(QStringLiteral("<p>one</p>\n<div>two one</div>\n<p>three one</p>"));
    edit.show();
    QApplication::processEvents();

    // Поиск вперед шагает по строкам
    QVERIFY2(edit.findNext(QStringLiteral("one")), "first must be found");
    QCOMPARE(edit.textCursor().blockNumber(), 0);
    QVERIFY2(edit.findNext(QStringLiteral("one")), "second must be found");
    QCOMPARE(edit.textCursor().blockNumber(), 1);

    // Назад с текущей позиции
    QVERIFY2(edit.findPrevious(QStringLiteral("one")), "prev must be found");
    QCOMPARE(edit.textCursor().blockNumber(), 0);

    // Зацикливание: с начала назад - последнее вхождение
    edit.moveCursor(QTextCursor::Start);
    QVERIFY2(edit.findPrevious(QStringLiteral("one")), "wrap must be found");
    QCOMPARE(edit.textCursor().blockNumber(), 2);

    // Нет совпадений
    QVERIFY2(!edit.findNext(QStringLiteral("zzz")), "none must not be found");
    QVERIFY2(!edit.findNext(QStringLiteral("")), "empty must not be found");

    // Регистр: без флага находит, с флагом - нет
    QVERIFY2(edit.findNext(QStringLiteral("ONE"))==true, "nocase must find");
    edit.moveCursor(QTextCursor::Start);
    QVERIFY2(edit.findNext(QStringLiteral("ONE"), QTextDocument::FindCaseSensitively)==false,
             "case must not find");

    // Замена всех одной правкой
    edit.setPlainText(QStringLiteral("<p>one</p>\n<div>two one</div>\n<p>three one</p>"));
    QCOMPARE(edit.replaceAll(QStringLiteral("one"), QStringLiteral("1")), 3);
    QVERIFY2(!edit.toPlainText().contains(QStringLiteral("one")), "all must be gone");
    QCOMPARE(edit.toPlainText().count(QStringLiteral("1")), 3);

    // Пустой запрос не вешает и ничего не трогает
    QCOMPARE(edit.replaceAll(QStringLiteral(""), QStringLiteral("x")), 0);

    // Замена текущего: курсор не на вхождении - ищет и меняет
    edit.setPlainText(QStringLiteral("aaa bbb aaa"));
    edit.moveCursor(QTextCursor::Start);
    QVERIFY2(edit.replaceCurrent(QStringLiteral("bbb"), QStringLiteral("B")),
             "current must be replaced");
    QCOMPARE(edit.toPlainText(), QStringLiteral("aaa B aaa"));
}

void TestHtml::highlightCounts(void)
{
    HtmlSourceEdit edit;
    edit.setPlainText(QStringLiteral("<p>one</p>\n<div>two one</div>\n<p>three one</p>"));

    // Подсветка считает все вхождения
    QCOMPARE(edit.highlightMatches(QStringLiteral("one")), 3);
    QCOMPARE(edit.highlightMatches(QStringLiteral("")), 0);
    edit.clearMatchHighlight();
}

void TestHtml::gotoLine(void)
{
    HtmlSourceEdit edit;
    edit.setPlainText(QStringLiteral("l1\nl2\nl3\nl4\nl5"));

    // Переход к строке с клампом
    edit.goToLine(3);
    QCOMPARE(edit.textCursor().blockNumber(), 2);
    edit.goToLine(999);
    QCOMPARE(edit.textCursor().blockNumber(), 4);
    edit.goToLine(0);
    QCOMPARE(edit.textCursor().blockNumber(), 0);
}

void TestHtml::styles(void)
{
    HtmlSourceEdit edit;
    edit.setPlainText(QStringLiteral("<p>one</p>\n<div>two one</div>"));

    // Диагностика окружения: ресурс стиля на месте, грузится
    QVERIFY2(QFile::exists(QStringLiteral(":/styles/html.xml")), "style resource must exist");
    {
        QFile f(QStringLiteral(":/styles/html.xml"));
        QSyntaxStyle probe;
        QVERIFY2(f.open(QIODevice::ReadOnly) && probe.load(QString::fromUtf8(f.readAll())),
                 "style must load");
        QVERIFY2(probe.getFormat(QStringLiteral("Keyword")).foreground().style()!=Qt::NoBrush,
                 "keyword must have a brush");
    }

    // Выбор палитры: светлая база - html, темная - html-dark
    QCOMPARE(edit.syntaxStyleName(), QStringLiteral("Mytetra-html"));
    {
        const QPalette saved=qApp->palette();
        QPalette dark=saved;
        dark.setColor(QPalette::Base, QColor(30, 30, 30));
        dark.setColor(QPalette::Text, QColor(232, 232, 232));
        qApp->setPalette(dark);

        HtmlSourceEdit darkEdit;
        QCOMPARE(darkEdit.syntaxStyleName(), QStringLiteral("Mytetra-html-dark"));

        qApp->setPalette(saved);
    }

    // Подсветка реально работает: у тегов есть форматы отличные от plain.
    // Подсветка асинхронна (таймер QSyntaxHighlighter), ждем ее циклом
    bool colored=false;
    QApplication::processEvents();

    for(QTextBlock block=edit.document()->begin(); block.isValid(); block=block.next())
        // Именно formats(): additionalFormats() в 5.15 deprecated-заглушка
        if(!block.layout()->formats().isEmpty())
        {
            colored=true;
            break;
        }
    QVERIFY2(colored, "tag formats must appear");
}

void TestHtml::gutter(void)
{
    HtmlSourceEdit edit;

    // Гуттер: ширина ненулевая и растет с разрядностью номеров
    edit.setPlainText(QStringLiteral("a\nb\nc"));
    const int narrow=edit.lineNumberAreaWidth();
    QString many;
    for(int i=0; i<150; ++i)
        many+=QStringLiteral("line\n");
    edit.setPlainText(many);
    QApplication::processEvents();
    const int wide=edit.lineNumberAreaWidth();
    QVERIFY2(narrow>0, "gutter must be visible");
    QVERIFY2(wide>narrow, "gutter must grow");
}

void TestHtml::tagTree(void)
{
    HtmlSourceEdit edit;

    // Дерево тегов: структура, подписи, прыжок, синхронизация.
    // Комментарий со span внутрь не попадает, br - лист без детей
    edit.setPlainText(QStringLiteral("<div id=\"a\"><p>x</p><br></div><!-- <span>no</span> -->"));
    HtmlTagTree tree;
    tree.setEditor(&edit);
    QCOMPARE(tree.topLevelItemCount(), 1);
    QTreeWidgetItem *divNode=tree.topLevelItem(0);
    QVERIFY2(divNode->text(0).contains(QStringLiteral("div"))
             && divNode->text(0).contains(QStringLiteral("#a")),
             "div must be labeled with id");
    QCOMPARE(divNode->childCount(), 2);
    QCOMPARE(tree.itemForPosition(1), divNode);
    QTreeWidgetItem *pNode=divNode->child(0);
    QCOMPARE(pNode->text(0), QStringLiteral("p"));
    QCOMPARE(tree.itemForPosition(13), pNode);
    tree.jumpToItem(pNode);
    QCOMPARE(edit.textCursor().position(), 12);
    {
        QTextCursor c(edit.document());
        c.setPosition(1);
        edit.setTextCursor(c);
    }
    tree.syncToCursor();
    QCOMPARE(tree.currentItem(), divNode);
}

void TestHtml::pairHighlight(void)
{
    HtmlSourceEdit edit;

    // Пара тегов: курсор внутри <div> - подсвечены оба тега
    edit.clearMatchHighlight();
    edit.setPlainText(QStringLiteral("<div>\n<p>x</p>\n</div>"));
    {
        QTextCursor c(edit.document());
        c.setPosition(1);
        edit.setTextCursor(c);
    }
    QCOMPARE(edit.extraSelections().size(), 2);

    // Без пары: тревожная подсветка одного тега
    edit.setPlainText(QStringLiteral("<div>\n<p>"));
    {
        QTextCursor c(edit.document());
        c.setPosition(1);
        edit.setTextCursor(c);
    }
    QCOMPARE(edit.extraSelections().size(), 1);

    // Вне тегов - пусто
    edit.moveCursor(QTextCursor::End);
    QCOMPARE(edit.extraSelections().size(), 0);
}

void TestHtml::enterIndent(void)
{
    HtmlSourceEdit edit;

    // Enter раскрывает контейнер, курсор остается внутри
    edit.setPlainText(QStringLiteral("<div>"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&edit, &enter);
    }
    QCOMPARE(edit.toPlainText(), QStringLiteral("<div>\n    \n</div>"));
    QCOMPARE(edit.textCursor().blockNumber(), 1);

    // Enter копирует отступ обычной строки
    edit.setPlainText(QStringLiteral("    abc"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&edit, &enter);
    }
    QCOMPARE(edit.toPlainText(), QStringLiteral("    abc\n    "));
}

void TestHtml::autoclose(void)
{
    HtmlSourceEdit edit;

    // Автозакрытие: <div + > дает пару, курсор между
    edit.setPlainText(QStringLiteral("<div"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent greater(QEvent::KeyPress, Qt::Key_Greater, Qt::NoModifier, QStringLiteral(">"));
        QApplication::sendEvent(&edit, &greater);
    }
    QCOMPARE(edit.toPlainText(), QStringLiteral("<div></div>"));
    QCOMPARE(edit.textCursor().position(), 5);

    // Закрывающий уже стоит - дубликата нет (лишний '>' остается как набран)
    edit.setPlainText(QStringLiteral("<div></div>"));
    {
        QTextCursor c(edit.document());
        c.setPosition(4);
        edit.setTextCursor(c);
        QKeyEvent greater(QEvent::KeyPress, Qt::Key_Greater, Qt::NoModifier, QStringLiteral(">"));
        QApplication::sendEvent(&edit, &greater);
    }
    QCOMPARE(edit.toPlainText(), QStringLiteral("<div>></div>"));

    // Пустые элементы не закрываются
    edit.setPlainText(QStringLiteral("<br"));
    edit.moveCursor(QTextCursor::End);
    {
        QKeyEvent greater(QEvent::KeyPress, Qt::Key_Greater, Qt::NoModifier, QStringLiteral(">"));
        QApplication::sendEvent(&edit, &greater);
    }
    QCOMPARE(edit.toPlainText(), QStringLiteral("<br>"));
}

void TestHtml::dialog(void)
{
    // Диалог: конструктор ходит в extern ShortcutManager
    // панель и движок связаны через findChild
    EditorMultiLineInputDialog dlg;
    dlg.setText(QStringLiteral("<p>alpha</p>"));
    dlg.show();
    QApplication::processEvents();

    HtmlSourceEdit *area=dlg.findChild<HtmlSourceEdit *>();
    QVERIFY2(area!=nullptr, "dialog must contain an editor");
    QVERIFY2(area->findNext(QStringLiteral("alpha")), "dialog editor must find");
    QVERIFY2(dlg.getText().contains(QStringLiteral("alpha")), "dialog must return text");
    QCOMPARE(area->replaceAll(QStringLiteral("alpha"), QStringLiteral("beta")), 1);
    QVERIFY2(dlg.getText().contains(QStringLiteral("beta")), "dialog must return replaced");
    QVERIFY2(dlg.isModified(), "dialog must be modified");
    QVERIFY2(area->lineNumberAreaWidth()>0, "dialog gutter must be visible");
}

QTEST_MAIN(TestHtml)

#include "test_html.moc"

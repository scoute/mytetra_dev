#include <QFile>
#include <QFontDatabase>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>
#include <QApplication>
#include <QPalette>

#include <QXMLHighlighter>
#include <QStyleSyntaxHighlighter>
#include <QSyntaxStyle>

#include "HtmlSourceEdit.h"


// Полоса номеров строк по примеру Code Editor Example из Qt:
// геометрию и покраску ведет редактор, здесь только пересылка paint
class HtmlSourceEdit::LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(HtmlSourceEdit *editor) : QWidget(editor), codeEditor(editor)
    {
    }

    QSize sizeHint(void) const override
    {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->paintLineNumberArea(event);
    }

private:
    HtmlSourceEdit *codeEditor;
};


HtmlSourceEdit::HtmlSourceEdit(QWidget *parent) : QPlainTextEdit(parent)
{
    // Моноширинный системный: HTML читается колонками
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    // Перенос по умолчанию как у старого QTextEdit в этом диалоге
    setLineWrapMode(QPlainTextEdit::WidgetWidth);

    // Подсветка тегов тем же движком что блоки кода в заметках
    xmlHighlighter=new QXMLHighlighter(document());
    QSyntaxStyle *style=new QSyntaxStyle(xmlHighlighter);

    // Тема подсветки по палитре приложения: темная база - светлые
    // цвета, иначе приглушенные под светлый фон (как в блоках кода).
    // Работает и для кастомных тем: смотрится именно итоговая палитра
    const QColor base=qApp->palette().color(QPalette::Active, QPalette::Base);
    const QString stylePath=(base.lightness()<128)
        ? QStringLiteral(":/styles/html-dark.xml")
        : QStringLiteral(":/styles/html.xml");

    QFile styleFile(stylePath);

    if(styleFile.open(QIODevice::ReadOnly))
        style->load(styleFile.readAll());

    // Стиль без фонов (в html.xml только foreground): на темных темах
    // редактор остается на палитре приложения, как блоки кода
    xmlHighlighter->setSyntaxStyle(style);

    lineNumberArea=new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &HtmlSourceEdit::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &HtmlSourceEdit::updateLineNumberArea);

    updateLineNumberAreaWidth(0);
}


HtmlSourceEdit::~HtmlSourceEdit()
{
    // xmlHighlighter и lineNumberArea - дети, удалятся сами
}


bool HtmlSourceEdit::findInternal(const QString &text,
                                  QTextDocument::FindFlags flags, bool backward)
{
    if(text.isEmpty())
        return false;

    if(backward)
        flags|=QTextDocument::FindBackward;

    QTextCursor cursor=textCursor();
    QTextCursor found=document()->find(text, cursor, flags);

    // Зацикливание: второй шанс с начала (или с конца для назад)
    if(found.isNull())
    {
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        found=document()->find(text, cursor, flags);
    }

    if(found.isNull())
        return false;

    setTextCursor(found);
    ensureCursorVisible();

    return true;
}


bool HtmlSourceEdit::findNext(const QString &text, QTextDocument::FindFlags flags)
{
    return findInternal(text, flags, false);
}


bool HtmlSourceEdit::findPrevious(const QString &text, QTextDocument::FindFlags flags)
{
    return findInternal(text, flags, true);
}


int HtmlSourceEdit::highlightMatches(const QString &text, QTextDocument::FindFlags flags)
{
    QList<QTextEdit::ExtraSelection> selections;

    if(!text.isEmpty())
    {
        QTextCursor cursor(document());
        cursor.movePosition(QTextCursor::Start);
        QTextCharFormat matchFormat;
        matchFormat.setBackground(QColor(QStringLiteral("#ffef0b")));
        matchFormat.setForeground(Qt::black);

        int count=0;

        while(count<maxHighlightMatches)
        {
            cursor=document()->find(text, cursor, flags);

            if(cursor.isNull())
                break;

            QTextEdit::ExtraSelection selection;
            selection.cursor=cursor;
            selection.format=matchFormat;
            selections.append(selection);
            ++count;
        }
    }

    setExtraSelections(selections);

    return selections.size();
}


void HtmlSourceEdit::clearMatchHighlight(void)
{
    setExtraSelections(QList<QTextEdit::ExtraSelection>());
}


bool HtmlSourceEdit::replaceCurrent(const QString &findText, const QString &replaceText,
                                    QTextDocument::FindFlags flags)
{
    if(findText.isEmpty())
        return false;

    QTextCursor cursor=textCursor();

    // Курсор уже стоит на вхождении: совпадение с учетом регистра
    const QString selected=cursor.selectedText();
    const bool selectedMatches=(flags & QTextDocument::FindCaseSensitively)
        ? (selected==findText)
        : (selected.compare(findText, Qt::CaseInsensitive)==0);

    if(!cursor.hasSelection() || !selectedMatches)
    {
        if(!findNext(findText, flags))
            return false;

        cursor=textCursor();
    }

    cursor.insertText(replaceText);

    return true;
}


int HtmlSourceEdit::replaceAll(const QString &findText, const QString &replaceText,
                               QTextDocument::FindFlags flags)
{
    if(findText.isEmpty())
        return 0;

    // Курсор блока отдельный и никогда не переназначается: переназначение
    // владельца beginEditBlock внутри цикла рвет баланс блока в Qt,
    // и последующие правки перестают пересвечиваться
    QTextCursor blockCursor(document());
    blockCursor.beginEditBlock();

    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::Start);

    int count=0;

    while(true)
    {
        cursor=document()->find(findText, cursor, flags);

        if(cursor.isNull())
            break;

        cursor.insertText(replaceText);
        ++count;
    }

    blockCursor.endEditBlock();

    // Позиция после массовой замены: в начало, курсор виден
    moveCursor(QTextCursor::Start);
    ensureCursorVisible();

    return count;
}


void HtmlSourceEdit::goToLine(int line)
{
    const int lastLine=document()->blockCount();
    const int target=qBound(1, line, qMax(1, lastLine));

    QTextCursor cursor(document()->findBlockByNumber(target-1));
    setTextCursor(cursor);

    // Именно ensure, не centerCursor: center на еще не раскатанном
    // лэйауте (setPlainText без показа/покраски) падает внутри Qt
    // в QTextLine::naturalTextRect
    ensureCursorVisible();
}


QString HtmlSourceEdit::syntaxStyleName(void) const
{
    if(xmlHighlighter==nullptr || xmlHighlighter->syntaxStyle()==nullptr)
        return QString();

    return xmlHighlighter->syntaxStyle()->name();
}


int HtmlSourceEdit::lineNumberAreaWidth(void) const
{
    int digits=1;
    int maxBlock=qMax(1, blockCount());

    while(maxBlock>=10)
    {
        maxBlock/=10;
        ++digits;
    }

    return 6+fontMetrics().horizontalAdvance(QLatin1Char('9'))*digits;
}


void HtmlSourceEdit::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    const QRect contents=contentsRect();
    lineNumberArea->setGeometry(QRect(contents.left(), contents.top(),
                                     lineNumberAreaWidth(), contents.height()));
}


void HtmlSourceEdit::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}


void HtmlSourceEdit::updateLineNumberArea(const QRect &rect, int dy)
{
    if(dy!=0)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if(rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}


void HtmlSourceEdit::paintLineNumberArea(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), palette().color(QPalette::Midlight));
    painter.setPen(palette().color(QPalette::Dark));

    QTextBlock block=firstVisibleBlock();
    int blockNumber=block.blockNumber();
    int top=qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom=top+qRound(blockBoundingRect(block).height());

    QTextCursor cursor=textCursor();
    const int currentLine=cursor.blockNumber();

    while(block.isValid() && top<=event->rect().bottom())
    {
        if(block.isVisible() && bottom>=event->rect().top())
        {
            // Текущая строка выделяется жирным: видно где курсор
            if(blockNumber==currentLine)
            {
                QFont boldFont=painter.font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
                painter.drawText(0, top, lineNumberArea->width()-3, fontMetrics().height(),
                                 Qt::AlignRight, QString::number(blockNumber+1));

                QFont normalFont=painter.font();
                normalFont.setBold(false);
                painter.setFont(normalFont);
            }
            else
            {
                painter.drawText(0, top, lineNumberArea->width()-3, fontMetrics().height(),
                                 Qt::AlignRight, QString::number(blockNumber+1));
            }
        }

        block=block.next();
        top=bottom;
        bottom=top+qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

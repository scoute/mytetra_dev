#include <QFile>
#include <QFontDatabase>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QApplication>
#include <QPalette>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QSet>

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

    void mousePressEvent(QMouseEvent *event) override
    {
        if(event->button()==Qt::LeftButton)
            codeEditor->toggleFoldAt(event->pos().y());
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

    // Подсветка парного тега следит за курсором и правками
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &HtmlSourceEdit::updatePairHighlight);
    connect(document(), &QTextDocument::contentsChanged,
            this, &HtmlSourceEdit::updatePairHighlight);

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

    // Вхождение могло попасть в свернутый пустой абзац:
    // показать блок, иначе курсор останется невидимым
    if(!found.block().isVisible())
    {
        QTextBlock unhideBlock=found.block();
        unhideBlock.setVisible(true);
        document()->markContentsDirty(unhideBlock.position(),
                                      unhideBlock.length());
    }

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
    matchSelections.clear();

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
            matchSelections.append(selection);
            ++count;
        }
    }

    applySelections();

    return matchSelections.size();
}


void HtmlSourceEdit::clearMatchHighlight(void)
{
    matchSelections.clear();
    applySelections();
}


void HtmlSourceEdit::applySelections(void)
{
    QList<QTextEdit::ExtraSelection> all=matchSelections+pairSelections;
    setExtraSelections(all);
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

    if(!cursor.block().isVisible())
    {
        QTextBlock unhideBlock=cursor.block();
        unhideBlock.setVisible(true);
        document()->markContentsDirty(unhideBlock.position(),
                                      unhideBlock.length());
    }

    setTextCursor(cursor);

    // Именно ensure, не centerCursor: center на еще не раскатанном
    // лэйауте (setPlainText без показа/покраски) падает внутри Qt
    // в QTextLine::naturalTextRect
    ensureCursorVisible();
}


// Пустой абзац toHtml: <p ...><br /></p> в одну строку.
// Атрибуты любые (Qt всегда пишет полный набор), важен только
// вид: открывающий p, одиночный br, закрывающий p
bool HtmlSourceEdit::isEmptyParagraphLine(const QString &line)
{
    static const QRegularExpression emptyPara(
        QStringLiteral("^\\s*<p(\\s[^<>]*)?>\\s*<br\\s*/?>\\s*</p>\\s*$"),
        QRegularExpression::CaseInsensitiveOption);

    return emptyPara.match(line).hasMatch();
}

bool HtmlSourceEdit::isFoldableLine(const QString &line)
{
    return isEmptyParagraphLine(line) &&
           line.contains(QStringLiteral("style="));
}


// Свернуть все пустые абзацы после полной замены текста.
// Ручные развороты сбрасываются: вызывается только из refreshSourcePane
void HtmlSourceEdit::updateEmptyFolds(void)
{
    bool any=false;
    int visibleCount=0;

    for(QTextBlock block=document()->firstBlock(); block.isValid(); block=block.next())
    {
        // Сворачиваются только развесистые инлайн-варианты: короткий
        // класс уже компактен и остается видимым якорем
        if(isFoldableLine(block.text()))
        {
            block.setVisible(false);
            any=true;
        }
        else
        {
            block.setVisible(true);
            ++visibleCount;
        }
    }

    // Все строки пустые (пустая заметка): первую оставить видимой,
    // иначе панель станет полностью пустой и непонятной
    if(any && visibleCount==0)
    {
        QTextBlock first=document()->firstBlock();

        if(first.isValid())
            first.setVisible(true);
    }

    document()->markContentsDirty(0, document()->characterCount());

    m_hasEmptyFolds=any;
    updateLineNumberAreaWidth(0);
    viewport()->update();
    lineNumberArea->update();
}


// Клик в гуттере: развернуть свернутый кусок после строки
// или свернуть пустой абзац под кликом
bool HtmlSourceEdit::toggleFoldAt(int gutterY)
{
    QTextCursor probe=cursorForPosition(QPoint(5, gutterY));

    if(probe.isNull())
        return false;

    QTextBlock block=probe.block();

    if(!block.isValid())
        return false;

    // После строки свернутый кусок: показать его целиком
    QTextBlock next=block.next();

    if(next.isValid() && !next.isVisible())
    {
        int from=next.position();
        int to=from;

        for(QTextBlock b=next; b.isValid() && !b.isVisible(); b=b.next())
        {
            b.setVisible(true);
            to=b.position()+b.length();
        }

        document()->markContentsDirty(from, to-from);
        viewport()->update();
        lineNumberArea->update();
        return true;
    }

    // Сама строка - пустой абзац: свернуть весь смежный кусок,
    // чтобы пачкой пустых строк не кликать по одной
    if(!isFoldableLine(block.text()))
        return false;

    QTextBlock first=block;

    while(first.previous().isValid() &&
          isFoldableLine(first.previous().text()))
        first=first.previous();

    int from=first.position();
    int to=from;

    for(QTextBlock b=first; b.isValid() && isFoldableLine(b.text()); b=b.next())
    {
        b.setVisible(false);
        to=b.position()+b.length();
    }

    document()->markContentsDirty(from, to-from);
    viewport()->update();
    lineNumberArea->update();
    return true;
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

    const int digitWidth=fontMetrics().horizontalAdvance(QLatin1Char('9'));

    // При свертках слева рисуются маркеры [-] и [+N]: им своя зона,
    // номера как раньше прижаты вправо
    if(m_hasEmptyFolds)
        return 6+digitWidth*digits+digitWidth*5;

    return 6+digitWidth*digits;
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

            // Маркеры сверток слева: [-] сворачивает пустой абзац,
            // [+N] разворачивает N спрятанных строк за ним
            if(m_hasEmptyFolds)
            {
                int hiddenRun=0;

                for(QTextBlock ahead=block.next();
                    ahead.isValid() && !ahead.isVisible() && hiddenRun<100;
                    ahead=ahead.next())
                    ++hiddenRun;

                if(hiddenRun>0)
                {
                    painter.drawText(2, top, lineNumberArea->width()-3, fontMetrics().height(),
                                     Qt::AlignLeft,
                                     QStringLiteral("[+")+
                                     (hiddenRun>99 ? QStringLiteral("99") : QString::number(hiddenRun))+
                                     QStringLiteral("]"));
                }
                else if(isFoldableLine(block.text()))
                {
                    painter.drawText(2, top, lineNumberArea->width()-3, fontMetrics().height(),
                                     Qt::AlignLeft, QStringLiteral("[-]"));
                }
            }
        }

        block=block.next();
        top=bottom;
        bottom=top+qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}


void HtmlSourceEdit::keyPressEvent(QKeyEvent *event)
{
    const bool isEnter=(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter)
        && event->modifiers()==Qt::NoModifier;
    const bool isGreater=(event->key()==Qt::Key_Greater
        || event->text()==QStringLiteral(">"));

    if(isEnter)
    {
        handleSmartEnter(event);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);

    if(isGreater)
        autoCloseTag();
}


void HtmlSourceEdit::handleSmartEnter(QKeyEvent *event)
{
    QTextCursor cursor=textCursor();

    if(cursor.hasSelection())
    {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    // Отступ текущей строки: пробелы и табы с ее начала
    const QString blockText=cursor.block().text();
    QString indent;

    for(QChar ch : blockText)
    {
        if(ch.isSpace())
            indent+=ch;
        else
            break;
    }

    // Тег, только что закрытый перед курсором: <div>|
    TagSpan tag;
    const bool afterOpenTag=tagEndingAt(cursor.position(), tag)
        && !tag.special && !tag.isClose && !tag.selfClose
        && !tag.name.isEmpty() && !isVoidElement(tag.name);

    cursor.beginEditBlock();
    cursor.insertBlock();

    if(afterOpenTag)
    {
        // Раскрыть контейнер: закрывающий тег на строке ниже,
        // курсор остается между с отступом глубже
        const QString unit=indent.contains(QLatin1Char('\t'))
            ? QStringLiteral("\t") : QStringLiteral("    ");
        cursor.insertText(indent+unit);
        const int middlePos=cursor.position();
        cursor.insertBlock();
        cursor.insertText(indent+QStringLiteral("</")+tag.name+QStringLiteral(">"));
        cursor.setPosition(middlePos);
    }
    else
    {
        cursor.insertText(indent);
    }

    cursor.endEditBlock();
    setTextCursor(cursor);
}


void HtmlSourceEdit::autoCloseTag(void)
{
    QTextCursor cursor=textCursor();

    if(cursor.hasSelection())
        return;

    // Только что допечатали '>' нового открывающего тега
    TagSpan tag;

    if(!tagEndingAt(cursor.position(), tag))
        return;

    if(tag.special || tag.isClose || tag.selfClose
        || tag.name.isEmpty() || isVoidElement(tag.name))
        return;

    // Дальше уже стоит '>' (допечатка внутри существующей разметки)
    // или свой же закрывающий: не дублировать
    QTextCursor after(document());
    after.setPosition(cursor.position());
    after.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    const QString tail=after.selectedText().trimmed();

    if(tail.startsWith(QLatin1Char('>')))
        return;

    if(tail.startsWith(QStringLiteral("</")+tag.name, Qt::CaseInsensitive))
        return;

    // Откат одной правкой вместе с набранным '>': курсор между тегами
    QTextCursor editCursor(cursor);
    editCursor.joinPreviousEditBlock();
    editCursor.insertText(QStringLiteral("</")+tag.name+QStringLiteral(">"));
    editCursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor,
                            tag.name.length()+3);
    setTextCursor(editCursor);
}


QString HtmlSourceEdit::tagInnerText(QTextDocument *doc, int from, int to)
{
    if(from<0 || to<from)
        return QString();

    QTextCursor sel(doc);
    sel.setPosition(from);
    sel.setPosition(to, QTextCursor::KeepAnchor);

    QString text=sel.selectedText();
    text.replace(QChar::ParagraphSeparator, QLatin1Char(' '));
    text.replace(QChar::LineSeparator, QLatin1Char(' '));

    return text;
}


bool HtmlSourceEdit::tagAt(int pos, TagSpan &tag) const
{
    QTextDocument *doc=document();

    if(pos<0 || pos>doc->characterCount())
        return false;

    // Ближайший '<' слева: между ним и курсором не должно быть '>'
    QTextCursor back(doc);
    back.setPosition(pos);
    QTextCursor open=doc->find(QStringLiteral("<"), back, QTextDocument::FindBackward);

    if(open.isNull())
        return false;

    const int lt=open.selectionStart();
    const QString before=tagInnerText(doc, lt+1, pos);

    if(before.contains(QLatin1Char('>')))
        return false;

    // Конец тега вперед. Незакрытый (набираемый) - не тег
    QTextCursor fwd(doc);
    fwd.setPosition(lt+1);
    QTextCursor close=doc->find(QStringLiteral(">"), fwd);

    if(close.isNull())
        return false;

    const int gt=close.selectionStart();

    if(pos<lt || pos>gt+1)
        return false;

    QString inner=tagInnerText(doc, lt+1, gt).trimmed();

    tag=TagSpan();
    tag.start=lt;
    tag.end=gt+1;

    if(inner.startsWith(QLatin1Char('!')) || inner.startsWith(QLatin1Char('?')))
    {
        tag.special=true;
        return true;
    }

    if(inner.startsWith(QLatin1Char('/')))
    {
        tag.isClose=true;
        inner=inner.mid(1).trimmed();
    }

    static const QRegularExpression nameRx(QStringLiteral("^[A-Za-z][\\w:.-]*"));
    const QRegularExpressionMatch nameMatch=nameRx.match(inner);

    if(!nameMatch.hasMatch())
        return false;

    tag.name=nameMatch.captured(0).toLower();

    const QString rest=inner.mid(tag.name.length()).trimmed();
    tag.selfClose=rest.endsWith(QLatin1Char('/'));

    return true;
}


bool HtmlSourceEdit::tagEndingAt(int pos, TagSpan &tag) const
{
    QTextDocument *doc=document();

    if(pos<1 || pos>doc->characterCount())
        return false;

    if(doc->characterAt(pos-1)!=QLatin1Char('>'))
        return false;

    // '<' назад: между ним и '>' не должно быть ни '>', ни '<'
    QTextCursor back(doc);
    back.setPosition(pos-1);
    QTextCursor open=doc->find(QStringLiteral("<"), back, QTextDocument::FindBackward);

    if(open.isNull())
        return false;

    const int lt=open.selectionStart();
    const QString inner=tagInnerText(doc, lt+1, pos-1).trimmed();

    if(inner.contains(QLatin1Char('>')) || inner.contains(QLatin1Char('<')))
        return false;

    TagSpan parsed;
    parsed.start=lt;
    parsed.end=pos;

    if(inner.startsWith(QLatin1Char('!')) || inner.startsWith(QLatin1Char('?')))
    {
        parsed.special=true;
        tag=parsed;
        return true;
    }

    QString rest=inner;

    if(rest.startsWith(QLatin1Char('/')))
    {
        parsed.isClose=true;
        rest=rest.mid(1).trimmed();
    }

    static const QRegularExpression nameRx(QStringLiteral("^[A-Za-z][\\w:.-]*"));
    const QRegularExpressionMatch nameMatch=nameRx.match(rest);

    if(!nameMatch.hasMatch())
        return false;

    parsed.name=nameMatch.captured(0).toLower();

    const QString tail=rest.mid(parsed.name.length()).trimmed();
    parsed.selfClose=tail.endsWith(QLatin1Char('/'));

    tag=parsed;
    return true;
}


bool HtmlSourceEdit::isVoidElement(const QString &name)
{
    // Пустые элементы HTML: закрывающего не бывает
    static const QSet<QString> voids=QSet<QString>()
        << QStringLiteral("area") << QStringLiteral("base")
        << QStringLiteral("br") << QStringLiteral("col")
        << QStringLiteral("embed") << QStringLiteral("hr")
        << QStringLiteral("img") << QStringLiteral("input")
        << QStringLiteral("link") << QStringLiteral("meta")
        << QStringLiteral("param") << QStringLiteral("source")
        << QStringLiteral("track") << QStringLiteral("wbr");

    return voids.contains(name.toLower());
}


bool HtmlSourceEdit::findMate(const TagSpan &tag, TagSpan &mate) const
{
    static const QRegularExpression tagRx(
        QStringLiteral("<\\s*(/?)\\s*([A-Za-z][\\w:.-]*)[^>]*?(/?)>"));

    const bool forward=!tag.isClose;
    QTextCursor cursor(document());
    cursor.setPosition(forward ? tag.end : tag.start);

    int depth=0;
    int lastPos=forward ? -1 : INT_MAX;

    for(int scanned=0; scanned<maxPairScan; ++scanned)
    {
        QTextCursor found=forward
            ? document()->find(tagRx, cursor)
            : document()->find(tagRx, cursor, QTextDocument::FindBackward);

        if(found.isNull())
            return false;

        const int matchPos=forward ? found.selectionEnd() : found.selectionStart();

        // Стоим на месте - дальше не продвинемся
        if((forward && matchPos<=lastPos) || (!forward && matchPos>=lastPos))
            return false;

        lastPos=matchPos;
        cursor.setPosition(matchPos);

        // Совпадение целиком в выделении: группы берутся повторным
        // матчем по его тексту (у курсора захватов нет)
        const QRegularExpressionMatch groups=tagRx.match(found.selectedText());
        const QString name=groups.captured(2);

        if(name.compare(tag.name, Qt::CaseInsensitive)!=0)
            continue;

        // Совпадение внутри комментария <!-- <div> -->: эвристика окном
        const int windowStart=qMax(0, found.selectionStart()-commentWindow);
        const QString window=tagInnerText(document(), windowStart, found.selectionStart());
        const int opens=window.count(QStringLiteral("<!--"));
        const int closes=window.count(QStringLiteral("-->"));

        if(opens>closes)
            continue;

        const bool close=!groups.captured(1).isEmpty();
        const bool selfClose=!groups.captured(3).isEmpty() || isVoidElement(name);

        if(selfClose)
            continue;

        if(forward)
        {
            if(close)
            {
                if(depth==0)
                {
                    mate.start=found.selectionStart();
                    mate.end=found.selectionEnd();
                    mate.name=name.toLower();
                    mate.isClose=true;
                    return true;
                }

                --depth;
            }
            else
            {
                ++depth;
            }
        }
        else
        {
            if(!close)
            {
                if(depth==0)
                {
                    mate.start=found.selectionStart();
                    mate.end=found.selectionEnd();
                    mate.name=name.toLower();
                    mate.isClose=false;
                    return true;
                }

                --depth;
            }
            else
            {
                ++depth;
            }
        }
    }

    return false;
}


void HtmlSourceEdit::updatePairHighlight(void)
{
    pairSelections.clear();

    TagSpan tag;

    if(tagAt(textCursor().position(), tag)
        && !tag.special && !tag.name.isEmpty()
        && !tag.selfClose && !isVoidElement(tag.name))
    {
        QTextCharFormat pairFormat;
        QTextCharFormat mismatchFormat;
        pairFormat.setBackground(QColor(110, 170, 230, 70));
        mismatchFormat.setBackground(QColor(230, 110, 110, 70));

        TagSpan mate;

        auto addRange=[&](const TagSpan &span, const QTextCharFormat &format)
        {
            QTextCursor cursor(document());
            cursor.setPosition(span.start);
            cursor.setPosition(span.end, QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection selection;
            selection.cursor=cursor;
            selection.format=format;
            pairSelections.append(selection);
        };

        if(findMate(tag, mate))
        {
            addRange(tag, pairFormat);
            addRange(mate, pairFormat);
        }
        else
        {
            // Парного нет - незакрытый тег подсвечивается тревожным
            addRange(tag, mismatchFormat);
        }
    }

    applySelections();
}

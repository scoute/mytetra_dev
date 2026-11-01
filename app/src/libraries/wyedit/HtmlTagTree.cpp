#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QRegularExpression>

#include "HtmlTagTree.h"
#include "libraries/wyedit/HtmlSourceEdit.h"


HtmlTagTree::HtmlTagTree(QWidget *parent) : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setUniformRowHeights(true);
    setMinimumWidth(140);
    setMaximumWidth(320);

    connect(this, &QTreeWidget::itemClicked,
            this, &HtmlTagTree::onItemClicked);
}


HtmlTagTree::~HtmlTagTree()
{
}


void HtmlTagTree::setEditor(HtmlSourceEdit *editor)
{
    if(m_editor!=nullptr)
    {
        disconnect(m_editor, nullptr, this, nullptr);
        disconnect(m_editor->document(), nullptr, this, nullptr);
    }

    m_editor=editor;

    if(m_editor==nullptr)
        return;

    connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
            this, &HtmlTagTree::syncToCursor);
    connect(m_editor->document(), &QTextDocument::contentsChanged,
            this, &HtmlTagTree::rebuild);

    rebuild();
}


QList<QPair<int, int>> HtmlTagTree::commentRanges(const QString &text)
{
    QList<QPair<int, int>> ranges;
    int from=0;

    while(true)
    {
        const int open=text.indexOf(QStringLiteral("<!--"), from);

        if(open<0)
            break;

        int close=text.indexOf(QStringLiteral("-->"), open+4);
        close=(close<0) ? text.length() : close+3;
        ranges.append(qMakePair(open, close));
        from=close;
    }

    return ranges;
}


QString HtmlTagTree::tagLabel(const QString &tagText)
{
    // Имя тега
    static const QRegularExpression nameRx(QStringLiteral("^<\\s*/?\\s*([A-Za-z][\\w:.-]*)"));
    const QRegularExpressionMatch nameMatch=nameRx.match(tagText);

    if(!nameMatch.hasMatch())
        return tagText.left(40);

    QString label=nameMatch.captured(1).toLower();

    // #id и классы для ориентира среди одноименных div
    static const QRegularExpression idRx(QStringLiteral("\\bid\\s*=\\s*\"([^\"]*)\""));
    const QRegularExpressionMatch idMatch=idRx.match(tagText);

    if(idMatch.hasMatch() && !idMatch.captured(1).isEmpty())
        label+=QStringLiteral(" #")+idMatch.captured(1).left(20);

    static const QRegularExpression classRx(QStringLiteral("\\bclass\\s*=\\s*\"([^\"]*)\""));
    const QRegularExpressionMatch classMatch=classRx.match(tagText);

    if(classMatch.hasMatch() && !classMatch.captured(1).isEmpty())
    {
        QString classes=classMatch.captured(1).simplified().left(30);
        classes.replace(QLatin1Char(' '), QLatin1Char('.'));
        label+=QStringLiteral(" .")+classes;
    }

    return label;
}


void HtmlTagTree::rebuild(void)
{
    if(m_editor==nullptr)
        return;

    m_sync=true;
    clear();

    const QString text=m_editor->document()->toPlainText();

    // Позиции в toPlainText совпадают с позициями документа:
    // разделитель блоков там и там один символ
    static const QRegularExpression tagRx(
        QStringLiteral("<\\s*(/?)\\s*([A-Za-z][\\w:.-]*)[^>]*?(/?)>"));

    static const QSet<QString> voids=QSet<QString>()
        << QStringLiteral("area") << QStringLiteral("base")
        << QStringLiteral("br") << QStringLiteral("col")
        << QStringLiteral("embed") << QStringLiteral("hr")
        << QStringLiteral("img") << QStringLiteral("input")
        << QStringLiteral("link") << QStringLiteral("meta")
        << QStringLiteral("param") << QStringLiteral("source")
        << QStringLiteral("track") << QStringLiteral("wbr");

    const QList<QPair<int, int>> comments=commentRanges(text);

    auto insideComment=[&](int pos)
    {
        for(const auto &range : comments)
        {
            if(pos>=range.first && pos<range.second)
                return true;

            if(pos<range.first)
                break;
        }

        return false;
    };

    QList<QTreeWidgetItem *> stack;
    int nodes=0;

    QRegularExpressionMatchIterator it=tagRx.globalMatch(text);

    while(it.hasNext() && nodes<maxNodes)
    {
        const QRegularExpressionMatch match=it.next();
        const int start=match.capturedStart();

        if(insideComment(start))
            continue;

        const QString name=match.captured(2).toLower();
        const bool close=!match.captured(1).isEmpty();
        const bool selfClose=!match.captured(3).isEmpty() || voids.contains(name);

        if(close)
        {
            // Закрываем ближайшее открытое: несовпадения (битый HTML)
            // просто снимают стек до совпавшего, лишнее игнорируется
            while(!stack.isEmpty())
            {
                QTreeWidgetItem *top=stack.takeLast();

                if(top->data(0, Qt::UserRole+1).toString()==name)
                    break;
            }

            continue;
        }

        QTreeWidgetItem *item=new QTreeWidgetItem();
        item->setText(0, tagLabel(match.captured(0)));
        item->setData(0, Qt::UserRole, start);
        item->setData(0, Qt::UserRole+1, name);
        item->setToolTip(0, match.captured(0).left(200));

        if(stack.isEmpty())
            addTopLevelItem(item);
        else
            stack.last()->addChild(item);

        ++nodes;

        if(!selfClose)
            stack.append(item);
    }

    expandAll();
    m_sync=false;

    syncToCursor();
}


QTreeWidgetItem *HtmlTagTree::itemForPosition(int pos) const
{
    QTreeWidgetItem *best=nullptr;
    int bestStart=-1;

    // Самый глубокий узел, содержащий позицию: обход с конца,
    // у вложенных старт не раньше родительского
    const QList<QTreeWidgetItem *> all=findItems(
        QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);

    for(QTreeWidgetItem *item : all)
    {
        const int start=item->data(0, Qt::UserRole).toInt();

        if(start<=pos && start>bestStart)
        {
            bestStart=start;
            best=item;
        }
    }

    return best;
}


void HtmlTagTree::syncToCursor(void)
{
    if(m_sync || m_editor==nullptr)
        return;

    m_sync=true;

    QTreeWidgetItem *item=itemForPosition(m_editor->textCursor().position());

    if(item!=nullptr)
    {
        setCurrentItem(item);
        scrollToItem(item);
    }
    else
    {
        clearSelection();
    }

    m_sync=false;
}


void HtmlTagTree::jumpToItem(QTreeWidgetItem *item)
{
    if(m_editor==nullptr || item==nullptr)
        return;

    const int pos=item->data(0, Qt::UserRole).toInt();

    QTextCursor cursor(m_editor->document());
    cursor.setPosition(pos);
    m_editor->setTextCursor(cursor);
    m_editor->ensureCursorVisible();
    m_editor->setFocus(Qt::MouseFocusReason);
}


void HtmlTagTree::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    if(m_sync)
        return;

    jumpToItem(item);
}

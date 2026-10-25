#include <QTimer>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>

#include "EditorStructurePanel.h"
#include "EditorTextArea.h"


EditorStructurePanel::EditorStructurePanel(QWidget *parent) : QListWidget(parent)
{
    setObjectName("editor_structure_panel");
    setVisible(false); // Показывается кнопкой тулбара
    setMinimumWidth(140);
    setMaximumWidth(320);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_timer=new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(300); // Дебаунс: перестройка не на каждую клавишу
    connect(m_timer, &QTimer::timeout, this, &EditorStructurePanel::onRebuildTimeout);

    connect(this, &QListWidget::itemClicked, this, &EditorStructurePanel::onJump);
}


void EditorStructurePanel::setTextArea(EditorTextArea *area)
{
    m_area=area;

    if(m_area==nullptr)
        return;

    connect(m_area->document(), &QTextDocument::contentsChanged,
            this,                &EditorStructurePanel::onDocumentChanged);
    connect(m_area, &QTextEdit::cursorPositionChanged,
            this,    &EditorStructurePanel::onCursorMoved);

    rebuild();
}


void EditorStructurePanel::rebuild(void)
{
    if(m_area==nullptr)
        return;

    m_sync=true;
    clear();

    QTextDocument *doc=m_area->document();
    for(QTextBlock block=doc->begin(); block.isValid(); block=block.next())
    {
        const int level=block.blockFormat().headingLevel();
        if(level<1 || level>3)
            continue;

        QString text=block.text().simplified();
        if(text.length()>100)
            text=text.left(100)+QStringLiteral("…");
        if(text.isEmpty())
            text=tr("(empty heading)");

        // Вложенность видна отступом: H1 без отступа, H2 и H3 глубже
        QListWidgetItem *item=new QListWidgetItem(QString((level-1)*2, QLatin1Char(' '))+text, this);
        item->setData(Qt::UserRole, block.position());
        item->setToolTip(block.text().simplified());
    }

    if(count()==0)
    {
        QListWidgetItem *item=new QListWidgetItem(tr("No H1-H3 headings"), this);
        item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
    }

    onCursorMoved();
    m_sync=false;
}


void EditorStructurePanel::onDocumentChanged(void)
{
    m_timer->start();
}


void EditorStructurePanel::onRebuildTimeout(void)
{
    // Перестраивается только видимая панель: скрытая никому не нужна,
    // при показе Editor дернет rebuild сам
    if(isVisible())
        rebuild();
}


void EditorStructurePanel::onJump(QListWidgetItem *item)
{
    if(m_sync || m_area==nullptr || item==nullptr)
        return;

    bool ok=false;
    const int pos=item->data(Qt::UserRole).toInt(&ok);
    if(!ok)
        return;

    QTextCursor cursor(m_area->document());
    cursor.setPosition(pos);
    m_area->setTextCursor(cursor);
    m_area->ensureCursorVisible();
    m_area->setFocus(Qt::MouseFocusReason);
}


void EditorStructurePanel::onCursorMoved(void)
{
    if(m_sync || m_area==nullptr || count()==0)
        return;

    const int row=currentHeadingRow(m_area->textCursor().position());

    m_sync=true;
    if(row>=0)
        setCurrentRow(row);
    else
        clearSelection();
    m_sync=false;
}


int EditorStructurePanel::currentHeadingRow(int cursorPos) const
{
    int row=-1;

    for(int i=0; i<count(); ++i)
    {
        bool ok=false;
        const int pos=item(i)->data(Qt::UserRole).toInt(&ok);
        if(!ok)
            return -1; // Плейсхолдер: заголовков нет

        if(pos<=cursorPos)
            row=i;
        else
            break;
    }

    return row;
}

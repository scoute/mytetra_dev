#include <QTimer>
#include <QLabel>
#include <QListWidget>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>

#include "EditorStructurePanel.h"
#include "EditorTextArea.h"


EditorStructurePanel::EditorStructurePanel(QWidget *parent) : QWidget(parent)
{
    setObjectName("editor_structure_panel");
    setVisible(false); // Показывается кнопкой тулбара
    setMinimumWidth(140);
    setMaximumWidth(320);

    QFont titleFont=this->font();
    titleFont.setBold(true);

    QLabel *title=new QLabel(tr("Outline"), this);
    title->setFont(titleFont);
    m_title=title;

    m_list=new QListWidget(this);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setUniformItemSizes(true);

    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addWidget(title);
    layout->addWidget(m_list, 1);

    m_timer=new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(300); // Дебаунс: перестройка не на каждую клавишу
    connect(m_timer, &QTimer::timeout, this, &EditorStructurePanel::onRebuildTimeout);

    connect(m_list, &QListWidget::itemClicked, this, &EditorStructurePanel::onJump);
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


int EditorStructurePanel::count(void) const
{
    return m_list ? m_list->count() : 0;
}


QListWidgetItem *EditorStructurePanel::item(int row) const
{
    return m_list ? m_list->item(row) : nullptr;
}


int EditorStructurePanel::currentRow(void) const
{
    return m_list ? m_list->currentRow() : -1;
}


void EditorStructurePanel::rebuild(void)
{
    if(m_area==nullptr || m_list==nullptr)
        return;

    m_sync=true;
    m_list->clear();

    QTextDocument *doc=m_area->document();
    for(QTextBlock block=doc->begin(); block.isValid(); block=block.next())
    {
        const int level=block.blockFormat().headingLevel();
        if(level<1 || level>3)
            continue;

        QString text=block.text().simplified();
        if(text.isEmpty())
            text=tr("(empty heading)");
        if(text.length()>100)
            text=text.left(100)+QStringLiteral("…");

        // Вложенность видна отступом: H1 без отступа, H2 и H3 глубже
        QListWidgetItem *item=new QListWidgetItem(QString((level-1)*2, QLatin1Char(' '))+text, m_list);
        item->setData(Qt::UserRole, block.position());
        item->setToolTip(block.text().simplified());
    }

    if(m_list->count()==0)
    {
        QListWidgetItem *item=new QListWidgetItem(tr("No H1-H3 headings"), m_list);
        item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
    }

    onCursorMoved();
    m_sync=false;

    adjustTitleIndent();
}


void EditorStructurePanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    adjustTitleIndent();

    // Замер в showEvent может застать неготовую геометрию (раскладка
    // применяется уже после показа): повтор уже по факту раскладки
    QTimer::singleShot(0, this, &EditorStructurePanel::adjustTitleIndent);
}


void EditorStructurePanel::adjustTitleIndent(void)
{
    if(m_title==nullptr || m_list==nullptr)
        return;

    // Без готовой геометрии вьюпорта замер врал бы: ждем раскладку
    if(m_list->viewport()->width()<=0 || m_list->viewport()->height()<=0)
        return;

    int indent=2;
    if(m_list->count()>0)
    {
        // Первая строка без отступа-вложенности: к ее тексту и ровняемся
        for(int i=0; i<m_list->count(); ++i)
        {
            QListWidgetItem *first=m_list->item(i);
            if(first==nullptr || first->text().startsWith(QLatin1Char(' ')))
                continue;

            const QRect r=m_list->visualItemRect(first);
            if(r.isValid() && r.width()>0)
            {
                const int itemX=m_list->viewport()->mapTo(this, r.topLeft()).x();
                indent=qMax(0, itemX-m_title->x());
            }
            break;
        }
    }
    m_title->setIndent(indent);
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
    if(m_sync || m_area==nullptr || m_list==nullptr || m_list->count()==0)
        return;

    const int row=currentHeadingRow(m_area->textCursor().position());

    m_sync=true;
    if(row>=0)
        m_list->setCurrentRow(row);
    else
        m_list->clearSelection();
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

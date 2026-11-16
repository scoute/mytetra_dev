#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include "SyncPreviewDialog.h"
#include "libraries/sync/SyncDiff.h"

using SyncPreview::Action;
using SyncPreview::Category;
using SyncPreview::Decisions;
using SyncPreview::Item;


SyncPreviewDialog::SyncPreviewDialog(const SyncPreview::Preview &preview,
                                     const Decisions &initial,
                                     const TextFetcher &ownText,
                                     const TextFetcher &otherText,
                                     QWidget *parent)
    : QDialog(parent),
      m_preview(preview),
      m_decisions(initial),
      m_ownText(ownText),
      m_otherText(otherText)
{
    setWindowTitle(tr("Дерево изменений синхронизации"));
    resize(840, 640);

    QVBoxLayout *layout=new QVBoxLayout(this);

    // --- заголовок: статус разбора + "Запомнить" ---
    QHBoxLayout *header=new QHBoxLayout();
    m_statusLabel=new QLabel(this);
    QFont statusFont=m_statusLabel->font();
    statusFont.setBold(true);
    statusFont.setPointSize(statusFont.pointSize()+2);
    m_statusLabel->setFont(statusFont);
    header->addWidget(m_statusLabel, 1);
    m_saveDefaults=new QCheckBox(tr("Запомнить"), this);
    m_saveDefaults->setToolTip(
        tr("Сохранить выбранные действия как умолчание для будущих синков"));
    header->addWidget(m_saveDefaults);
    layout->addLayout(header);

    // --- вкладки-направления ---
    m_tabs=new QTabBar(this);
    m_tabs->setExpanding(true);
    buildTabs();
    connect(m_tabs, &QTabBar::currentChanged,
            this, &SyncPreviewDialog::onTabChanged);
    layout->addWidget(m_tabs);

    // --- строка инструментов: действия, дерево, помощь ---
    QHBoxLayout *toolbar=new QHBoxLayout();
    m_actionsButton=new QToolButton(this);
    m_actionsButton->setText(tr("Действия ▾"));
    m_actionsButton->setPopupMode(QToolButton::InstantPopup);
    buildActionsMenu();
    toolbar->addWidget(m_actionsButton);

    m_wholeTreeCheck=new QCheckBox(tr("Всё дерево"), this);
    m_wholeTreeCheck->setChecked(m_wholeTree);
    m_wholeTreeCheck->setToolTip(
        tr("Группировать элементы по веткам. Выключено — плоский список по категориям"));
    connect(m_wholeTreeCheck, &QCheckBox::toggled,
            this, &SyncPreviewDialog::onWholeTreeToggled);
    toolbar->addWidget(m_wholeTreeCheck);

    toolbar->addStretch(1);

    QToolButton *helpButton=new QToolButton(this);
    helpButton->setText(QStringLiteral("ⓘ"));
    helpButton->setToolTip(tr("Обозначения и вкладки"));
    connect(helpButton, &QToolButton::clicked,
            this, &SyncPreviewDialog::onHelpClicked);
    toolbar->addWidget(helpButton);
    layout->addLayout(toolbar);

    // --- дерево элементов ---
    m_itemsTree=new QTreeWidget(this);
    m_itemsTree->setColumnCount(3);
    m_itemsTree->setHeaderLabels(QStringList()
                                 << tr("Элемент") << tr("Действие") << tr("Инфо"));
    m_itemsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_itemsTree->setRootIsDecorated(true);
    layout->addWidget(m_itemsTree, 3);
    buildItems();

    connect(m_itemsTree, &QTreeWidget::itemSelectionChanged,
            this, &SyncPreviewDialog::onItemSelectionChanged);

    // --- панель ветки + diff ---
    m_branchLabel=new QLabel(this);
    QFont branchFont=m_branchLabel->font();
    branchFont.setBold(true);
    branchFont.setPointSize(branchFont.pointSize()+1);
    m_branchLabel->setFont(branchFont);
    m_branchLabel->setWordWrap(true);
    layout->addWidget(m_branchLabel);

    m_diffView=new QPlainTextEdit(this);
    m_diffView->setReadOnly(true);
    m_diffView->setPlaceholderText(
        tr("Выберите измененную запись, чтобы увидеть diff."));
    layout->addWidget(m_diffView, 2);

    // --- кнопки ---
    QDialogButtonBox *buttons=new QDialogButtonBox(this);
    QPushButton *applyButton=buttons->addButton(
        tr("Применить"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Отмена"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    applyButton->setDefault(true);

    refreshCounter();
}


bool SyncPreviewDialog::itemVisibleOnTab(const Item &item, int tab) const
{
    if(tab==0)
        return item.fromLocal;
    if(tab==1)
        return !item.fromLocal;

    // Обоюдные правки: элемент есть с обеих сторон (правка или
    // переименование, а не одностороннее создание/удаление)
    return item.category==Category::Edit || item.category==Category::Rename;
}


QString SyncPreviewDialog::tabName(int tab) const
{
    if(tab==0)
        return tr("↑ Я → сервер");
    if(tab==1)
        return tr("↓ Сервер → я");

    return tr("⇅ Обоюдные правки");
}


void SyncPreviewDialog::buildTabs(void)
{
    int up=0, down=0, both=0;

    for(const Item &item : m_preview.items)
    {
        if(item.fromLocal)
            up++;
        else
            down++;

        if(item.category==Category::Edit || item.category==Category::Rename)
            both++;
    }

    m_tabs->addTab(tr("↑ Я → сервер")+
        (up>0 ? QStringLiteral(" · %1").arg(up) : QStringLiteral(" · —")));
    m_tabs->addTab(tr("↓ Сервер → я")+
        (down>0 ? QStringLiteral(" · +%1").arg(down) : QStringLiteral(" · —")));
    m_tabs->addTab(tr("⇅ Обоюдные правки")+
        (both>0 ? QStringLiteral(" · %1").arg(both) : QStringLiteral(" · —")));
    m_tabs->setCurrentIndex(0);
    m_tab=0;
}


void SyncPreviewDialog::buildActionsMenu(void)
{
    QMenu *menu=new QMenu(m_actionsButton);

    for(int i=0; i<5; ++i)
    {
        const Category category=static_cast<Category>(i);
        const int count=m_preview.count(category);

        QMenu *sub=menu->addMenu(
            QStringLiteral("%1 (%2)")
                .arg(SyncPreview::categoryName(category)).arg(count));
        sub->setEnabled(count>0);

        for(int a=0; a<5; ++a)
        {
            const Action action=static_cast<Action>(a);
            QAction *entry=sub->addAction(SyncPreview::actionName(action));
            entry->setCheckable(true);
            entry->setChecked(m_decisions.category.value(category,
                Action::Confirm)==action);
            entry->setProperty("category", i);
            entry->setProperty("action", a);

            connect(entry, &QAction::triggered,
                    this, &SyncPreviewDialog::onActionsMenuTriggered);
        }
    }

    m_actionsButton->setMenu(menu);
}


void SyncPreviewDialog::onActionsMenuTriggered(void)
{
    QAction *entry=qobject_cast<QAction *>(sender());

    if(entry==nullptr)
        return;

    const Category category=
        static_cast<Category>(entry->property("category").toInt());
    const Action action=
        static_cast<Action>(entry->property("action").toInt());

    m_decisions.category[category]=action;

    // Точечные действия, которые пользователь не менял, следуют категории
    buildItems();
    buildActionsMenu();
    refreshCounter();
}


QString SyncPreviewDialog::markerFor(const Item &item)
{
    switch(item.category)
    {
    case Category::Create: return QStringLiteral("+");
    case Category::Rename: return QStringLiteral("≡");
    case Category::Edit:   return QStringLiteral("~");
    default:               return QStringLiteral("−");
    }
}


QString SyncPreviewDialog::directionFor(const Item &item)
{
    return item.fromLocal ? tr("↑ наше") : tr("↓ ихнее");
}


void SyncPreviewDialog::refreshRowText(QTreeWidgetItem *row)
{
    if(row==nullptr)
        return;

    const int itemIndex=row->data(0, Qt::UserRole).toInt();

    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return;

    const Item &item=m_preview.items.at(itemIndex);
    const bool decided=m_decisions.forItem(item)!=Action::Confirm;

    row->setText(0, QStringLiteral("%1 %2 %3%4")
        .arg(markerFor(item), directionFor(item),
             item.name.isEmpty() ? tr("(без имени)") : item.name,
             decided ? QStringLiteral(" ✓") : QString()));
}


void SyncPreviewDialog::buildItems(void)
{
    m_itemsTree->clear();

    // Элементы текущей вкладки (порядок плана стабильный)
    QList<int> visible;

    for(int i=0; i<m_preview.items.size(); ++i)
        if(itemVisibleOnTab(m_preview.items.at(i), m_tab))
            visible << i;

    if(visible.isEmpty())
    {
        QTreeWidgetItem *empty=new QTreeWidgetItem(m_itemsTree);
        empty->setText(0, tr("В этой вкладке нет изменений"));
        empty->setDisabled(true);
        empty->setFirstColumnSpanned(true);
        m_branchLabel->setText(
            tr("В этой вкладке нет элементов для разбора"));
        m_diffView->clear();
        return;
    }

    if(!m_wholeTree)
    {
        // Плоский список по категориям (прежний вид)
        for(int i=0; i<5; ++i)
        {
            const Category category=static_cast<Category>(i);
            bool has=false;

            for(int index : visible)
                if(m_preview.items.at(index).category==category)
                {
                    has=true;
                    break;
                }

            if(!has)
                continue;

            QTreeWidgetItem *group=new QTreeWidgetItem(m_itemsTree);
            group->setText(0, SyncPreview::categoryName(category));
            group->setData(0, Qt::UserRole, -1);
            group->setFirstColumnSpanned(true);
            QFont font=group->font(0);
            font.setBold(true);
            group->setFont(0, font);

            for(int index : visible)
            {
                const Item &item=m_preview.items.at(index);

                if(item.category!=category)
                    continue;

                QTreeWidgetItem *row=new QTreeWidgetItem(group);
                row->setData(0, Qt::UserRole, index);
                refreshRowText(row);

                QComboBox *combo=new QComboBox(m_itemsTree);

                for(int a=0; a<5; ++a)
                    combo->addItem(SyncPreview::actionName(static_cast<Action>(a)));

                combo->setCurrentIndex(static_cast<int>(m_decisions.forItem(item)));
                combo->setProperty("itemIndex", index);

                connect(combo,
                    static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    this, &SyncPreviewDialog::onItemActionChanged);

                m_itemsTree->setItemWidget(row, 1, combo);
                row->setText(2, item.isBranch ? tr("ветка") : tr("запись"));
            }

            group->setExpanded(true);
        }

        for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
        {
            const int itemIndex=(*it)->data(0, Qt::UserRole).toInt();

            if(itemIndex>=0)
            {
                m_itemsTree->setCurrentItem(*it);
                break;
            }
        }

        return;
    }

    // Дерево по веткам: узлы-ветки (свое комбо, если ветка тоже в плане),
    // записи — дети своих веток; сироты и верхнеуровневые — в корне
    QMap<QString, QTreeWidgetItem *> branchNodes;

    auto ensureBranchNode=[&](const QString &branchKey,
                              const QString &branchTitle,
                              bool dimmed) -> QTreeWidgetItem * {
        if(branchNodes.contains(branchKey))
            return branchNodes.value(branchKey);

        QTreeWidgetItem *node=new QTreeWidgetItem(m_itemsTree);
        node->setText(0, branchTitle);
        node->setData(0, Qt::UserRole, -1);
        node->setData(0, Qt::UserRole+1, branchKey);

        if(dimmed)
            node->setForeground(0, m_itemsTree->palette().color(
                QPalette::Disabled, QPalette::WindowText));

        branchNodes.insert(branchKey, node);
        return node;
    };

    // Сначала узлы для веток из плана (у них может быть свое действие)
    for(int index : visible)
    {
        const Item &item=m_preview.items.at(index);

        if(!item.isBranch)
            continue;

        QTreeWidgetItem *node=ensureBranchNode(
            item.id,
            item.name.isEmpty() ? tr("(без имени)") : item.name,
            false);
        node->setData(0, Qt::UserRole, index);
        node->setData(0, Qt::ForegroundRole, QVariant());
        refreshRowText(node);

        QComboBox *combo=new QComboBox(m_itemsTree);

        for(int a=0; a<5; ++a)
            combo->addItem(SyncPreview::actionName(static_cast<Action>(a)));

        combo->setCurrentIndex(static_cast<int>(m_decisions.forItem(item)));
        combo->setProperty("itemIndex", index);

        connect(combo,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &SyncPreviewDialog::onItemActionChanged);

        m_itemsTree->setItemWidget(node, 1, combo);
        node->setText(2, directionFor(item));
    }

    // Затем записи — под ветки; ветки — под родителей
    for(int index : visible)
    {
        const Item &item=m_preview.items.at(index);

        if(item.isBranch)
        {
            // Ветку — под родителя, если тот тоже в дереве
            if(!item.parentId.isEmpty() && branchNodes.contains(item.parentId))
            {
                QTreeWidgetItem *node=branchNodes.value(item.id);
                const int topRow=m_itemsTree->indexOfTopLevelItem(node);

                if(topRow>=0)
                {
                    m_itemsTree->takeTopLevelItem(topRow);
                    branchNodes.value(item.parentId)->addChild(node);
                }
            }

            continue;
        }

        QTreeWidgetItem *parent=nullptr;

        if(!item.parentId.isEmpty())
        {
            ensureBranchNode(item.parentId,
                item.parentName.isEmpty() ? tr("(без имени)") : item.parentName,
                true);
            parent=branchNodes.value(item.parentId);
        }

        QTreeWidgetItem *row=(parent!=nullptr)
            ? new QTreeWidgetItem(parent)
            : new QTreeWidgetItem(m_itemsTree);
        row->setData(0, Qt::UserRole, index);
        refreshRowText(row);

        QComboBox *combo=new QComboBox(m_itemsTree);

        for(int a=0; a<5; ++a)
            combo->addItem(SyncPreview::actionName(static_cast<Action>(a)));

        combo->setCurrentIndex(static_cast<int>(m_decisions.forItem(item)));
        combo->setProperty("itemIndex", index);

        connect(combo,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &SyncPreviewDialog::onItemActionChanged);

        m_itemsTree->setItemWidget(row, 1, combo);
        row->setText(2, directionFor(item));
    }

    for(auto it=branchNodes.constBegin(); it!=branchNodes.constEnd(); ++it)
        it.value()->setExpanded(true);

    if(m_itemsTree->topLevelItemCount()>0)
        m_itemsTree->setCurrentItem(m_itemsTree->topLevelItem(0));
}


void SyncPreviewDialog::refreshCounter(void)
{
    int decided=0;

    for(const Item &item : m_preview.items)
        if(m_decisions.forItem(item)!=Action::Confirm)
            decided++;

    const int total=m_preview.items.size();

    if(total>0 && decided==total)
        m_statusLabel->setText(tr("✓ Всё разобрано (%1/%2)").arg(decided).arg(total));
    else
        m_statusLabel->setText(tr("Разобрано (%1/%2)").arg(decided).arg(total));
}


void SyncPreviewDialog::onTabChanged(int index)
{
    m_tab=index;
    buildItems();
}


void SyncPreviewDialog::onWholeTreeToggled(bool checked)
{
    m_wholeTree=checked;
    buildItems();
}


void SyncPreviewDialog::onItemActionChanged(int index)
{
    QComboBox *combo=qobject_cast<QComboBox *>(sender());

    if(combo==nullptr)
        return;

    const int itemIndex=combo->property("itemIndex").toInt();

    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return;

    m_decisions.byId[m_preview.items.at(itemIndex).id]=
        static_cast<Action>(index);

    // Обновить галочку ✓ в строке без пересборки дерева
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
        if((*it)->data(0, Qt::UserRole).toInt()==itemIndex)
            refreshRowText(*it);

    refreshCounter();
}


void SyncPreviewDialog::onItemSelectionChanged(void)
{
    const QList<QTreeWidgetItem *> selected=m_itemsTree->selectedItems();

    if(selected.isEmpty())
        return;

    updateBranchPanel(selected.first());
}


void SyncPreviewDialog::updateBranchPanel(QTreeWidgetItem *row)
{
    if(row==nullptr)
        return;

    // Ключ ветки: у записи — parentId, у ветки — свой id
    QString branchKey=row->data(0, Qt::UserRole+1).toString();
    QString branchTitle;
    const Item *current=nullptr;

    const int itemIndex=row->data(0, Qt::UserRole).toInt();

    if(itemIndex>=0 && itemIndex<m_preview.items.size())
    {
        current=&m_preview.items.at(itemIndex);

        if(current->isBranch)
        {
            branchKey=current->id;
            branchTitle=current->name;
        }
        else
        {
            branchKey=current->parentId;
            branchTitle=current->parentName;
        }
    }
    else if(!branchKey.isEmpty())
    {
        branchTitle=row->text(0);
    }

    if(branchTitle.isEmpty())
        branchTitle=tr("Верхний уровень");

    // Сколько элементов текущей вкладки в этой ветке
    int inBranch=0;

    for(int i=0; i<m_preview.items.size(); ++i)
    {
        const Item &item=m_preview.items.at(i);

        if(!itemVisibleOnTab(item, m_tab))
            continue;

        const QString key=item.isBranch ? item.id : item.parentId;

        if(key==branchKey)
            inBranch++;
    }

    m_branchLabel->setText(tr("Ветка «%1» · %2 · %3")
        .arg(branchTitle, tabName(m_tab)).arg(inBranch));

    showDiffFor(current);
}


void SyncPreviewDialog::showDiffFor(const Item *item)
{
    if(item==nullptr || item->category!=Category::Edit || item->isBranch)
    {
        m_diffView->setPlainText(
            tr("Выберите измененную запись, чтобы увидеть diff."));
        return;
    }

    const QString own=m_ownText ? m_ownText(*item) : QString();
    const QString other=m_otherText ? m_otherText(*item) : QString();

    if(other.isEmpty())
    {
        m_diffView->setPlainText(tr("Текст другой стороны недоступен."));
        return;
    }

    // Для наших правок: ихняя версия -> наша; для ихних: наша -> ихняя
    const QString before=item->fromLocal ? other : own;
    const QString after=item->fromLocal ? own : other;

    const QList<SyncDiff::Line> lines=SyncDiff::diff(before, after);

    QStringList out;
    out << tr("=== %1 (другая сторона) -> %2 (это устройство) ===")
               .arg(tr("старое"), tr("новое"));

    for(const SyncDiff::Line &line : lines)
    {
        switch(line.kind)
        {
        case SyncDiff::Line::Equal:  out << QStringLiteral("  ")+line.text; break;
        case SyncDiff::Line::Removed: out << QStringLiteral("- ")+line.text; break;
        case SyncDiff::Line::Added:  out << QStringLiteral("+ ")+line.text; break;
        }
    }

    out << SyncDiff::summary(lines);

    m_diffView->setPlainText(out.join(QLatin1Char('\n')));
}


void SyncPreviewDialog::onHelpClicked(void)
{
    showHelp();
}


void SyncPreviewDialog::showHelp(void)
{
    QDialog help(this);
    help.setWindowTitle(tr("Обозначения"));
    help.resize(520, 480);

    QVBoxLayout *layout=new QVBoxLayout(&help);

    QPlainTextEdit *text=new QPlainTextEdit(&help);
    text->setReadOnly(true);
    text->setPlainText(
        tr("Обозначения\n"
           "+ — создано: новая запись или ветка\n"
           "≡ — переименовано: изменилось имя или свойства\n"
           "~ — правка текста\n"
           "− — удалено\n"
           "↑ — наше едет наружу\n"
           "↓ — ихнее приезжает к нам\n"
           "✓ — решено: действие выбрано\n"
           "\n"
           "Вкладки\n"
           "↑ Я → сервер: мои изменения уедут наружу\n"
           "↓ Сервер → я: ихние изменения приедут к нам\n"
           "⇅ Обоюдные правки: элемент есть с обеих сторон "
           "(правка или переименование с двух сторон)\n"
           "\n"
           "Кнопки и галочки\n"
           "«Запомнить» — сохранить выбранные действия как умолчание "
           "для будущих синков\n"
           "«Всё дерево» — группировать элементы по веткам. "
           "Выключено — плоский список по категориям\n"
           "«Действия ▾» — действия по умолчанию для каждой категории\n"
           "«Применить» — выполнить синк с выбранными действиями, "
           "«Отмена» — прервать, расхождения останутся"));
    layout->addWidget(text);

    QDialogButtonBox *buttons=new QDialogButtonBox(
        QDialogButtonBox::Close, Qt::Horizontal, &help);
    connect(buttons, &QDialogButtonBox::rejected, &help, &QDialog::accept);
    layout->addWidget(buttons);

    help.exec();
}


Decisions SyncPreviewDialog::decisions(void) const
{
    return m_decisions;
}


bool SyncPreviewDialog::wantSaveDefaults(void) const
{
    return m_saveDefaults->isChecked();
}

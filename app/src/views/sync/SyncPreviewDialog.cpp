#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>

#include "SyncPreviewDialog.h"
#include "libraries/sync/SyncDiff.h"

using SyncPreview::Action;
using SyncPreview::Category;
using SyncPreview::Danger;
using SyncPreview::Decisions;
using SyncPreview::Item;

// Опасно ли действие для КОНКРЕТНОГО элемента: красным светим удаление
// здесь/на сервере и затирание полной версии пустой. Воскрешение и пропуск
// безопасны всегда; запланированное удаление едет как есть — тоже красное
static bool dangerOf(const Item &item, Action action,
                     const QMap<QString, Danger> &dangers)
{
    const Danger danger=dangers.value(item.id, Danger::None);

    switch(action)
    {
    case Action::TakeServer:
        // воскрешение моего удаления — спасение, не потеря
        if(item.category==Category::MyDelete)
            return false;
        // моя правка сносится их версией (кроме спасения полной)
        if(item.category==Category::Edit)
            return danger!=Danger::MyEmpty;
        return false;
    case Action::ForceMine:
        // воскрешение моей копии наверх — спасение
        if(item.category==Category::TheirDelete)
            return false;
        // их правка сносится моей версией (кроме спасения полной)
        if(item.category==Category::Edit)
            return danger!=Danger::TheirEmpty;
        return false;
    case Action::Apply:
    case Action::Confirm:
        return item.category==Category::MyDelete
            || item.category==Category::TheirDelete;
    case Action::Skip:
        return false;
    }

    return false;
}

// Чем кончится "Применить/Подтвердить" для этого элемента
static QString plannedOutcome(const Item &item)
{
    switch(item.category)
    {
    case Category::Create:
        return item.fromLocal ? QObject::tr("Новое уедет на сервер")
                              : QObject::tr("Новое приедет с сервера");
    case Category::Edit:
        return item.fromLocal ? QObject::tr("Моя правка уедет на сервер")
                              : QObject::tr("Их правка приедет ко мне");
    case Category::Rename:
        return item.fromLocal ? QObject::tr("Моё переименование уедет наверх")
                              : QObject::tr("Их переименование приедет ко мне");
    case Category::MyDelete:
        return QObject::tr("Удаление уедет на сервер");
    case Category::TheirDelete:
        return QObject::tr("Удалится и у меня тоже");
    }

    return QString();
}

static QString takeServerSub(const Item &item, Danger danger)
{
    switch(item.category)
    {
    case Category::Create:
        return QObject::tr("Не отправлять моё (останется только у меня)");
    case Category::Edit:
        return danger==Danger::MyEmpty
            ? QObject::tr("Взять их полную версию (моя почти пустая)")
            : QObject::tr("Отменить мою правку, взять их версию");
    case Category::Rename:
        return QObject::tr("Отменить моё переименование");
    case Category::MyDelete:
        return QObject::tr("Отменить моё удаление (воскресить с сервера)");
    default:
        return QObject::tr("Восстановить с сервера");
    }
}

static QString forceMineSub(const Item &item, Danger danger)
{
    switch(item.category)
    {
    case Category::Create:
        return QObject::tr("Не скачивать (останется только у них)");
    case Category::Edit:
        return danger==Danger::TheirEmpty
            ? QObject::tr("Оставить мою полную версию (их почти пустая)")
            : QObject::tr("Оставить мою версию, их не брать");
    case Category::Rename:
        return QObject::tr("Отменить их переименование");
    case Category::TheirDelete:
        return QObject::tr("Не удалять у меня (воскресить мою копию наверх)");
    default:
        return QObject::tr("Настоять на моей версии");
    }
}

// Цвета бейджей — как в мобильной версии
static uint categoryColor(Category category)
{
    switch(category)
    {
    case Category::Create:      return 0xFF66BB6A;
    case Category::Rename:      return 0xFF42A5F5;
    case Category::Edit:        return 0xFFFFB74D;
    case Category::MyDelete:    return 0xFFEF5350;
    case Category::TheirDelete: return 0xFFCE93D8;
    }

    return 0xFFB0B0B0;
}


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
    resize(880, 660);

    // Опасность бития: почти-пустое затирает содержательное.
    // Меряем заранее (тексты обеих сторон), как в мобильной версии
    QApplication::setOverrideCursor(Qt::WaitCursor);

    for(const Item &item : m_preview.items)
    {
        if(item.category!=Category::Edit || item.isBranch)
            continue;

        const QString own=m_ownText ? m_ownText(item) : QString();
        const QString other=m_otherText ? m_otherText(item) : QString();
        const Danger danger=SyncPreview::dangerFor(
            item,
            SyncPreview::isNearEmpty(own, item.ownAttaches),
            SyncPreview::isNearEmpty(other, item.otherAttaches));

        if(danger!=Danger::None)
        {
            m_dangers.insert(item.id, danger);
            // Авто-дефолт защиты: оставить полную версию
            m_decisions.byId[item.id]=(danger==Danger::MyEmpty)
                ? Action::TakeServer : Action::ForceMine;
        }
    }

    QApplication::restoreOverrideCursor();

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

    // --- предупреждение о битии ---
    if(!m_dangers.isEmpty())
    {
        QLabel *warn=new QLabel(tr("⚠ %1: почти-пустой текст затирает содержательный — дефолт «полная версия»")
            .arg(m_dangers.size()), this);
        QFont warnFont=warn->font();
        warnFont.setBold(true);
        warn->setFont(warnFont);
        warn->setStyleSheet(QStringLiteral("color: #FF8A80;"));
        warn->setWordWrap(true);
        layout->addWidget(warn);
    }

    // --- вкладки-направления с git-статистикой ---
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
        tr("Показать элементы всех вкладок сразу (чужие — бледным контекстом). "
           "Выключено — только дерево текущей вкладки"));
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

    // --- панель: заголовок + поединок + diff ---
    m_branchLabel=new QLabel(this);
    QFont branchFont=m_branchLabel->font();
    branchFont.setBold(true);
    branchFont.setPointSize(branchFont.pointSize()+1);
    m_branchLabel->setFont(branchFont);
    m_branchLabel->setWordWrap(true);
    layout->addWidget(m_branchLabel);

    m_duelRow=new QWidget(this);
    QHBoxLayout *duelLayout=new QHBoxLayout(m_duelRow);
    duelLayout->setContentsMargins(0, 0, 0, 0);
    m_mineButton=new QPushButton(tr("↑ Наша"), m_duelRow);
    m_theirsButton=new QPushButton(tr("↓ Ихняя"), m_duelRow);
    m_duelHint=new QLabel(m_duelRow);
    m_duelHint->setWordWrap(true);
    duelLayout->addWidget(m_mineButton);
    duelLayout->addWidget(m_theirsButton);
    duelLayout->addWidget(m_duelHint, 1);
    connect(m_mineButton, &QPushButton::clicked,
            this, &SyncPreviewDialog::onDuelMine);
    connect(m_theirsButton, &QPushButton::clicked,
            this, &SyncPreviewDialog::onDuelTheirs);
    layout->addWidget(m_duelRow);

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

    refreshAll();
}


bool SyncPreviewDialog::itemVisibleOnTab(const Item &item, int tab) const
{
    if(tab==0)
        return item.fromLocal;
    if(tab==1)
        return !item.fromLocal;

    // Обоюдные правки: только правки текста (как в мобильной версии).
    // Переименования идут по направлениям
    return item.category==Category::Edit;
}


QString SyncPreviewDialog::tabName(int tab) const
{
    if(tab==0)
        return tr("↑ Я → сервер");
    if(tab==1)
        return tr("↓ Сервер → я");

    return tr("⇅ Обоюдные правки");
}


QString SyncPreviewDialog::tabStats(int tab) const
{
    int created=0, renamed=0, edited=0, deleted=0;

    for(const Item &item : m_preview.items)
    {
        if(!itemVisibleOnTab(item, tab))
            continue;

        switch(item.category)
        {
        case Category::Create: created++; break;
        case Category::Rename: renamed++; break;
        case Category::Edit: edited++; break;
        default: deleted++; break;
        }
    }

    QStringList parts;

    if(created>0)
        parts << QStringLiteral("+%1").arg(created);

    if(renamed>0)
        parts << QStringLiteral("≡%1").arg(renamed);

    if(edited>0)
        parts << QStringLiteral("~%1").arg(edited);

    if(deleted>0)
        parts << QStringLiteral("−%1").arg(deleted);

    return parts.isEmpty() ? QStringLiteral("—") : parts.join(QStringLiteral(" "));
}


void SyncPreviewDialog::buildTabs(void)
{
    for(int tab=0; tab<3; ++tab)
        m_tabs->addTab(QStringLiteral("%1 · %2").arg(tabName(tab), tabStats(tab)));

    // Начальная вкладка: обоюдные, если есть правки — иначе вверх
    bool hasEdit=false;

    for(const Item &item : m_preview.items)
        if(item.category==Category::Edit)
        {
            hasEdit=true;
            break;
        }

    m_tab=hasEdit ? 2 : 0;
    m_tabs->setCurrentIndex(m_tab);
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
    refreshAll();
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

    QString text=QStringLiteral("%1 %2 %3%4%5")
        .arg(markerFor(item), directionFor(item),
             item.name.isEmpty() ? tr("(без имени)") : item.name,
             decided ? QStringLiteral(" ✓") : QString(),
             m_dangers.contains(item.id) ? QStringLiteral(" ⚠") : QString());
    row->setText(0, text);
    row->setData(0, Qt::UserRole+2, text);
    row->setForeground(0, QColor(categoryColor(item.category)));
}


void SyncPreviewDialog::buildItems(void)
{
    m_itemsTree->clear();

    // Узлы для всех элементов (фильтр вкладок — скрытием, как на мобиле)
    QMap<QString, QTreeWidgetItem *> nodeById;
    QList<QTreeWidgetItem *> roots;

    auto contextNode=[&](const QString &id, const QString &name,
                         QTreeWidgetItem *parent) -> QTreeWidgetItem * {
        if(nodeById.contains(id))
            return nodeById.value(id);

        QTreeWidgetItem *node=(parent!=nullptr)
            ? new QTreeWidgetItem(parent)
            : new QTreeWidgetItem(m_itemsTree);
        node->setText(0, name.isEmpty() ? tr("(ветка)") : name);
        node->setData(0, Qt::UserRole, -1);
        node->setData(0, Qt::UserRole+1, id);
        node->setData(0, Qt::UserRole+2, node->text(0));

        if(parent==nullptr)
            roots << node;

        nodeById.insert(id, node);
        return node;
    };

    // Порядок: сначала мелкие (предки раньше) — стабильный план
    QList<int> order;

    for(int i=0; i<m_preview.items.size(); ++i)
        order << i;

    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        if(m_preview.items.at(a).pathIds.size()!=m_preview.items.at(b).pathIds.size())
            return m_preview.items.at(a).pathIds.size()
                < m_preview.items.at(b).pathIds.size();

        return m_preview.items.at(a).name < m_preview.items.at(b).name;
    });

    for(int index : order)
    {
        const Item &item=m_preview.items.at(index);

        QTreeWidgetItem *parent=nullptr;

        for(int level=0; level<item.pathIds.size(); ++level)
        {
            parent=contextNode(item.pathIds.at(level),
                level < item.path.size() ? item.path.at(level) : QString(),
                parent);
        }

        QTreeWidgetItem *node=(parent!=nullptr)
            ? new QTreeWidgetItem(parent)
            : new QTreeWidgetItem(m_itemsTree);
        node->setData(0, Qt::UserRole, index);
        node->setData(0, Qt::UserRole+1, item.isBranch ? item.id : item.parentId);
        refreshRowText(node);

        if(item.isBranch)
        {
            QComboBox *combo=makeActionCombo(index);
            m_itemsTree->setItemWidget(node, 1, combo);
            node->setText(2, item.isBranch ? tr("ветка") : tr("запись"));
        }
        else
        {
            QComboBox *combo=makeActionCombo(index);
            m_itemsTree->setItemWidget(node, 1, combo);
            node->setText(2, item.isBranch ? tr("ветка") : tr("запись"));
        }

        if(parent==nullptr)
            roots << node;

        // Ветки тоже адресуются по id (для подсветки и контекста)
        if(item.isBranch)
            nodeById.insert(item.id, node);
        else if(!nodeById.contains(item.id))
            nodeById.insert(item.id, node);
    }

    applyFilters();

    for(QTreeWidgetItem *root : roots)
        root->setExpanded(true);

    // Начальный выбор: первый элемент текущей вкладки
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        if(!(*it)->isHidden())
        {
            const int itemIndex=(*it)->data(0, Qt::UserRole).toInt();

            if(itemIndex>=0)
            {
                m_itemsTree->setCurrentItem(*it);
                break;
            }
        }
    }

    refreshBranchMarks();
}


// Подписи действий в строке: только осмысленные для направления
// ("взять с сервера" — лишь нашему, "настоять на моём" — лишь ихнему);
// бессмысленные пункты скрыты, а не врут
static QList<Action> rowActions(const Item &item)
{
    QList<Action> actions;
    actions << Action::Apply << Action::Confirm;

    if(item.fromLocal)
        actions << Action::TakeServer;
    else
        actions << Action::ForceMine;

    actions << Action::Skip;
    return actions;
}

static QString actionSub(const Item &item, Action action, Danger danger)
{
    switch(action)
    {
    case Action::Apply:
        return plannedOutcome(item)
            + QObject::tr(" Без лишних вопросов.");
    case Action::Confirm:
        return plannedOutcome(item) + QObject::tr(" — подтверждаю вручную.");
    case Action::TakeServer:
        return takeServerSub(item, danger);
    case Action::ForceMine:
        return forceMineSub(item, danger);
    case Action::Skip:
        return QObject::tr("Ничего не делать, напомнить при следующем синке.");
    }

    return QString();
}

QComboBox *SyncPreviewDialog::makeActionCombo(int index)
{
    const Item &item=m_preview.items.at(index);
    const Danger danger=m_dangers.value(item.id, Danger::None);
    QComboBox *combo=new QComboBox(m_itemsTree);
    combo->setProperty("itemIndex", index);
    // обычный цвет текста — для возврата с красного
    combo->setProperty("normalColor",
                       combo->palette().color(QPalette::Text));

    for(Action action : rowActions(item))
    {
        const int row=combo->count();
        combo->addItem(SyncPreview::actionName(action),
                       static_cast<int>(action));
        combo->setItemData(row, actionSub(item, action, danger),
                           Qt::ToolTipRole);
    }

    syncComboState(combo, item);

    connect(combo,
        static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &SyncPreviewDialog::onItemActionChanged);

    return combo;
}

// Состояние комбо под решение: выбор, красный при опасности, тултип исхода
void SyncPreviewDialog::syncComboState(QComboBox *combo, const Item &item)
{
    if(combo==nullptr)
        return;

    const Action effective=m_decisions.forItem(item);
    const int row=combo->findData(static_cast<int>(effective));

    if(row>=0 && combo->currentIndex()!=row)
    {
        QSignalBlocker block(combo);
        combo->setCurrentIndex(row);
    }

    const Danger danger=m_dangers.value(item.id, Danger::None);
    const bool dangerNow=dangerOf(item, effective, m_dangers);
    QPalette pal=combo->palette();
    pal.setColor(QPalette::Text, dangerNow
                 ? QColor(QStringLiteral("#C00000"))
                 : combo->property("normalColor").value<QColor>());
    combo->setPalette(pal);
    combo->setToolTip(actionSub(item, effective, danger));
}

// Все комбо дерева под текущие решения (дуэль/категории меняют их мимо строк)
void SyncPreviewDialog::refreshCombos(void)
{
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        QTreeWidgetItem *row=*it;
        const int itemIndex=row->data(0, Qt::UserRole).toInt();

        if(itemIndex<0 || itemIndex>=m_preview.items.size())
            continue;

        QComboBox *combo=qobject_cast<QComboBox *>(
            m_itemsTree->itemWidget(row, 1));

        if(combo!=nullptr)
            syncComboState(combo, m_preview.items.at(itemIndex));
    }
}


void SyncPreviewDialog::applyFilters(void)
{
    QSet<QString> visibleIds;
    QSet<QString> contextIds;

    for(const Item &item : m_preview.items)
    {
        if(!itemVisibleOnTab(item, m_tab))
            continue;

        visibleIds.insert(item.id);

        for(const QString &aid : item.pathIds)
            contextIds.insert(aid);
    }

    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        QTreeWidgetItem *node=*it;
        const int itemIndex=node->data(0, Qt::UserRole).toInt();

        if(itemIndex<0)
        {
            // Контекстный узел предка
            const QString id=node->data(0, Qt::UserRole+1).toString();
            const bool inContext=contextIds.contains(id);
            node->setHidden(!m_wholeTree && !inContext);
            setDimmed(node, m_wholeTree || inContext);
            continue;
        }

        const Item &item=m_preview.items.at(itemIndex);
        const bool visible=visibleIds.contains(item.id);
        const bool context=!visible && contextIds.contains(item.id);

        if(m_wholeTree)
        {
            // Всё дерево: чужие вкладки — бледным контекстом
            node->setHidden(false);
            setDimmed(node, !visible);
        }
        else
        {
            node->setHidden(!visible && !context);
            setDimmed(node, context);
        }
    }
}


void SyncPreviewDialog::setDimmed(QTreeWidgetItem *node, bool dimmed)
{
    if(node==nullptr)
        return;

    const int itemIndex=node->data(0, Qt::UserRole).toInt();

    if(dimmed)
    {
        node->setForeground(0, m_itemsTree->palette().color(
            QPalette::Disabled, QPalette::WindowText));
    }
    else if(itemIndex>=0 && itemIndex<m_preview.items.size())
    {
        node->setForeground(0, QColor(categoryColor(
            m_preview.items.at(itemIndex).category)));
    }
    else
    {
        node->setData(0, Qt::ForegroundRole, QVariant());
    }
}


void SyncPreviewDialog::refreshBranchMarks(void)
{
    // Ветка подсвечена целиком, когда разобрана (как на мобиле):
    // все элементы под узлом решены
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        QTreeWidgetItem *node=*it;

        // Только узлы-ветки (есть дети или это ветка из плана)
        if(node->childCount()==0)
            continue;

        QList<int> sub;
        collectSubtreeItems(node, sub);

        if(sub.isEmpty())
            continue;

        bool all=true;

        for(int index : sub)
            if(m_decisions.forItem(m_preview.items.at(index))==Action::Confirm)
            {
                all=false;
                break;
            }

        QString base=node->data(0, Qt::UserRole+2).toString();

        if(base.isEmpty())
            base=node->text(0);

        node->setText(0, all ? base+QStringLiteral(" ✓") : base);
    }
}


void SyncPreviewDialog::collectSubtreeItems(QTreeWidgetItem *node,
                                            QList<int> &out) const
{
    if(node==nullptr)
        return;

    const int itemIndex=node->data(0, Qt::UserRole).toInt();

    if(itemIndex>=0 && itemIndex<m_preview.items.size())
        out << itemIndex;

    for(int i=0; i<node->childCount(); ++i)
        collectSubtreeItems(node->child(i), out);
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


void SyncPreviewDialog::refreshAll(void)
{
    // Тексты строк (галочки ✓, предупреждения ⚠)
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
        refreshRowText(*it);

    refreshBranchMarks();
    refreshCounter();
    refreshCombos();
    applyFilters();
}


void SyncPreviewDialog::onTabChanged(int index)
{
    m_tab=index;
    applyFilters();

    // Панель — под первый видимый элемент вкладки
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        if(!(*it)->isHidden())
        {
            const int itemIndex=(*it)->data(0, Qt::UserRole).toInt();

            if(itemIndex>=0)
            {
                m_itemsTree->setCurrentItem(*it);
                break;
            }
        }
    }
}


void SyncPreviewDialog::onWholeTreeToggled(bool checked)
{
    m_wholeTree=checked;
    applyFilters();
}


void SyncPreviewDialog::onItemActionChanged(int index)
{
    Q_UNUSED(index);
    QComboBox *combo=qobject_cast<QComboBox *>(sender());

    if(combo==nullptr)
        return;

    const int itemIndex=combo->property("itemIndex").toInt();

    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return;

    const int actionValue=combo->currentData().toInt();

    if(actionValue<0 || actionValue>4)
        return;

    m_decisions.byId[m_preview.items.at(itemIndex).id]=
        static_cast<Action>(actionValue);

    refreshAll();
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

    const int itemIndex=row->data(0, Qt::UserRole).toInt();
    const Item *current=(itemIndex>=0 && itemIndex<m_preview.items.size())
        ? &m_preview.items.at(itemIndex) : nullptr;

    QString branchKey=row->data(0, Qt::UserRole+1).toString();
    QString branchTitle;

    if(current!=nullptr)
    {
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

    for(const Item &item : m_preview.items)
    {
        if(!itemVisibleOnTab(item, m_tab))
            continue;

        const QString key=item.isBranch ? item.id : item.parentId;

        if(key==branchKey)
            inBranch++;
    }

    if(current!=nullptr && !current->isBranch)
        m_branchLabel->setText(tr("Запись «%1» · %2")
            .arg(current->name.isEmpty() ? tr("(без имени)") : current->name,
                 tabName(m_tab)));
    else
        m_branchLabel->setText(tr("Ветка «%1» · %2 · %3")
            .arg(branchTitle, tabName(m_tab)).arg(inBranch));

    updateDuel(current);
    showDiffFor(current);
}


void SyncPreviewDialog::updateDuel(const Item *item)
{
    // Поединок версий — только для правок текста записей
    if(item==nullptr || item->category!=Category::Edit || item->isBranch)
    {
        m_duelRow->setVisible(false);
        m_duelItemId.clear();
        return;
    }

    m_duelRow->setVisible(true);
    m_duelItemId=item->id;
    refreshDuel();
}


void SyncPreviewDialog::refreshDuel(void)
{
    if(m_duelItemId.isEmpty())
        return;

    const Action chosen=m_decisions.byId.value(
        m_duelItemId, Action::Confirm);

    auto markButton=[&](QPushButton *button, bool active) {
        QFont font=button->font();
        font.setBold(active);
        button->setFont(font);
        button->setFlat(!active);
    };

    markButton(m_mineButton, chosen==Action::ForceMine);
    markButton(m_theirsButton, chosen==Action::TakeServer);

    QString hint;
    const Danger danger=m_dangers.value(m_duelItemId, Danger::None);

    // Предложение эвристики (как на мобиле): битая сторона проигрывает,
    // иначе побеждает более новая
    bool proposalMine=false;

    if(m_duelItemId.isEmpty())
        proposalMine=false;
    else
    {
        for(const Item &item : m_preview.items)
            if(item.id==m_duelItemId)
            {
                if(danger==Danger::MyEmpty)
                    proposalMine=false;
                else if(danger==Danger::TheirEmpty)
                    proposalMine=true;
                else
                    proposalMine=item.fromLocal;

                break;
            }
    }

    if(chosen==Action::ForceMine)
        hint=tr("решено: наша победила");
    else if(chosen==Action::TakeServer)
        hint=tr("решено: ихняя победила");
    else
        hint=tr("предложение: %1 — подтвердите")
            .arg(proposalMine ? tr("наша") : tr("ихняя"));

    m_duelHint->setText(hint);
}


void SyncPreviewDialog::onDuelMine(void)
{
    if(m_duelItemId.isEmpty())
        return;

    m_decisions.byId[m_duelItemId]=Action::ForceMine;
    refreshDuel();
    refreshAll();
}


void SyncPreviewDialog::onDuelTheirs(void)
{
    if(m_duelItemId.isEmpty())
        return;

    m_decisions.byId[m_duelItemId]=Action::TakeServer;
    refreshDuel();
    refreshAll();
}


void SyncPreviewDialog::showDiffFor(const Item *item)
{
    if(item!=nullptr
       && (item->category==Category::MyDelete
           || item->category==Category::TheirDelete))
    {
        m_diffView->setPlainText(tr("Элемент удалён, показывать нечего."));
        return;
    }

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

    if(m_dangers.contains(item->id))
        out << tr("⚠ Защита от бития: с одной стороны почти пустой текст, дефолт — оставить полную.") << QString();

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
    help.resize(560, 520);

    QVBoxLayout *layout=new QVBoxLayout(&help);

    // Справка в цветах дерева (как в мобильной версии): символы
    // действий — цветом своей категории, опасное — красным, иначе
    // легенда врёт. QTextEdit вместо Plain: нужен rich text
    const auto sym = [](const QString &mark, const QString &color,
                        const QString &text) {
        return QStringLiteral("<p style='margin:0 0 2px 0;'><font color='%1'><b>%2</b></font> %3</p>")
            .arg(color, mark.toHtmlEscaped(), text.toHtmlEscaped());
    };
    const QString green=QStringLiteral("#66BB6A");
    const QString blue=QStringLiteral("#42A5F5");
    const QString orange=QStringLiteral("#FFFFB74D");
    const QString red=QStringLiteral("#EF5350");
    const QString purple=QStringLiteral("#CE93D8");
    const QString darkred=QStringLiteral("#C00000");

    QString html;
    html+=QStringLiteral("<h3>")+tr("Обозначения")+QStringLiteral("</h3>");
    html+=sym(QStringLiteral("+"), green, tr("создано: новая запись или ветка"));
    html+=sym(QStringLiteral("≡"), blue, tr("переименовано: изменилось имя или свойства"));
    html+=sym(QStringLiteral("~"), orange, tr("правка текста"));
    html+=sym(QStringLiteral("−"), red, tr("удалено"));
    html+=sym(QStringLiteral("↑"), green, tr("наше едет наружу"));
    html+=sym(QStringLiteral("↓"), blue, tr("ихнее приезжает к нам"));
    html+=sym(QStringLiteral("⚠"), darkred, tr("почти-пустая заметка затирает содержательную: по умолчанию остается полная версия"));
    html+=sym(QStringLiteral("✓"), green, tr("решено: выбрано действие; ветка подсвечена целиком, когда разобрана"));
    html+=QStringLiteral("<h3>")+tr("Действия")+QStringLiteral("</h3>");
    html+=sym(QStringLiteral("✓"), green, tr("Применить — выполнить запланированное молча"));
    html+=sym(QStringLiteral("✓"), green, tr("Подтвердить — то же, но только после моего взгляда"));
    html+=sym(QStringLiteral("⚠"), darkred, tr("Красное с ⚠ в списке — данные будут удалены или затёрты: удаление здесь/на сервере, затирание полной версии пустой"));
    html+=sym(QStringLiteral("•"), purple, tr("Восстановить с сервера — отменить моё, взять их версию (моё удаление — воскресить)"));
    html+=sym(QStringLiteral("•"), purple, tr("Настоять на моём — отменить ихнее, оставить мою версию (их удаление — воскресить мою копию наверх)"));
    html+=sym(QStringLiteral("•"), purple, tr("Пропустить — ничего не делать в этот раз"));
    html+=QStringLiteral("<h3>")+tr("Вкладки")+QStringLiteral("</h3>");
    html+=sym(QStringLiteral("↑"), green, tr("Я → сервер: мои изменения уедут наружу"));
    html+=sym(QStringLiteral("↓"), blue, tr("Сервер → я: ихние изменения приедут к нам"));
    html+=sym(QStringLiteral("⇅"), orange, tr("Обоюдные правки: текст разошелся с обеих сторон. Кнопками «Наша»/«Ихняя» выбираете, чья версия победит. Эвристика предлагает более новую, но решаете вы"));
    html+=QStringLiteral("<h3>")+tr("Кнопки и галочки")+QStringLiteral("</h3>");
    html+=sym(QStringLiteral("•"), green, tr("«Запомнить» — сохранить выбранные действия как умолчание для будущих синков"));
    html+=sym(QStringLiteral("•"), green, tr("«Всё дерево» — показать элементы всех вкладок сразу (чужие — бледным контекстом)"));
    html+=sym(QStringLiteral("•"), green, tr("«Действия ▾» — действия по умолчанию для целых категорий"));
    html+=sym(QStringLiteral("•"), green, tr("Клик по ветке — её записи внизу; «Применить» — выполнить, «Отмена» — прервать"));

    QTextEdit *text=new QTextEdit(&help);
    text->setReadOnly(true);
    text->setHtml(html);
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

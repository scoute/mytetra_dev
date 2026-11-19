#include <QApplication>
#include <QElapsedTimer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

#include "SyncPreviewDialog.h"
#include "libraries/sync/SyncDiff.h"

using SyncPreview::Action;
using SyncPreview::Category;
using SyncPreview::Danger;
using SyncPreview::Decisions;
using SyncPreview::Item;

// Светлая ли тема (палитра окна): скругления кнопок строк и прочие
// светлые штрихи не должны ехать в тёмную. Темы десктопа — default
// (светлая) и dark, палитра приезжает из CustomPalette CSS
static bool isLightTheme(void)
{
    return QApplication::palette().color(QPalette::Window).lightness() > 128;
}

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
        // принятие ихнего удаления — моя копия тоже умрёт
        if(item.category==Category::TheirDelete)
            return true;
        // моя правка сносится их версией (кроме спасения полной)
        if(item.category==Category::Edit)
            return danger!=Danger::MyEmpty;
        return false;
    case Action::ForceMine:
        // воскрешение моей копии наверх — спасение
        if(item.category==Category::TheirDelete)
            return false;
        // продавливание моего удаления — копия на сервере тоже умрёт
        if(item.category==Category::MyDelete)
            return true;
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

// Чем кончится "Применить/Предоставить решать мне" для этого элемента
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

static QString actionSub(const Item &item, Action action, Danger danger);

// Цвета бейджей — как в мобильной версии
static uint categoryColor(Category category)
{
    switch(category)
    {
    case Category::Create:      return 0xFF66BB6A;
    case Category::Rename:      return 0xFF42A5F5;
    case Category::Edit:        return 0xFFFFB74D;
    case Category::MyDelete:    return 0xFFEF5350;
    // Ихние удаления — бардовые, а не фиолетовые: удаление должно
    // выглядеть опасно (зеркало мобильной палитры)
    case Category::TheirDelete: return 0xFF8E244D;
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
    // Skip умер как класс: старые сохранённые Skip считаем нерешенными
    for(auto it=m_decisions.category.begin(); it!=m_decisions.category.end(); ++it)
        if(it.value()==Action::Skip) it.value()=Action::Confirm;
    for(auto it=m_decisions.byId.begin(); it!=m_decisions.byId.end(); ++it)
        if(it.value()==Action::Skip) it.value()=Action::Confirm;

    setWindowTitle(tr("Дерево изменений синхронизации"));
    resize(880, 660);

    // Кнопки свернуть/развернуть: окно часто открывают рядом с деревом,
    // на пол-экрана — разворот на полную должен быть под рукой
    setWindowFlags(windowFlags()
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint);

    // Опасность бития: почти-пустое затирает содержательное.
    // Меряем заранее (тексты обеих сторон), как в мобильной версии
    QElapsedTimer pt; pt.start();
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
    qDebug() << "SyncPreviewDialog: danger precompute" << m_preview.items.size()
             << "items in" << pt.elapsed() << "ms";

    QVBoxLayout *layout=new QVBoxLayout(this);

    // --- заголовок: бублик, статус разбора + "Запомнить" ---
    QHBoxLayout *header=new QHBoxLayout();
    m_meldRing=new SyncMeldRing(this, this);
    header->addWidget(m_meldRing);
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

    // --- предупреждение meld: удаления решаются стрелками ---
    m_meldDanger=new QLabel(this);
    {
        QFont dangerFont=m_meldDanger->font();
        dangerFont.setBold(true);
        m_meldDanger->setFont(dangerFont);
        m_meldDanger->setStyleSheet(QStringLiteral(
            "color: #C62828; background: #FFEBEE;"
            " border: 1.5px solid #EF5350; border-radius: 8px;"
            " padding: 6px 10px;"));
        m_meldDanger->setWordWrap(true);
        m_meldDanger->setVisible(false);
    }
    layout->addWidget(m_meldDanger);

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
    m_actionsButton->setText(tr("Действия по умолчанию ▾"));
    m_actionsButton->setPopupMode(QToolButton::InstantPopup);
    buildActionsMenu();
    toolbar->addWidget(m_actionsButton);

    m_expandAllButton=new QPushButton(tr("Развернуть всё"), this);
    m_expandAllButton->setToolTip(tr("Развернуть все ветки дерева"));
    m_expandAllButton->setAutoDefault(false);
    toolbar->addWidget(m_expandAllButton);

    m_collapseAllButton=new QPushButton(tr("Свернуть всё"), this);
    m_collapseAllButton->setToolTip(tr("Свернуть все ветки дерева"));
    m_collapseAllButton->setAutoDefault(false);
    toolbar->addWidget(m_collapseAllButton);

    // Переключатель вида: то же окно, те же решения — дерево xor сравнение.
    // Два радио вместо тоггла «Meld»: слово «meld» ничего не говорит
    // тем, кто не в курсе, а «Дерево | Сравнение» читается сразу
    m_treeRadio=new QRadioButton(tr("Дерево"), this);
    m_treeRadio->setToolTip(tr("Обычное дерево изменений"));
    m_treeRadio->setChecked(true);
    connect(m_treeRadio, &QRadioButton::clicked,
            this, [this]() { setMeldMode(false); });
    toolbar->addWidget(m_treeRadio);

    m_meldRadio=new QRadioButton(tr("Сравнение"), this);
    m_meldRadio->setToolTip(tr("Сравнение сторон стрелками →/←: "
                               "слева моё, справа серверное"));
    connect(m_meldRadio, &QRadioButton::clicked,
            this, [this]() { setMeldMode(true); });
    toolbar->addWidget(m_meldRadio);
    toolbar->addWidget(m_meldRadio);

    // --- Meld bulk: всё видимое — одной стрелкой (уважает фильтр).
    // Повторный тап снимает (та же toggle-семантика, что в дереве)
    m_meldUpAll=new QToolButton(this);
    m_meldUpAll->setText(tr("→ все"));
    m_meldUpAll->setToolTip(tr("Поставить → всем видимым строкам"));
    m_meldUpAll->setAutoRaise(true);
    m_meldUpAll->setVisible(false);
    connect(m_meldUpAll, &QToolButton::clicked,
            this, [this]() { meldStageAll(true); });
    toolbar->addWidget(m_meldUpAll);

    m_meldDownAll=new QToolButton(this);
    m_meldDownAll->setText(tr("← все"));
    m_meldDownAll->setToolTip(tr("Поставить ← всем видимым строкам"));
    m_meldDownAll->setAutoRaise(true);
    m_meldDownAll->setVisible(false);
    connect(m_meldDownAll, &QToolButton::clicked,
            this, [this]() { meldStageAll(false); });
    toolbar->addWidget(m_meldDownAll);

    m_jumpButton=new QPushButton(tr("↓ 0"), this);
    m_jumpButton->setToolTip(tr("К первому неразобранному"));
    // Как в мобиле: красный фон + белый текст, скругление сильнее
    m_jumpButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: #EF5350;"
        " border: 1px solid #C62828; border-radius: 10px;"
        " padding: 2px 10px; font-weight: bold; }"
        "QPushButton:disabled { color: white; background: #BDBDBD;"
        " border-color: #9E9E9E; }"));
    connect(m_jumpButton, &QPushButton::clicked,
            this, &SyncPreviewDialog::onJumpClicked);
    toolbar->addWidget(m_jumpButton);

    toolbar->addStretch(1);

    QToolButton *helpButton=new QToolButton(this);
    helpButton->setText(QStringLiteral("ⓘ"));
    helpButton->setToolTip(tr("Обозначения и вкладки"));
    connect(helpButton, &QToolButton::clicked,
            this, &SyncPreviewDialog::onHelpClicked);
    toolbar->addWidget(helpButton);
    layout->addLayout(toolbar);

    // --- дерево элементов ---
    m_itemsTree=new QTreeWidget(this);    m_itemsTree->setColumnCount(3);
    m_itemsTree->setHeaderLabels(QStringList()
                                 << tr("Элемент") << tr("Действие") << tr("Инфо"));
    m_itemsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_itemsTree->setRootIsDecorated(true);
    m_itemsTree->setIndentation(12);
    // Все строки одной высоты (шрифт общий) — скролл/индексы за O(1),
    // критично на десятках тысяч строк
    m_itemsTree->setUniformRowHeights(true);
    m_itemsTree->setItemDelegateForColumn(
        0, new SyncPreviewItemDelegate(m_itemsTree));
    // Колонка «Действие» рисуется делегатом (без живых виджетов):
    // 42k строк × 3 кнопки вешали перераскладку на каждый чих
    m_itemsTree->setItemDelegateForColumn(
        1, new SyncPreviewActionDelegate(this, m_itemsTree));
    // Ховер подсвечивается по регионам глифа — нужен трекинг мыши
    m_itemsTree->setMouseTracking(true);
    // Клики по ✓/✕/diff разбираем сами, ДО смены селекции:
    // иначе каждый тап перезагружал бы diff-панель (текст с хаба)
    m_itemsTree->viewport()->installEventFilter(this);
    // Рекурсивное да/нет: правый клик по ветке — всё поддерево разом
    m_itemsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_itemsTree, &QWidget::customContextMenuRequested,
            this, &SyncPreviewDialog::onTreeContextMenu);
    // Два вида на одни решения: дерево xor meld-таблица
    m_viewStack=new QStackedWidget(this);
    m_viewStack->addWidget(m_itemsTree);

    m_meldModel=new SyncMeldModel(this, this);
    m_meldView=new QTreeView(this);
    m_meldView->setModel(m_meldModel);
    m_meldView->setRootIsDecorated(true);
    m_meldView->setIndentation(12);
    m_meldView->setUniformRowHeights(true);
    m_meldView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_meldView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_meldView->setMouseTracking(true);
    m_meldView->setItemDelegate(new SyncMeldDelegate(this, m_meldView));
    m_meldView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_meldView->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_meldView->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_meldView->header()->resizeSection(1, 150);
    m_meldView->viewport()->installEventFilter(this);
    m_viewStack->addWidget(m_meldView);
    layout->addWidget(m_viewStack, 3);
    connect(m_expandAllButton, &QPushButton::clicked,
            this, [this]() {
        if(m_meldMode && m_meldView!=nullptr)
            m_meldView->expandAll();
        else if(m_itemsTree!=nullptr)
            m_itemsTree->expandAll();
    });
    connect(m_collapseAllButton, &QPushButton::clicked,
            this, [this]() {
        if(m_meldMode && m_meldView!=nullptr)
            m_meldView->collapseAll();
        else if(m_itemsTree!=nullptr)
            m_itemsTree->collapseAll();
    });
    m_meldModel->setSection(m_tab);
    m_meldModel->rebuild();
    buildItems();

    connect(m_itemsTree, &QTreeWidget::itemSelectionChanged,
            this, &SyncPreviewDialog::onItemSelectionChanged);
    connect(m_meldView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this, &SyncPreviewDialog::onMeldSelectionChanged);
    m_meldView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_meldView, &QWidget::customContextMenuRequested,
            this, &SyncPreviewDialog::onMeldContextMenu);

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

    QHBoxLayout *diffTabs=new QHBoxLayout();
    diffTabs->setContentsMargins(0, 0, 0, 0);
    m_diffOursButton=new QPushButton(tr("Наша"), this);
    m_diffTheirsButton=new QPushButton(tr("Ихняя"), this);
    m_diffDiffButton=new QPushButton(tr("Diff"), this);
    m_diffOursButton->setCheckable(true);
    m_diffTheirsButton->setCheckable(true);
    m_diffDiffButton->setCheckable(true);
    diffTabs->addWidget(m_diffOursButton);
    diffTabs->addWidget(m_diffTheirsButton);
    diffTabs->addWidget(m_diffDiffButton);
    diffTabs->addStretch(1);
    layout->addLayout(diffTabs);

    auto markDiffTab=[&](QPushButton *active) {
        for(QPushButton *b : {m_diffOursButton, m_diffTheirsButton,
                              m_diffDiffButton})
            b->setChecked(b==active);
    };
    markDiffTab(m_diffDiffButton);

    connect(m_diffOursButton, &QPushButton::clicked, this, [this, markDiffTab]() {
        m_diffMode=0;
        markDiffTab(m_diffOursButton);
        refreshDiffView();
    });
    connect(m_diffTheirsButton, &QPushButton::clicked, this, [this, markDiffTab]() {
        m_diffMode=1;
        markDiffTab(m_diffTheirsButton);
        refreshDiffView();
    });
    connect(m_diffDiffButton, &QPushButton::clicked, this, [this, markDiffTab]() {
        m_diffMode=2;
        markDiffTab(m_diffDiffButton);
        refreshDiffView();
    });

    m_diffView=new QTextEdit(this);
    m_diffView->setReadOnly(true);
    m_diffView->setAcceptRichText(true);
    m_diffView->setPlaceholderText(
        tr("Выберите измененную запись, чтобы увидеть diff."));
    layout->addWidget(m_diffView, 2);

    // --- кнопки: «Применить» активно при хоть одном решении (гейт
    // canApply как на мобиле), закрывает окно только когда всё разобрано,
    // иначе — частичный синк решённого с сужением дерева
    QDialogButtonBox *buttons=new QDialogButtonBox(this);
    m_applyButton=buttons->addButton(
        tr("Применить"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Отмена"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &SyncPreviewDialog::onApplyClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    m_applyButton->setDefault(true);

    refreshAll();
}


int SyncPreviewDialog::sectionOf(const Item &item) const
{
    // 0 — Я → сервер, 1 — Сервер → я, 2 — обоюдные правки.
    // Обоюдные — все Edit (как tabOf на мобиле), остальное по направлению
    if(item.category==Category::Edit)
        return 2;
    return item.fromLocal ? 0 : 1;
}


bool SyncPreviewDialog::itemVisibleOnTab(const Item &item, int tab) const
{
    return sectionOf(item)==tab;
}


QString SyncPreviewDialog::tabName(int tab) const
{
    if(tab==0)
        return tr("↑ Я → сервер");
    if(tab==1)
        return tr("↓ Сервер → я");

    return tr("⇅ Обоюдные правки");
}


// Статистика вкладки цветными кусками: текст + цвет категории.
// QTabBar не умеет многоцветный текст — куски рисуются в иконку
QList<QPair<QString, QColor>> SyncPreviewDialog::tabStatSpans(int tab) const
{
    int created=0, renamed=0, edited=0, deleted=0;

    for(const Item &item : m_preview.items)
    {
        if(isSynced(item.id))
            continue;

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

    QList<QPair<QString, QColor>> spans;

    if(created>0)
        spans << qMakePair(QStringLiteral("+%1").arg(created),
                           QColor(categoryColor(Category::Create)));

    if(renamed>0)
        spans << qMakePair(QStringLiteral("≡%1").arg(renamed),
                           QColor(categoryColor(Category::Rename)));

    if(edited>0)
        spans << qMakePair(QStringLiteral("~%1").arg(edited),
                           QColor(categoryColor(Category::Edit)));

    if(deleted>0)
        spans << qMakePair(QStringLiteral("−%1").arg(deleted),
                           QColor(categoryColor(Category::MyDelete)));

    if(spans.isEmpty())
        spans << qMakePair(QStringLiteral("—"), QColor(0x888888));

    return spans;
}

// Пиктограмма статистики: цветные куски на прозрачном фоне
QPixmap SyncPreviewDialog::tabStatsIcon(int tab) const
{
    const QList<QPair<QString, QColor>> spans=tabStatSpans(tab);
    QFontMetrics fm(m_tabs->font());
    const int gap=fm.horizontalAdvance(QStringLiteral(" "));
    int width=0;

    for(const auto &span : spans)
        width+=fm.horizontalAdvance(span.first)+gap;

    width-=gap;
    const int height=fm.height();
    QImage image(QSize(width, height), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setFont(m_tabs->font());
    int x=0;

    for(const auto &span : spans)
    {
        painter.setPen(span.second);
        painter.drawText(x, fm.ascent(), span.first);
        x+=fm.horizontalAdvance(span.first)+gap;
    }

    return QPixmap::fromImage(image);
}


void SyncPreviewDialog::refreshTabs(void)
{
    // Статистика по остатку (после частичного синка дерево сузилось)
    QSignalBlocker block(m_tabs);
    QSize iconSize(0, 0);

    for(int tab=0; tab<3; ++tab)
    {
        const QPixmap icon=tabStatsIcon(tab);
        iconSize=iconSize.expandedTo(icon.size());
        m_tabs->setTabIcon(tab, icon);
    }

    m_tabs->setIconSize(iconSize);
}


void SyncPreviewDialog::buildTabs(void)
{
    QSize iconSize(0, 0);

    for(int tab=0; tab<3; ++tab)
    {
        m_tabs->addTab(tabName(tab));
        const QPixmap icon=tabStatsIcon(tab);
        iconSize=iconSize.expandedTo(icon.size());
        m_tabs->setTabIcon(tab, icon);
    }

    m_tabs->setIconSize(iconSize);

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

        for(int a=0; a<4; ++a)
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


void SyncPreviewDialog::setPartialSyncer(PartialSyncer syncer)
{
    m_partialSync=syncer;
}


bool SyncPreviewDialog::isSynced(const QString &id) const
{
    return m_syncedIds.contains(id);
}


bool SyncPreviewDialog::isDone(const Item &item) const
{
    return isSynced(item.id)
        || m_decisions.forItem(item)!=Action::Confirm;
}


bool SyncPreviewDialog::canApply(void) const
{
    // Хоть одно решённое несинкнутое (как canApply мобилы)
    for(const Item &item : m_preview.items)
        if(!isSynced(item.id)
           && m_decisions.forItem(item)!=Action::Confirm)
            return true;

    return false;
}


bool SyncPreviewDialog::allDecided(void) const
{
    if(m_preview.items.isEmpty())
        return false;

    for(const Item &item : m_preview.items)
        if(!isDone(item))
            return false;

    return true;
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
    if(m_meldModel!=nullptr)
    {
        m_meldModel->rebuild();

        if(m_meldMode && m_meldView!=nullptr)
            m_meldView->expandAll();
    }
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

    // Части для делегата-пилюли: имя в пилюле, остальное текстом
    row->setData(0, MarkerTextRole, markerFor(item));
    row->setData(0, MarkerColorRole, QColor(categoryColor(item.category)));
    row->setData(0, ArrowTextRole, directionFor(item));
    row->setData(0, ArrowColorRole,
                 QColor(item.fromLocal ? 0xFF66BB6A : 0xFF64B5F6));
    row->setData(0, NameTextRole,
                 item.name.isEmpty() ? tr("(без имени)") : item.name);
    row->setData(0, HasCheckRole, decided);
    row->setData(0, HasWarnRole, m_dangers.contains(item.id));
    row->setData(0, PillColorRole, QColor(categoryColor(item.category)));
    row->setData(0, DimmedRole, false);
    row->setData(0, BranchRole, item.isBranch);

    // Части для делегата колонки «Действие»: поединок или ✓/✕,
    // активные кнопки, тултипы тех же текстов, что были у живых кнопок.
    // Текст diff — в DisplayRole (тот же tr, переводы не едут)
    const bool isDuel=((item.category==Category::Edit && !item.isBranch)
        || (item.category==Category::Rename && item.isBranch));
    const Danger danger=m_dangers.value(item.id, Danger::None);
    const Action effective=m_decisions.forItem(item);
    const Action accept=acceptFor(item);
    const Action reject=rejectFor(item);
    bool yesActive=false, noActive=false;

    if(decided)
    {
        if(isDuel)
        {
            yesActive=(effective==Action::ForceMine);
            noActive=(effective==Action::TakeServer);
        }
        else
        {
            yesActive=(effective==accept);
            noActive=(effective==reject);
        }
    }

    row->setText(1, tr("diff"));
    row->setData(1, ActionIsDuelRole, isDuel);
    row->setData(1, ActionYesActiveRole, yesActive);
    row->setData(1, ActionNoActiveRole, noActive);
    row->setData(1, ActionYesTipRole, actionSub(
        item, isDuel ? Action::ForceMine : accept, danger));
    row->setData(1, ActionNoTipRole, actionSub(
        item, isDuel ? Action::TakeServer : reject, danger));
    row->setData(1, ActionDiffTipRole, tr("Показать diff"));

    // Подсветка требующего решения: нерешенное — жирным + тёплый фон
    QFont rf=row->font(0);
    rf.setBold(!decided);
    row->setFont(0, rf);
    if(!decided)
        row->setBackground(0, QBrush(QColor(0x26, 0xA6, 0x9A, 0x2E)));
    else
        row->setData(0, Qt::BackgroundRole, QVariant());
}


void SyncPreviewDialog::buildItems(void)
{
    QElapsedTimer bt; bt.start();
    // 40k+ строк с живыми кнопками: без заморозки каждый insert тянет
    // перераскладку всего дерева (было 20с на 42k). Высоты строк однородны
    // (всё от одного шрифта), плюс setUniformRowHeights в конструкторе
    m_itemsTree->setUpdatesEnabled(false);
    m_itemsTree->clear();
    for(int i=0;i<3;++i) m_sectionHeaders[i]=nullptr;

    // Разделы-линейки: одно дерево целиком (как в макете).
    // Уже синкнутое частичным «Применить» в дерево не входит
    int secCount[3]={0,0,0};
    for(const Item &it : m_preview.items)
    {
        if(isSynced(it.id))
            continue;
        secCount[sectionOf(it)]++;
    }

    for(int sec=0; sec<3; ++sec)
    {
        QTreeWidgetItem *h=new QTreeWidgetItem(m_itemsTree);
        h->setFirstColumnSpanned(true);
        h->setData(0, Qt::UserRole, -2);
        h->setData(0, Qt::UserRole+1, QStringLiteral("section-%1").arg(sec));
        h->setFlags(h->flags() & ~Qt::ItemIsSelectable);
        h->setExpanded(true);
        const QString title=QStringLiteral("── %1 · %2 ──")
            .arg(tabName(sec)).arg(secCount[sec]);
        h->setText(0, title);
        h->setData(0, Qt::UserRole+2, title);
        QFont f=h->font(0); f.setBold(true); h->setFont(0, f);
        m_sectionHeaders[sec]=h;
    }

    // Узлы для всех элементов, по секциям (предки внутри своей секции)
    QMap<QString, QTreeWidgetItem *> nodeById;

    // Контекст-предки внутри своей секции (ключ секция+id)
    auto contextNode=[&](int sec, const QString &id, const QString &name,
                         QTreeWidgetItem *parent) -> QTreeWidgetItem * {
        const QString key=QStringLiteral("%1:%2").arg(sec).arg(id);
        if(nodeById.contains(key))
            return nodeById.value(key);

        QTreeWidgetItem *top=(parent!=nullptr) ? parent
                                              : m_sectionHeaders[sec];
        QTreeWidgetItem *node=new QTreeWidgetItem(top);
        node->setText(0, name.isEmpty() ? tr("(ветка)") : name);
        node->setData(0, Qt::UserRole, -1);
        node->setData(0, Qt::UserRole+1, id);
        node->setData(0, Qt::UserRole+2, node->text(0));
        node->setData(0, MarkerTextRole, QString());
        node->setData(0, ArrowTextRole, QString());
        node->setData(0, NameTextRole, node->text(0));
        node->setData(0, HasCheckRole, false);
        node->setData(0, HasWarnRole, false);
        node->setData(0, PillColorRole, QColor(0x888888));
        node->setData(0, DimmedRole, false);
        node->setData(0, BranchRole, true); // контекстные узлы — ветки
        nodeById.insert(key, node);
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
        if(isSynced(m_preview.items.at(index).id))
            continue;

        const Item &item=m_preview.items.at(index);
        const int sec=sectionOf(item);

        QTreeWidgetItem *parent=nullptr;

        for(int level=0; level<item.pathIds.size(); ++level)
        {
            parent=contextNode(sec, item.pathIds.at(level),
                level < item.path.size() ? item.path.at(level) : QString(),
                parent);
        }

        QTreeWidgetItem *node=new QTreeWidgetItem(
            parent!=nullptr ? parent : m_sectionHeaders[sec]);
        node->setData(0, Qt::UserRole, index);
        node->setData(0, Qt::UserRole+1, item.isBranch ? item.id : item.parentId);
        refreshRowText(node);

        node->setText(2, item.isBranch ? tr("ветка") : tr("запись"));

        // Ветки тоже адресуются по id секции (для подсветки)
        const QString selfKey=QStringLiteral("%1:%2").arg(sec).arg(item.id);
        if(item.isBranch)
            nodeById.insert(selfKey, node);
        else if(!nodeById.contains(selfKey))
            nodeById.insert(selfKey, node);
    }

    applyFilters();

    for(int sec=0; sec<3; ++sec)
        if(m_sectionHeaders[sec]!=nullptr)
            m_sectionHeaders[sec]->setExpanded(true);

    // По умолчанию всё развёрнуто: контекстные ветки в Qt стартуют
    // свёрнутыми, раскрываем целиком (перестройка после частичного
    // синка тоже приходит сюда)
    m_itemsTree->expandAll();

    // Начальный выбор: первый элемент
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        const int itemIndex=(*it)->data(0, Qt::UserRole).toInt();

        if(itemIndex>=0)
        {
            m_itemsTree->setCurrentItem(*it);
            break;
        }
    }

    m_itemsTree->setUpdatesEnabled(true);
    refreshBranchMarks();
    int totalRows=0;
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
        ++totalRows;
    qDebug() << "SyncPreviewDialog: buildItems" << totalRows
             << "rows in" << bt.elapsed() << "ms";
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

SyncPreviewItemDelegate::SyncPreviewItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}


QSize SyncPreviewItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{
    // Ширина — как обычно, высота — фиксированная для всех строк
    // (см. комментарий в заголовке). -2 вслед за утоньшением пилюли
    const QSize base=QStyledItemDelegate::sizeHint(option, index);
    QFontMetrics fm(option.font);
    return QSize(base.width(), fm.height()+7); // +1px воздуха между пилюлями
}


void SyncPreviewItemDelegate::paint(QPainter *painter,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    const QString name=index.data(NameTextRole).toString();

    // Нет частей — обычная отрисовка (групповые заголовки и т.п.)
    if(name.isNull())
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Фон, фокус и выделение — штатно, без текста
    QStyleOptionViewItem o(option);
    initStyleOption(&o, index);
    o.text.clear();

    QStyle *style=o.widget!=nullptr ? o.widget->style()
                                    : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);

    const bool selected=(o.state & QStyle::State_Selected)!=0;
    const bool dimmed=index.data(DimmedRole).toBool();
    const bool grayed=dimmed && !selected;

    const QColor normal=o.palette.color(QPalette::Text);
    const QColor hilite=o.palette.color(QPalette::HighlightedText);
    const QColor disabled=o.palette.color(QPalette::Disabled,
                                          QPalette::Text);
    const QColor fg=selected ? hilite : normal;
    const QColor dimFg=selected ? hilite : disabled;

    QFontMetrics fm(o.font);
    const int rowH=o.rect.height();
    const int gap=6;
    const int pillPad=8;

    // Замер сегментов: маркер, стрелка, пилюля(имя), хвост
    const QString marker=index.data(MarkerTextRole).toString();
    const QColor markerBase=index.data(MarkerColorRole).value<QColor>();
    const QString arrow=index.data(ArrowTextRole).toString();
    const QColor arrowBase=index.data(ArrowColorRole).value<QColor>();
    const bool hasWarn=index.data(HasWarnRole).toBool();
    const bool subtreeDone=index.data(SubtreeDoneRole).toBool();

    const QString warnMark=hasWarn ? QStringLiteral(" ⚠") : QString();
    const QString doneMark=subtreeDone ? QStringLiteral(" ✓") : QString();
    const int warnW=hasWarn ? fm.horizontalAdvance(warnMark) : 0;
    const int doneW=subtreeDone ? fm.horizontalAdvance(doneMark) : 0;

    int x=o.rect.left()+4;
    const int right=o.rect.right()-4;
    const int cy=o.rect.top()+rowH/2;

    auto drawSegment=[&](const QString &s, const QColor &c, int &xpos) {
        if(s.isEmpty())
            return;

        painter->setPen(c);
        painter->setFont(o.font);
        painter->drawText(xpos, cy-fm.height()/2,
                          fm.horizontalAdvance(s), fm.height(),
                          Qt::AlignLeft | Qt::AlignVCenter, s);
        xpos+=fm.horizontalAdvance(s)+gap;
    };

    drawSegment(marker, grayed ? dimFg : markerBase.isValid()
                ? (selected ? fg : markerBase) : fg, x);
    drawSegment(arrow, grayed ? dimFg : arrowBase.isValid()
                ? (selected ? fg : arrowBase) : fg, x);

    // Пилюля имени: заливка категории, рамка категории, текст обычный
    QColor pillBase=index.data(PillColorRole).value<QColor>();

    if(!pillBase.isValid())
        pillBase=QColor(0x888888);

    if(grayed)
        pillBase=QColor(0x888888);

    const bool isBranch=index.data(BranchRole).toBool();
    const int iconW=isBranch ? fm.height() : fm.height()*4/5;
    const int iconGap=6;
    const int tailW=warnW+doneW;
    const int nameAvail=qMax(20, right-x-tailW-pillPad*2-iconW-iconGap);
    const QString shownName=fm.elidedText(name, Qt::ElideRight, nameAvail);
    const int pillW=fm.horizontalAdvance(shownName)+iconW+iconGap+pillPad*2;
    // Пилюля уже по вертикали (компромисс: -2px высоты, иконка едет следом)
    const int pillH=qMin(rowH-2, fm.height()+4);
    // Горизонтали — строго на целые пиксели: иначе косметическое перо
    // 1px ложится между пикселями и верх/низ мылятся даже без AA
    const qreal topEdge=qRound(cy-pillH/2.0);
    const qreal botEdge=qRound(cy+pillH/2.0);
    const QRectF pillRect(x, topEdge, pillW, botEdge-topEdge);
    const qreal pillRadius=(botEdge-topEdge)/2;

    QColor fill=pillBase;
    fill.setAlpha(selected ? 255 : 90);
    painter->setBrush(fill);
    // Ветки: рамка чёрная и всегда 1px — цветная жирная (2px при
    // subtreeDone) смотрелась наляписто рядом с заметками 1px.
    // Готовность поддерева и так видна по зелёной галке после пилюли.
    // Серый контекст — как был, серым пунктиром
    QPen pillPen(selected ? fg : (isBranch && !grayed ? QColor(Qt::black)
                                                     : pillBase), 1);
    // Строго 1px без мыла: косметическое перо на дробных координатах
    // со сглаживанием даёт полупиксельную «1.5» и липнет к соседям
    pillPen.setWidth(0);
    // Серый контекст — пунктиром, как на мобиле
    if(grayed)
        pillPen.setStyle(Qt::DashLine);
    painter->setPen(pillPen);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->drawRoundedRect(pillRect, pillRadius, pillRadius);
    painter->setRenderHint(QPainter::Antialiasing, false);

    // Иконка типа: классика как на мобиле — жёлтая папка,
    // белый лист с серым контуром
    {
        const int iconH=pillH-8;
        const int iconX=x+6; // иконка чуть правее, зазор до текста 6→4
        const int iconY=cy-iconH/2;
        painter->setRenderHint(QPainter::Antialiasing, true);
        if(isBranch)
        {
            const int tabW=iconW*45/100, tabH=iconH*32/100;
            painter->setBrush(QColor(0xFFC107));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(QRectF(iconX, iconY, tabW, tabH+2), 2, 2);
            painter->drawRoundedRect(QRectF(iconX, iconY+tabH*0.5,
                iconW, iconH-tabH*0.5), 2, 2);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(QColor(0xE6A200), 1));
            painter->drawRoundedRect(QRectF(iconX+0.5, iconY+tabH*0.5+0.5,
                iconW-1, iconH-tabH*0.5-1), 2, 2);
        }
        else
        {
            const int f=iconW*30/100;
            QPolygonF page;
            page << QPointF(iconX, iconY)
                 << QPointF(iconX+iconW-f, iconY)
                 << QPointF(iconX+iconW, iconY+f)
                 << QPointF(iconX+iconW, iconY+iconH)
                 << QPointF(iconX, iconY+iconH);
            painter->setBrush(QColor(Qt::white));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(page);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(QColor(0x9E9E9E), 1));
            painter->drawPolygon(page);
        }
        painter->setRenderHint(QPainter::Antialiasing, false);
    }

    painter->setPen(grayed ? dimFg : fg);
    painter->setFont(o.font);
    painter->drawText(QRect(x+4+iconW+iconGap, pillRect.top(),
        pillW-4-iconW-iconGap-pillPad, pillH),
        Qt::AlignLeft | Qt::AlignVCenter, shownName);
    x+=pillW+gap;

    if(hasWarn)
    {
        painter->setPen(grayed ? dimFg : QColor(0xC00000));
        painter->drawText(x, cy-fm.height()/2, warnW, fm.height(),
                          Qt::AlignLeft | Qt::AlignVCenter, warnMark);
        x+=warnW;
    }

    if(subtreeDone)
    {
        painter->setPen(grayed ? dimFg : QColor(0xFF66BB6A));
        painter->drawText(x, cy-fm.height()/2,
                          doneW, fm.height(),
                          Qt::AlignLeft | Qt::AlignVCenter, doneMark);
    }
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



SyncPreview::Action SyncPreviewDialog::acceptFor(const SyncPreview::Item &item)
{
    using Action=SyncPreview::Action;
    Q_UNUSED(item);
    return Action::Apply;
}


SyncPreview::Action SyncPreviewDialog::rejectFor(const SyncPreview::Item &item)
{
    using Action=SyncPreview::Action;
    using Category=SyncPreview::Category;
    if(item.category==Category::TheirDelete)
        return Action::ForceMine;
    if(item.category==Category::Edit)
        return item.fromLocal ? Action::TakeServer : Action::ForceMine;
    return item.fromLocal ? Action::TakeServer : Action::ForceMine;
}


SyncPreviewActionDelegate::SyncPreviewActionDelegate(SyncPreviewDialog *dialog,
                                                         QTreeWidget *view)
    : QStyledItemDelegate(view),
      m_dialog(dialog),
      m_view(view)
{
}


// Раскладка ячейки «Действие»: три региона в строку с шагом 2 —
// как был QHBoxLayout(spacing 2, margins 0) с тремя QToolButton.
// false — строка без действий (заголовок секции, контекст)
bool SyncPreviewActionDelegate::cellLayout(const QStyleOptionViewItem &option,
                                           const QModelIndex &index,
                                           QRect &yes, QRect &no,
                                           QRect &diff) const
{
    const QModelIndex sib=index.sibling(index.row(), 0);

    if(!sib.isValid() || !sib.data(Qt::UserRole).isValid())
        return false;

    if(sib.data(Qt::UserRole).toInt()<0)
        return false;

    if(!index.data(ActionIsDuelRole).isValid())
        return false;

    const bool duel=index.data(ActionIsDuelRole).toBool();
    QFont bold=option.font;
    bold.setBold(true);
    const QFontMetrics fm(bold);
    // Глифы кнопок чуть меньше текста (duel-стрелки — тем же кеглем,
    // кнопки остаются парными); diff-текст — обычным кеглем
    QFont glyphFont=bold;
    // Глифы кнопок — максимально жирно (Black вместо Bold):
    // белые ✓/✕ на цветном фоне тонкими почти не читаются
    glyphFont.setWeight(QFont::Black);
    if(glyphFont.pointSize()>0)
        glyphFont.setPointSize(qMax(8, glyphFont.pointSize()-1));
    else
        glyphFont.setPixelSize(qMax(8, glyphFont.pixelSize()-1));
    const QFontMetrics gfm(glyphFont);
    const QFontMetrics rfm(option.font);
    const QString yesGlyph=duel ? QStringLiteral("\u2191")
                                : QStringLiteral("\u2713");
    const QString noGlyph=duel ? QStringLiteral("\u2193")
                               : QStringLiteral("\u2715");
    const QString diffLabel=index.data(Qt::DisplayRole).toString();

    if(diffLabel.isEmpty())
        return false;

    const int h=qMax(16, option.rect.height()-4);
    const int cy=option.rect.center().y();
    const int wYes=gfm.horizontalAdvance(yesGlyph)+12;
    const int wNo=gfm.horizontalAdvance(noGlyph)+12;
    const int wDiff=rfm.horizontalAdvance(diffLabel)+12;
    int x=option.rect.left()+1;
    yes=QRect(x, cy-h/2, wYes, h);
    x+=wYes+2;
    no=QRect(x, cy-h/2, wNo, h);
    x+=wNo+2;
    diff=QRect(x, cy-h/2, wDiff, h);
    return true;
}


int SyncPreviewActionDelegate::hitTest(const QModelIndex &index,
                                         const QPoint &viewportPos) const
{
    if(!index.isValid() || index.column()!=1 || m_view==nullptr)
        return 0;

    QStyleOptionViewItem option;
    option.initFrom(m_view);
    option.rect=m_view->visualRect(index);
    option.font=m_view->font();
    return partAt(option, index, viewportPos);
}


// Регион внутри ячейки: 0 — мимо кнопок, 1 — «да», 2 — «нет», 3 — diff
int SyncPreviewActionDelegate::partAt(const QStyleOptionViewItem &option,
                                      const QModelIndex &index,
                                      const QPoint &pos) const
{
    QRect rYes, rNo, rDiff;

    if(!cellLayout(option, index, rYes, rNo, rDiff))
        return 0;

    if(rYes.contains(pos))
        return 1;

    if(rNo.contains(pos))
        return 2;

    if(rDiff.contains(pos))
        return 3;

    return 0;
}


void SyncPreviewActionDelegate::paint(QPainter *painter,
                                      const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    // Фон, фокус и выделение — штатно, текста в колонке нет
    QStyleOptionViewItem o(option);
    initStyleOption(&o, index);
    o.text.clear();

    QStyle *style=o.widget!=nullptr ? o.widget->style()
                                    : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);

    QRect rYes, rNo, rDiff;

    if(!cellLayout(option, index, rYes, rNo, rDiff))
        return;

    const bool duel=index.data(ActionIsDuelRole).toBool();
    const bool yesActive=index.data(ActionYesActiveRole).toBool();
    const bool noActive=index.data(ActionNoActiveRole).toBool();
    const QString diffLabel=index.data(Qt::DisplayRole).toString();

    // Те же цвета, что были у живых кнопок (см. старый syncWidgetState):
    // в активе — сплошной цвет + белый глиф, иначе — dim-текст на
    // полупрозрачном сером. diff — текстовый линк без фона
    const QColor green(0x66BB6A), red(0xEF5350), blue(0x64B5F6);
    const QColor grayFg(0x777777), grayBd(0x555555);
    const QColor grayBg(0x777777, 0x777777, 0x777777, 0x28);
    const int radius=isLightTheme() ? 6 : 0;

    const bool hoverOn=(o.state & QStyle::State_MouseOver)!=0
        && m_hover==index && m_hoverPart!=0;

    QFont bold=o.font;
    bold.setBold(true);
    QFont glyphFont=bold;
    glyphFont.setWeight(QFont::Black); // как в cellLayout: жирнее белого
    if(glyphFont.pointSize()>0)
        glyphFont.setPointSize(qMax(8, glyphFont.pointSize()-1));
    else
        glyphFont.setPixelSize(qMax(8, glyphFont.pixelSize()-1));

    auto paintButton=[&](const QRect &r, const QString &glyph,
                         const QColor &solid, bool active, bool hovered) {
        painter->setBrush(active ? solid : grayBg);
        painter->setPen(QPen(active ? solid : (hovered ? solid : grayBd),
                             1.5));
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawRoundedRect(QRectF(r), radius, radius);
        // ✓/✕ — жирным штрихом вручную: шрифтовой глиф тонкий даже
        // в Black (у DejaVu нет такого начертания — Qt молча берёт Bold).
        // Стрелки дуэлей — шрифтом, как раньше
        if(glyph==QStringLiteral("\u2713") || glyph==QStringLiteral("\u2715"))
        {
            QPen strokePen(active ? QColor(Qt::white) : grayFg,
                           qMax(2, r.height()/7));
            strokePen.setCapStyle(Qt::RoundCap);
            strokePen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(strokePen);
            const qreal x0=r.left(), y0=r.top();
            const qreal w=r.width(), h=r.height();
            if(glyph==QStringLiteral("\u2713"))
            {
                painter->drawLine(QPointF(x0+w*0.28, y0+h*0.55),
                                  QPointF(x0+w*0.45, y0+h*0.70));
                painter->drawLine(QPointF(x0+w*0.45, y0+h*0.70),
                                  QPointF(x0+w*0.74, y0+h*0.30));
            }
            else
            {
                painter->drawLine(QPointF(x0+w*0.34, y0+h*0.34),
                                  QPointF(x0+w*0.66, y0+h*0.66));
                painter->drawLine(QPointF(x0+w*0.66, y0+h*0.34),
                                  QPointF(x0+w*0.34, y0+h*0.66));
            }
        }
        else
        {
            painter->setPen(active ? QColor(Qt::white) : grayFg);
            painter->setFont(glyphFont);
            painter->drawText(r, Qt::AlignCenter, glyph);
        }
        painter->setRenderHint(QPainter::Antialiasing, false);
    };

    paintButton(rYes, duel ? QStringLiteral("\u2191")
                           : QStringLiteral("\u2713"),
                green, yesActive, hoverOn && m_hoverPart==1);
    paintButton(rNo, duel ? QStringLiteral("\u2193")
                          : QStringLiteral("\u2715"),
                duel ? blue : red, noActive, hoverOn && m_hoverPart==2);

    QFont linkFont=o.font;

    if(hoverOn && m_hoverPart==3)
        linkFont.setUnderline(true);

    painter->setFont(linkFont);
    painter->setPen(o.palette.color(QPalette::Text));
    painter->drawText(rDiff, Qt::AlignCenter, diffLabel);
}


QSize SyncPreviewActionDelegate::sizeHint(const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    QRect rYes, rNo, rDiff;

    if(!cellLayout(option, index, rYes, rNo, rDiff))
    {
        const QSize base=QStyledItemDelegate::sizeHint(option, index);
        return QSize(base.width(), option.fontMetrics.height()+7);
    }

    return QSize(rDiff.right()-rYes.left()+3,
                 option.fontMetrics.height()+7);
}


bool SyncPreviewActionDelegate::editorEvent(QEvent *event,
                                            QAbstractItemModel *model,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index)
{
    Q_UNUSED(model);

    if(event->type()==QEvent::MouseMove)
    {
        const QMouseEvent *me=static_cast<const QMouseEvent *>(event);
        const int part=partAt(option, index, me->pos());

        if(m_hover!=index || m_hoverPart!=part)
        {
            m_hover=index;
            m_hoverPart=part;

            if(m_view!=nullptr && m_view->viewport()!=nullptr)
                m_view->viewport()->update();
        }

        return false;
    }

    // Клики НЕ здесь: их разбирает фильтр вьюпорта (eventFilter диалога),
    // чтобы press не двигал селекцию (иначе каждый ✓/✕ перезагружал бы
    // diff с хаба). Здесь только ховер
    return false;
}


// Тултипы тех же текстов, что были у живых кнопок — по регионам глифа
bool SyncPreviewActionDelegate::helpEvent(QHelpEvent *event,
                                         QAbstractItemView *view,
                                         const QStyleOptionViewItem &option,
                                         const QModelIndex &index)
{
    if(event->type()!=QEvent::ToolTip)
        return false;

    const int part=partAt(option, index, event->pos());
    QString tip;

    if(part==1)
        tip=index.data(ActionYesTipRole).toString();
    else if(part==2)
        tip=index.data(ActionNoTipRole).toString();
    else if(part==3)
        tip=index.data(ActionDiffTipRole).toString();

    if(tip.isEmpty())
        return false;

    QToolTip::showText(event->globalPos(), tip, view);
    return true;
}


bool SyncPreviewDialog::eventFilter(QObject *watched, QEvent *event)
{
    if(watched==m_itemsTree->viewport())
    {
        if(event->type()==QEvent::MouseButtonPress)
        {
            const QMouseEvent *me=static_cast<const QMouseEvent *>(event);

            if(me->button()==Qt::LeftButton)
            {
                cancelActionArm(); // зависший взвод (release мимо вьюпорта)
                auto *delegate=static_cast<SyncPreviewActionDelegate *>(
                    m_itemsTree->itemDelegateForColumn(1));
                const QModelIndex idx=m_itemsTree->indexAt(me->pos());
                const int part=(delegate!=nullptr && idx.isValid())
                    ? delegate->hitTest(idx, me->pos()) : 0;

                if(part!=0)
                {
                    m_actionArmed=true;
                    m_armedView=m_itemsTree;
                    m_armedIndex=idx;
                    m_armedPart=part;
                    m_savedCurrent=m_itemsTree->currentIndex();
                    // Селекция сменится молча: diff с хаба не дёргаем
                    m_itemsTree->blockSignals(true);
                }
            }
        }
        else if(event->type()==QEvent::MouseButtonRelease)
        {
            const QMouseEvent *me=static_cast<const QMouseEvent *>(event);

            if(me->button()==Qt::LeftButton && m_actionArmed
               && m_armedView==m_itemsTree)
            {
                auto *delegate=static_cast<SyncPreviewActionDelegate *>(
                    m_itemsTree->itemDelegateForColumn(1));
                const QModelIndex idx=m_itemsTree->indexAt(me->pos());
                const int part=(delegate!=nullptr && idx.isValid())
                    ? delegate->hitTest(idx, me->pos()) : 0;
                const bool same=idx.isValid() && idx==m_armedIndex
                    && part==m_armedPart && part!=0;
                // Селекцию вернуть молча, затем разблокировать
                m_itemsTree->setCurrentIndex(m_savedCurrent);
                m_itemsTree->blockSignals(false);
                m_actionArmed=false;
                m_armedView=nullptr;

                if(same)
                {
                    static const char *roles[]={"", "yes", "no", "diff"};
                    const QModelIndex sib=
                        m_armedIndex.sibling(m_armedIndex.row(), 0);
                    const int itemIndex=sib.data(Qt::UserRole).toInt();

                    if(itemIndex>=0)
                        handleActionClick(itemIndex,
                            QString::fromLatin1(roles[part]));
                }

                return true;
            }
        }
        else if(event->type()==QEvent::Leave)
        {
            cancelActionArm();
        }
    }
    else if(m_meldView!=nullptr && watched==m_meldView->viewport())
    {
        // Та же защита селекции для meld-таблицы: стрелки не двигают
        // выбор (иначе каждый тап перезагружал бы diff с хаба)
        if(event->type()==QEvent::MouseButtonPress)
        {
            const QMouseEvent *me=static_cast<const QMouseEvent *>(event);

            if(me->button()==Qt::LeftButton)
            {
                cancelActionArm();
                auto *delegate=static_cast<SyncMeldDelegate *>(
                    m_meldView->itemDelegate());
                const QModelIndex idx=m_meldView->indexAt(me->pos());
                const int part=(delegate!=nullptr && idx.isValid()
                                && idx.column()==1)
                    ? delegate->hitTest(idx, me->pos()) : 0;

                if(part!=0)
                {
                    m_actionArmed=true;
                    m_armedView=m_meldView;
                    m_armedIndex=idx;
                    m_armedPart=part;
                    m_savedCurrent=m_meldView->currentIndex();
                    m_meldView->blockSignals(true);
                }
            }
        }
        else if(event->type()==QEvent::MouseButtonRelease)
        {
            const QMouseEvent *me=static_cast<const QMouseEvent *>(event);

            if(me->button()==Qt::LeftButton && m_actionArmed
               && m_armedView==m_meldView)
            {
                auto *delegate=static_cast<SyncMeldDelegate *>(
                    m_meldView->itemDelegate());
                const QModelIndex idx=m_meldView->indexAt(me->pos());
                const int part=(delegate!=nullptr && idx.isValid()
                                && idx.column()==1)
                    ? delegate->hitTest(idx, me->pos()) : 0;
                const bool same=idx.isValid() && idx==m_armedIndex
                    && part==m_armedPart && part!=0;
                m_meldView->setCurrentIndex(m_savedCurrent);
                m_meldView->blockSignals(false);
                m_actionArmed=false;
                m_armedView=nullptr;

                if(same && m_meldModel!=nullptr)
                {
                    static const char *roles[]={"", "up", "down", "diff"};
                    const int itemIndex=
                        m_meldModel->itemIndex(m_armedIndex);

                    if(itemIndex>=0)
                        meldActionClick(itemIndex,
                            QString::fromLatin1(roles[part]));
                }

                return true;
            }
        }
        else if(event->type()==QEvent::Leave)
        {
            cancelActionArm();
        }
    }

    return QDialog::eventFilter(watched, event);
}


void SyncPreviewDialog::cancelActionArm(void)
{
    if(!m_actionArmed)
        return;

    if(m_meldView!=nullptr && m_armedView==m_meldView)
    {
        m_meldView->blockSignals(true);
        m_meldView->setCurrentIndex(m_savedCurrent);
        m_meldView->blockSignals(false);
    }
    else if(m_itemsTree!=nullptr)
    {
        m_itemsTree->blockSignals(true);
        m_itemsTree->setCurrentIndex(m_savedCurrent);
        m_itemsTree->blockSignals(false);
    }

    m_actionArmed=false;
    m_armedView=nullptr;
}


void SyncPreviewDialog::onTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *node=m_itemsTree->itemAt(pos);

    if(node==nullptr)
        return;

    QList<int> sub;
    collectSubtreeItems(node, sub);

    if(sub.isEmpty())
        return;

    QMenu menu(m_itemsTree);
    QAction *yesAction=menu.addAction(
        tr("✓ Применить всё поддерево (%1)").arg(sub.size()));
    QAction *noAction=menu.addAction(
        tr("✕ Отклонить всё поддерево (%1)").arg(sub.size()));

    QAction *picked=menu.exec(m_itemsTree->viewport()->mapToGlobal(pos));

    if(picked==yesAction)
        applySubtreeDecisions(node, true);
    else if(picked==noAction)
        applySubtreeDecisions(node, false);
}


void SyncPreviewDialog::applySubtreeDecisions(QTreeWidgetItem *node,
                                              bool accept)
{
    QList<int> sub;
    collectSubtreeItems(node, sub);

    if(sub.isEmpty())
        return;

    // Цель — та же, что одиночный тап (дуэли — своим маппингом)
    auto targetFor=[this, accept](const Item &item) {
        const bool isDuel=((item.category==Category::Edit && !item.isBranch)
            || (item.category==Category::Rename && item.isBranch));

        if(accept)
            return isDuel ? Action::ForceMine : acceptFor(item);

        return isDuel ? Action::TakeServer : rejectFor(item);
    };

    // Всё уже в цели — снимаем обратно (как повторный тап)
    bool allSet=true;

    for(int index : sub)
        if(m_decisions.forItem(m_preview.items.at(index))!=targetFor(m_preview.items.at(index)))
        {
            allSet=false;
            break;
        }

    for(int index : sub)
    {
        const Item &item=m_preview.items.at(index);
        m_decisions.byId[item.id]=allSet ? Action::Confirm : targetFor(item);
    }

    refreshAll();
}


void SyncPreviewDialog::handleActionClick(int itemIndex, const QString &role)
{
    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return;

    const Item &item=m_preview.items.at(itemIndex);
    const Action effective=m_decisions.forItem(item);

    if(role==QStringLiteral("diff"))
    {
        // Diff по ссылке: выбрать строку (панель diff обновится сама)
        for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
        {
            if((*it)->data(0, Qt::UserRole).toInt()==itemIndex)
            {
                m_itemsTree->setCurrentItem(*it);
                m_itemsTree->scrollToItem(*it);
                break;
            }
        }
        return;
    }

    const bool isDuel=((item.category==Category::Edit && !item.isBranch)
        || (item.category==Category::Rename && item.isBranch));
    Action target=Action::Confirm;

    if(role==QStringLiteral("yes"))
        target=isDuel ? Action::ForceMine : acceptFor(item);
    else if(role==QStringLiteral("no"))
        target=isDuel ? Action::TakeServer : rejectFor(item);
    else
        return;

    // Повторный тап снимает решение (как в макете)
    m_decisions.byId[item.id]=(effective==target) ? Action::Confirm : target;
    refreshAll();
}

void SyncPreviewDialog::applyFilters(void)
{
    // Новая концепция: одно дерево целиком, вкладки — якоря.
    // Фильтра нет: всё видимо, контекст не бледним
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        (*it)->setHidden(false);
        setDimmed(*it, false);
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

        node->setData(0, SubtreeDoneRole, all);
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
        if(isDone(item))
            decided++;

    const int total=m_preview.items.size();

    if(total>0 && decided==total)
        m_statusLabel->setText(tr("✓ Всё разобрано (%1/%2)").arg(decided).arg(total));
    else
        m_statusLabel->setText(tr("Разобрано (%1/%2)").arg(decided).arg(total));

    // Гейт как на мобиле: активно при хоть одном решённом несинкнутом
    if(m_applyButton!=nullptr)
        m_applyButton->setEnabled(canApply());

    // Бублик прогресса + предупреждение про удаления (meld)
    if(m_meldRing!=nullptr)
        m_meldRing->setProgress(decided, total);

    if(m_meldDanger!=nullptr)
    {
        int meldDeaths=0;

        for(const Item &item : m_preview.items)
        {
            if(isSynced(item.id))
                continue;

            if(m_tab>=0 && sectionOf(item)!=m_tab)
                continue;

            if(item.category==Category::MyDelete
               || item.category==Category::TheirDelete)
                if(m_decisions.forItem(item)==Action::Confirm)
                    meldDeaths++;
        }

        if(m_meldMode && meldDeaths>0)
        {
            m_meldDanger->setText(tr("%1: удаления в плане — "
                "стрелка туда/сюда решает судьбу").arg(meldDeaths));
            m_meldDanger->setVisible(true);
        }
        else
            m_meldDanger->setVisible(false);
    }

    if(m_jumpButton!=nullptr)
    {
        const int left=total-decided;
        m_jumpButton->setText(tr("↓ %1").arg(left));
        m_jumpButton->setVisible(left>0);
    }
}


void SyncPreviewDialog::onApplyClicked(void)
{
    // Всё разобрано — старый путь: закрыть окно, финал — в сессии
    if(allDecided() || !m_partialSync)
    {
        accept();
        return;
    }

    // Частичное «Применить» (как на мобиле): синкаем решённое сейчас,
    // дерево локально сузится до остатка, окно НЕ закрываем
    QMap<QString, Action> actions;

    for(const Item &item : m_preview.items)
    {
        if(isSynced(item.id))
            continue;

        const Action effective=m_decisions.forItem(item);

        if(effective!=Action::Confirm)
            actions.insert(item.id, effective);
    }

    if(actions.isEmpty())
        return;

    const int decidedN=actions.size();
    int totalN=0;
    for(const Item &item : m_preview.items)
        if(!isSynced(item.id))
            totalN++;
    const QMessageBox::StandardButton pick=QMessageBox::question(
        this,
        tr("Частичная синхронизация"),
        tr("Синхронизировать только %1 из %2? "
           "Оставшиеся элементы можно синхронизировать позже в этом же окне.")
            .arg(decidedN).arg(totalN),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if(pick!=QMessageBox::Yes)
        return;

    bool ok=false;

    try
    {
        ok=m_partialSync(actions);
    }
    catch(...)
    {
        ok=false;
    }

    if(!ok)
    {
        QMessageBox::warning(
            this,
            tr("Частичная синхронизация"),
            tr("Не вышло — попробуйте позже."));
        return;
    }

    for(auto it=actions.constBegin(); it!=actions.constEnd(); ++it)
        m_syncedIds.insert(it.key());

    buildItems();
    if(m_meldModel!=nullptr)
    {
        m_meldModel->rebuild();

        if(m_meldMode && m_meldView!=nullptr)
            m_meldView->expandAll();
    }
    refreshTabs();
    refreshAll();

    QMessageBox::information(
        this,
        tr("Частичная синхронизация"),
        tr("Готово: %1, осталось: %2.").arg(decidedN).arg(totalN-decidedN));
}


void SyncPreviewDialog::refreshAll(void)
{
    // Тексты строк (галочки ✓, предупреждения ⚠)
    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
        refreshRowText(*it);

    refreshBranchMarks();
    refreshCounter();
    applyFilters();

    if(m_meldModel!=nullptr)
        m_meldModel->refreshAllRows();
}


void SyncPreviewDialog::setMeldMode(bool meld)
{
    if(m_meldMode==meld)
        return;

    m_meldMode=meld;

    if(m_treeRadio!=nullptr)
        m_treeRadio->setChecked(!meld);

    if(m_meldRadio!=nullptr)
        m_meldRadio->setChecked(meld);

    // Заголовок следит за видом: дерево изменений xor сравнение сторон
    setWindowTitle(meld ? tr("Сравнение сторон синхронизации")
                        : tr("Дерево изменений синхронизации"));

    if(m_viewStack!=nullptr)
        m_viewStack->setCurrentIndex(meld ? 1 : 0);

    // Развернуть/свернуть — оба вида структурные: кнопки всегда видны,
    // действуют на активный вид
    if(m_expandAllButton!=nullptr)
        m_expandAllButton->setVisible(true);

    if(m_collapseAllButton!=nullptr)
        m_collapseAllButton->setVisible(true);

    if(m_meldUpAll!=nullptr)
        m_meldUpAll->setVisible(meld);

    if(m_meldDownAll!=nullptr)
        m_meldDownAll->setVisible(meld);

    if(meld && m_meldModel!=nullptr)
    {
        m_meldModel->setSection(m_tab);
        m_meldModel->rebuild();

        if(m_meldView!=nullptr)
            m_meldView->expandAll();

        // Тот же выбор в другом виде (если строка есть в фильтре)
        const int current=currentItemIndex();
        Q_UNUSED(current);
        refreshCounter();
    }
    else
    {
        refreshCounter();
    }
}


bool SyncPreviewDialog::isMeldMode(void) const
{
    return m_meldMode;
}


void SyncPreviewDialog::meldActionClick(int itemIndex, const QString &role)
{
    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return;

    if(role==QStringLiteral("diff"))
    {
        // Diff по ссылке: выбрать строку (панель diff обновится сама)
        if(m_meldModel!=nullptr && m_meldView!=nullptr)
        {
            const QModelIndex idx=m_meldModel->indexForItem(itemIndex);

            if(idx.isValid())
            {
                m_meldView->setCurrentIndex(idx);
                m_meldView->scrollTo(idx);
            }
        }

        return;
    }

    const Item &item=m_preview.items.at(itemIndex);
    const Action effective=m_decisions.forItem(item);
    Action target=Action::Confirm;
    bool have=false;

    if(role==QStringLiteral("up"))
        have=meldUpTarget(item, target);
    else if(role==QStringLiteral("down"))
        have=meldDownTarget(item, target);
    else
        return;

    if(!have)
        return;

    // Повторный тап снимает решение (как в дереве)
    m_decisions.byId[item.id]=(effective==target) ? Action::Confirm : target;
    refreshAll();

    if(m_meldModel!=nullptr)
        m_meldModel->refreshPreviewRow(itemIndex);
}


QString SyncPreviewDialog::meldArrowTip(int itemIndex, bool up) const
{
    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return QString();

    const Item &item=m_preview.items.at(itemIndex);
    const Danger danger=m_dangers.value(item.id, Danger::None);
    Action target=Action::Confirm;

    if(up)
    {
        if(!meldUpTarget(item, target))
            return QString();
    }
    else if(!meldDownTarget(item, target))
        return QString();

    // Meld-подсказки подробнее дерева: в скобках — чем кончится
    // именно здесь (дерево тот же словарь показывает без скобок)
    const bool dangerMyEmpty=(danger==Danger::MyEmpty);
    const bool dangerTheirEmpty=(danger==Danger::TheirEmpty);

    if(item.category==Category::Create)
    {
        if(up)
            return tr("Залить на сервер (останется с обеих сторон)");
        return tr("Скачать с сервера (останется с обеих сторон)");
    }

    if(item.category==Category::Edit)
    {
        if(up)
        {
            if(dangerTheirEmpty)
                return tr("Оставить мою полную версию (их почти пустая)");
            return tr("Оставить мою версию (их заменится моей)");
        }

        if(dangerMyEmpty)
            return tr("Взять их полную версию (моя почти пустая)");
        return tr("Взять их версию (моя заменится их)");
    }

    if(item.category==Category::Rename)
    {
        if(up)
        {
            if(item.fromLocal)
                return tr("Моё имя побеждает (станет моим с обеих сторон)");
            return tr("Отменить их переименование "
                      "(имя останется моим с обеих сторон)");
        }

        if(item.fromLocal)
            return tr("Отменить моё переименование "
                      "(имя станет их с обеих сторон)");
        return tr("Принять их переименование "
                 "(имя станет их с обеих сторон)");
    }

    if(item.category==Category::MyDelete)
    {
        if(up)
            return tr("Настоять на моём удалении "
                      "(не воскрешать — удалится и на сервере)");
        return tr("Воскресить с сервера (вернётся и ко мне)");
    }

    // TheirDelete
    if(up)
        return tr("Не удалять у меня "
                 "(моя копия воскресит их сторону)");
    return tr("Принять их удаление (удалится и у меня тоже)");
}


const SyncPreview::Preview &SyncPreviewDialog::meldPreviewData(void) const
{
    return m_preview;
}


const SyncPreview::Item *SyncPreviewDialog::meldPreviewItem(int index) const
{
    if(index<0 || index>=m_preview.items.size())
        return nullptr;

    return &m_preview.items.at(index);
}


SyncPreview::Action SyncPreviewDialog::meldEffectiveDecision(
        const Item &item) const
{
    return m_decisions.forItem(item);
}


bool SyncPreviewDialog::meldItemDanger(const QString &id) const
{
    return m_dangers.contains(id);
}


bool SyncPreviewDialog::meldItemSynced(const QString &id) const
{
    return isSynced(id);
}


int SyncPreviewDialog::meldItemSection(const Item &item) const
{
    return sectionOf(item);
}


// Цель стрелки для элемента (та же, что одиночный тап)
static bool meldTargetFor(const SyncPreview::Item &item, bool up,
                          SyncPreview::Action &target)
{
    return up ? meldUpTarget(item, target) : meldDownTarget(item, target);
}


void SyncPreviewDialog::meldBranchAction(const QModelIndex &node, bool up)
{
    if(m_meldModel==nullptr || !node.isValid())
        return;

    QList<int> sub;
    m_meldModel->collectSubtreeItems(node, sub);

    if(sub.isEmpty())
        return;

    // Цель — та же, что одиночный тап; direction в терминах meld
    auto targetFor=[up](const Item &item, Action &target) {
        return meldTargetFor(item, up, target);
    };

    // Всё уже в цели — снимаем обратно (как повторный тап)
    bool allSet=true;

    for(int index : sub)
    {
        const Item &item=m_preview.items.at(index);
        Action target=Action::Confirm;

        if(!targetFor(item, target)
           || m_decisions.forItem(item)!=target)
        {
            allSet=false;
            break;
        }
    }

    for(int index : sub)
    {
        const Item &item=m_preview.items.at(index);
        Action target=Action::Confirm;

        if(!targetFor(item, target))
            continue;

        m_decisions.byId[item.id]=allSet ? Action::Confirm : target;
    }

    refreshAll();
}


void SyncPreviewDialog::meldStageAll(bool up)
{
    if(m_meldModel==nullptr)
        return;

    // Всё видимое (фильтр вкладки уважаем, схлопнутое — тоже разбираем:
    // bulk честно покрывает направление целиком, не только экран)
    QList<int> sub;

    std::function<void(const QModelIndex &)> walk=[&](
            const QModelIndex &parent) {
        for(int row=0, rows=m_meldModel->rowCount(parent);
            row<rows; ++row)
        {
            const QModelIndex idx=m_meldModel->index(row, 0, parent);
            const int itemIndex=m_meldModel->itemIndex(idx);

            if(itemIndex>=0)
                sub << itemIndex;

            walk(idx);
        }
    };
    walk(QModelIndex());

    if(sub.isEmpty())
        return;

    bool allSet=true;

    for(int index : sub)
    {
        const Item &item=m_preview.items.at(index);
        Action target=Action::Confirm;

        if(!meldTargetFor(item, up, target)
           || m_decisions.forItem(item)!=target)
        {
            allSet=false;
            break;
        }
    }

    for(int index : sub)
    {
        const Item &item=m_preview.items.at(index);
        Action target=Action::Confirm;

        if(!meldTargetFor(item, up, target))
            continue;

        m_decisions.byId[item.id]=allSet ? Action::Confirm : target;
    }

    refreshAll();
}


void SyncPreviewDialog::onMeldContextMenu(const QPoint &pos)
{
    if(m_meldView==nullptr || m_meldModel==nullptr)
        return;

    const QModelIndex idx=m_meldView->indexAt(pos);

    if(!idx.isValid())
        return;

    QList<int> sub;
    m_meldModel->collectSubtreeItems(idx, sub);

    if(sub.isEmpty())
        return;

    int upCount=0, downCount=0;

    for(int index : sub)
    {
        const Item &item=m_preview.items.at(index);
        Action target=Action::Confirm;

        if(meldUpTarget(item, target))
            ++upCount;

        if(meldDownTarget(item, target))
            ++downCount;
    }

    QMenu menu(m_meldView);
    QAction *upAction=nullptr, *downAction=nullptr;

    if(upCount>0)
        upAction=menu.addAction(tr("→ Всё поддерево (%1)").arg(upCount));

    if(downCount>0)
        downAction=menu.addAction(tr("← Всё поддерево (%1)").arg(downCount));

    if(upAction==nullptr && downAction==nullptr)
        return;

    QAction *picked=menu.exec(m_meldView->viewport()->mapToGlobal(pos));

    if(picked==upAction)
        meldBranchAction(idx, true);
    else if(picked==downAction)
        meldBranchAction(idx, false);
}


void SyncPreviewDialog::onTabChanged(int index)
{
    // Вкладки — якоря-ссылки: тап скроллит к разделу, фильтра нет
    m_tab=index;

    if(m_meldMode && m_meldModel!=nullptr)
    {
        // В meld-режиме вкладки фильтруют: таблица показывает одно
        // направление целиком
        m_meldModel->setSection(index);

        if(m_meldView!=nullptr)
            m_meldView->expandAll();

        return;
    }

    if(index>=0 && index<3 && m_sectionHeaders[index]!=nullptr)
    {
        m_itemsTree->scrollToItem(m_sectionHeaders[index],
            QAbstractItemView::PositionAtTop);
        m_itemsTree->setCurrentItem(m_sectionHeaders[index]);
    }
}


void SyncPreviewDialog::onJumpClicked(void)
{
    // К первому неразобранному (Confirm == нет решения): прыжок + выбор
    if(m_meldMode && m_meldModel!=nullptr && m_meldView!=nullptr)
    {
        // DFS по модели (порядок строк); схлопнутое раскрываем по пути
        bool found=false;
        QModelIndex target;
        std::function<void(const QModelIndex &)> walk=[&](
                const QModelIndex &parent) {
            if(found)
                return;

            for(int row=0, rows=m_meldModel->rowCount(parent);
                row<rows && !found; ++row)
            {
                const QModelIndex idx=m_meldModel->index(row, 0,
                                                          parent);
                const int itemIndex=m_meldModel->itemIndex(idx);

                if(itemIndex>=0 && itemIndex<m_preview.items.size()
                   && m_decisions.forItem(
                       m_preview.items.at(itemIndex))==Action::Confirm)
                {
                    target=idx;
                    found=true;
                    return;
                }

                walk(idx);
            }
        };
        walk(QModelIndex());

        if(found && target.isValid())
        {
            // Раскрыть предков, иначе scrollTo в схлопнутое не попадёт
            for(QModelIndex p=target.parent(); p.isValid();
                p=p.parent())
                m_meldView->setExpanded(p, true);

            m_meldView->scrollTo(target,
                QAbstractItemView::PositionAtTop);
            m_meldView->setCurrentIndex(target);
        }

        return;
    }

    for(QTreeWidgetItemIterator it(m_itemsTree); *it; ++it)
    {
        QTreeWidgetItem *node=*it;
        const int itemIndex=node->data(0, Qt::UserRole).toInt();
        if(itemIndex<0 || itemIndex>=m_preview.items.size())
            continue;
        if(m_decisions.forItem(m_preview.items.at(itemIndex))==Action::Confirm)
        {
            m_itemsTree->scrollToItem(node,
                QAbstractItemView::PositionAtTop);
            m_itemsTree->setCurrentItem(node);
            break;
        }
    }
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


void SyncPreviewDialog::onMeldSelectionChanged(void)
{
    if(m_meldView==nullptr || m_meldModel==nullptr)
        return;

    const QModelIndex current=m_meldView->currentIndex();

    if(!current.isValid())
        return;

    const int itemIndex=m_meldModel->itemIndex(current);

    if(itemIndex>=0)
    {
        const Item &item=m_preview.items.at(itemIndex);
        const QString key=item.isBranch ? item.id : item.parentId;
        const QString title=item.isBranch ? item.name : item.parentName;
        showPanelForItem(itemIndex, key, title);
    }
    else
    {
        // Шапка группы: панель ветки без элемента
        showPanelForItem(-1, m_meldModel->headerKey(current),
                         current.sibling(current.row(), 0)
                             .data(Qt::DisplayRole).toString());
    }
}


int SyncPreviewDialog::currentItemIndex(void) const
{
    if(m_meldMode && m_meldView!=nullptr)
    {
        const QModelIndex current=m_meldView->currentIndex();

        if(current.isValid() && m_meldModel!=nullptr)
            return m_meldModel->itemIndex(current);

        return -1;
    }

    if(m_itemsTree==nullptr)
        return -1;

    const QList<QTreeWidgetItem *> selected=m_itemsTree->selectedItems();

    if(selected.isEmpty())
        return -1;

    const int itemIndex=selected.first()->data(0, Qt::UserRole).toInt();

    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return -1;

    return itemIndex;
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

    if(itemIndex>=0 && itemIndex<m_preview.items.size())
        showPanelForItem(itemIndex, branchKey, branchTitle);
    else if(!branchKey.isEmpty() || !branchTitle.isEmpty())
        showPanelForItem(-1, branchKey, branchTitle);
}


void SyncPreviewDialog::showPanelForItem(int itemIndex,
                                         const QString &branchKey,
                                         const QString &branchTitleFallback)
{
    const Item *current=(itemIndex>=0 && itemIndex<m_preview.items.size())
        ? &m_preview.items.at(itemIndex) : nullptr;

    QString branchTitle=branchTitleFallback;

    if(current!=nullptr)
    {
        if(current->isBranch)
            branchTitle=current->name;
        else
            branchTitle=current->parentName;
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
    // иначе побеждает более новая по mtime (размер при равных байтах молчит)
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
        hint=tr("оставляем наш вариант");
    else if(chosen==Action::TakeServer)
        hint=tr("перетираем с сервера");
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
    m_diffOwn.clear();
    m_diffOther.clear();
    m_diffHtml.clear();

    if(item!=nullptr
       && (item->category==Category::MyDelete
           || item->category==Category::TheirDelete))
    {
        m_diffView->setPlainText(tr("Элемент удалён, показывать нечего."));
        return;
    }

    if(item!=nullptr && item->isBranch)
    {
        // Ветки: диффим имена (текста у веток нет) — из самого айтема,
        // фетчеры для веток отдают пусто
        m_diffOwn=item->ownName;
        m_diffOther=item->otherName;

        if(m_diffOwn.isEmpty() && m_diffOther.isEmpty())
        {
            m_diffView->setPlainText(tr("Имени нет ни с одной стороны."));
            return;
        }

        if(!m_diffOwn.isEmpty() && m_diffOther.isEmpty())
        {
            m_diffView->setPlainText(
                m_diffOwn + QStringLiteral("\n\n")
                + tr("(на другой стороне такой ветки нет)"));
            return;
        }

        if(m_diffOwn.isEmpty() && !m_diffOther.isEmpty())
        {
            m_diffView->setPlainText(
                m_diffOther + QStringLiteral("\n\n")
                + tr("(у меня такой ветки нет)"));
            return;
        }

        // before — чужая версия, after — моя (как у правок текста)
        const QString bBefore=item->fromLocal ? m_diffOther : m_diffOwn;
        const QString bAfter=item->fromLocal ? m_diffOwn : m_diffOther;
        const QList<SyncDiff::Line> bLines=SyncDiff::diffLines(
            SyncDiff::visibleLines(bBefore), SyncDiff::visibleLines(bAfter));

        const QString green=QStringLiteral("#43A047");
        const QString red=QStringLiteral("#E53935");
        QString bHtml=QStringLiteral("<pre style='margin:0; font-family: monospace;'>");
        int bAdded=0, bRemoved=0;

        for(const SyncDiff::Line &line : bLines)
        {
            const QString esc=line.text.toHtmlEscaped();

            switch(line.kind)
            {
            case SyncDiff::Line::Equal:
                bHtml+=esc+QStringLiteral("<br>");
                break;
            case SyncDiff::Line::Added:
                bHtml+=QStringLiteral("<font color=\"%1\">+%2</font><br>")
                    .arg(green).arg(esc);
                bAdded++;
                break;
            default:
                bHtml+=QStringLiteral("<font color=\"%1\">-%2</font><br>")
                    .arg(red).arg(esc);
                bRemoved++;
                break;
            }
        }

        bHtml+=QStringLiteral("<font color=\"%1\">+%2</font> <font color=\"%3\">-%4</font>")
            .arg(green).arg(bAdded).arg(red).arg(bRemoved);
        bHtml+=QStringLiteral("</pre>");

        m_diffHtml=bHtml;
        refreshDiffView();
        return;
    }

    if(item==nullptr || item->category!=Category::Edit)
    {
        m_diffView->setPlainText(
            tr("Выберите измененную запись, чтобы увидеть diff."));
        return;
    }

    const QString own=m_ownText ? m_ownText(*item) : QString();
    const QString other=m_otherText ? m_otherText(*item) : QString();

    if(other.isEmpty())
    {
        m_diffOwn.clear();
        m_diffOther.clear();
        m_diffHtml.clear();
        m_diffView->setPlainText(tr("Текст другой стороны недоступен."));
        return;
    }

    // Для наших правок: ихняя версия -> наша; для ихних: наша -> ихняя
    const QString before=item->fromLocal ? other : own;
    const QString after=item->fromLocal ? own : other;

    // Диффим видимое (без HTML-мусора редакторов), картинки —
    // заглушками "[картинка: имя]" (как в мобильной версии)
    const QList<SyncDiff::Line> lines=SyncDiff::diffLines(
        SyncDiff::visibleLines(before), SyncDiff::visibleLines(after));

    // Git-раскраска как в мобильной версии: + зелёным, − красным,
    // шапка и счётчик — своим цветом. QTextEdit вместо Plain:
    // нужен rich text, текст экранируется
    const QString green=QStringLiteral("#43A047");
    const QString red=QStringLiteral("#E53935");
    QString html=QStringLiteral("<pre style='margin:0; font-family: monospace;'>");

    auto esc=[](const QString &t) { return t.toHtmlEscaped(); };

    if(m_dangers.contains(item->id))
        html+=QStringLiteral("<b><font color=\"%1\">\u26A0 %2</font></b><br>").arg(
            red, tr("Защита от бития: с одной стороны почти пустой текст, дефолт — оставить полную.").toHtmlEscaped());

    html+=QStringLiteral("<b>%1</b><br>").arg(
        tr("=== %1 (другая сторона) -> %2 (это устройство) ===")
        .arg(tr("старое"), tr("новое")).toHtmlEscaped());

    int added=0, removed=0;

    for(const SyncDiff::Line &line : lines)
    {
        switch(line.kind)
        {
        case SyncDiff::Line::Equal:
            html+=QStringLiteral("  %1<br>").arg(esc(line.text));
            break;
        case SyncDiff::Line::Removed:
            ++removed;
            html+=QStringLiteral("<font color=\"%1\">- %2</font><br>").arg(red, esc(line.text));
            break;
        case SyncDiff::Line::Added:
            ++added;
            html+=QStringLiteral("<font color=\"%1\">+ %2</font><br>").arg(green, esc(line.text));
            break;
        }
    }

    html+=QStringLiteral("<font color=\"%1\">+%2</font> <font color=\"%3\">-%4</font>")
        .arg(green).arg(added).arg(red).arg(removed);
    html+=QStringLiteral("</pre>");

    // Три вкладки: полные видимые тексты обеих сторон + diff.
    // "Наша" — моя версия (это устройство), "Ихняя" — другая сторона
    m_diffOwn=SyncDiff::visibleLines(
        item->fromLocal ? after : before).join(QStringLiteral("\n"));
    m_diffOther=SyncDiff::visibleLines(
        item->fromLocal ? before : after).join(QStringLiteral("\n"));
    m_diffHtml=html;
    refreshDiffView();
}


void SyncPreviewDialog::refreshDiffView(void)
{
    if(m_diffMode==0 && !m_diffOwn.isEmpty())
        m_diffView->setPlainText(m_diffOwn);
    else if(m_diffMode==1 && !m_diffOther.isEmpty())
        m_diffView->setPlainText(m_diffOther);
    else if(!m_diffHtml.isEmpty())
        m_diffView->setHtml(m_diffHtml);
    else
        m_diffView->clear();
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
    html+=QStringLiteral("<h3>")+tr("Действия по умолчанию")+QStringLiteral("</h3>");
    html+=sym(QStringLiteral("✓"), green, tr("Применить — выполнить запланированное молча"));
    html+=sym(QStringLiteral("✓"), green, tr("Предоставить решать мне — то же, но только после моего взгляда"));
    html+=sym(QStringLiteral("⚠"), darkred, tr("Красное с ⚠ в списке — данные будут удалены или затёрты: удаление здесь/на сервере, затирание полной версии пустой"));
    html+=sym(QStringLiteral("•"), purple, tr("Восстановить с сервера — отменить моё, взять их версию (моё удаление — воскресить)"));
    html+=sym(QStringLiteral("•"), purple, tr("Настоять на моём — отменить ихнее, оставить мою версию (их удаление — воскресить мою копию наверх)"));
    html+=QStringLiteral("<h3>")+tr("Вкладки")+QStringLiteral("</h3>");
    html+=sym(QStringLiteral("↑"), green, tr("Я → сервер: мои изменения уедут наружу"));
    html+=sym(QStringLiteral("↓"), blue, tr("Сервер → я: ихние изменения приедут к нам"));
    html+=sym(QStringLiteral("⇅"), orange, tr("Обоюдные правки: текст разошелся с обеих сторон. Кнопками «Наша»/«Ихняя» выбираете, чья версия победит. Эвристика предлагает более новую, но решаете вы"));
    html+=QStringLiteral("<h3>")+tr("Кнопки и галочки")+QStringLiteral("</h3>");
    html+=sym(QStringLiteral("•"), green, tr("«Запомнить» — сохранить выбранные действия как умолчание для будущих синков"));
    html+=sym(QStringLiteral("•"), green, tr("«Действия по умолчанию ▾» — действия по умолчанию для целых категорий"));
    html+=sym(QStringLiteral("✓"), green, tr("Галка у строки — решение принято (безопасные предзаполнены); «Применить» активно при хоть одном решении: всё разобрано — закроет окно, иначе синкнет решённое, а дерево сузится до остатка"));
    html+=sym(QStringLiteral("•"), green, tr("«Применить» — выполнить, «Отмена» — прервать"));

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
    Decisions result=m_decisions;

    // Уже синкнутое частичным «Применить» в финальный план не входит
    // (applyDecisions вычеркнет Skip, наборы не пересекаются)
    for(const QString &id : m_syncedIds)
        result.byId[id]=Action::Skip;

    return result;
}


bool SyncPreviewDialog::wantSaveDefaults(void) const
{
    return m_saveDefaults->isChecked();
}

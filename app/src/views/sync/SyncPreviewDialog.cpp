#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
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
    setWindowTitle(tr("Sync preview"));
    resize(760, 560);

    QVBoxLayout *layout=new QVBoxLayout(this);

    layout->addWidget(new QLabel(
        tr("What this sync will do.\n"
           "Safe changes (added) apply silently, "
           "everything else needs your choice."), this));

    // --- категории с действиями ---
    QFormLayout *categories=new QFormLayout();

    for(int i=0; i<5; ++i)
    {
        const Category category=static_cast<Category>(i);
        const int count=m_preview.count(category);

        QComboBox *combo=new QComboBox(this);

        for(int a=0; a<5; ++a)
            combo->addItem(SyncPreview::actionName(static_cast<Action>(a)));

        combo->setCurrentIndex(static_cast<int>(m_decisions.category.value(
            category, Action::Confirm)));
        combo->setEnabled(count>0);

        connect(combo,
                static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &SyncPreviewDialog::onCategoryActionChanged);

        m_categoryActions[i]=combo;

        QLabel *label=new QLabel(
            QStringLiteral("%1 (%2)")
                .arg(SyncPreview::categoryName(category)).arg(count), this);

        if(count>0 && category!=Category::Create)
        {
            QFont font=label->font();
            font.setBold(true);
            label->setFont(font);
        }

        categories->addRow(label, combo);
    }

    layout->addLayout(categories);

    // --- элементы с точечными действиями ---
    m_itemsTree=new QTreeWidget(this);
    m_itemsTree->setColumnCount(3);
    m_itemsTree->setHeaderLabels(QStringList()
                                 << tr("Item") << tr("Action") << tr("Info"));
    m_itemsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_itemsTree->setRootIsDecorated(true);
    layout->addWidget(m_itemsTree, 2);

    buildItems();

    connect(m_itemsTree, &QTreeWidget::itemSelectionChanged,
            this, &SyncPreviewDialog::onItemSelectionChanged);

    // --- diff ---
    m_diffView=new QPlainTextEdit(this);
    m_diffView->setReadOnly(true);
    m_diffView->setPlaceholderText(
        tr("Select a modified note to see the diff."));
    layout->addWidget(m_diffView, 2);

    m_saveDefaults=new QCheckBox(
        tr("Remember these actions as defaults"), this);
    layout->addWidget(m_saveDefaults);

    QDialogButtonBox *buttons=new QDialogButtonBox(this);
    QPushButton *syncButton=buttons->addButton(
        tr("Synchronize"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    syncButton->setDefault(true);
}


void SyncPreviewDialog::buildItems(void)
{
    m_itemsTree->clear();

    for(int i=0; i<5; ++i)
    {
        const Category category=static_cast<Category>(i);
        const int count=m_preview.count(category);

        if(count==0)
            continue;

        QTreeWidgetItem *group=new QTreeWidgetItem(m_itemsTree);
        group->setText(0, QStringLiteral("%1 (%2)")
                           .arg(SyncPreview::categoryName(category)).arg(count));
        group->setFirstColumnSpanned(true);
        QFont font=group->font(0);
        font.setBold(true);
        group->setFont(0, font);

        for(int index=0; index<m_preview.items.size(); ++index)
        {
            const Item &item=m_preview.items.at(index);

            if(item.category!=category)
                continue;

            QTreeWidgetItem *row=new QTreeWidgetItem(group);
            row->setText(0, item.name.isEmpty()
                                ? tr("(without name)") : item.name);
            row->setData(0, Qt::UserRole, index);

            QComboBox *combo=new QComboBox(m_itemsTree);

            for(int a=0; a<5; ++a)
                combo->addItem(SyncPreview::actionName(static_cast<Action>(a)));

            combo->setCurrentIndex(static_cast<int>(m_decisions.forItem(item)));
            combo->setProperty("itemIndex", index);

            connect(combo,
                    static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    this, &SyncPreviewDialog::onItemActionChanged);

            m_itemsTree->setItemWidget(row, 1, combo);

            row->setText(2, item.isBranch ? tr("branch") : tr("note"));
        }

        group->setExpanded(true);
    }
}


void SyncPreviewDialog::onCategoryActionChanged(int index)
{
    QComboBox *combo=qobject_cast<QComboBox *>(sender());

    if(combo==nullptr)
        return;

    for(int i=0; i<5; ++i)
        if(m_categoryActions[i]==combo)
        {
            m_decisions.category[static_cast<Category>(i)]=
                static_cast<Action>(index);
            break;
        }

    // Точечные действия, которые пользователь не менял, следуют категории
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
}


void SyncPreviewDialog::onItemSelectionChanged(void)
{
    const QList<QTreeWidgetItem *> selected=m_itemsTree->selectedItems();

    if(selected.isEmpty())
        return;

    showDiffFor(selected.first());
}


void SyncPreviewDialog::showDiffFor(QTreeWidgetItem *row)
{
    if(row==nullptr)
        return;

    const int itemIndex=row->data(0, Qt::UserRole).toInt();

    if(itemIndex<0 || itemIndex>=m_preview.items.size())
        return;

    const Item &item=m_preview.items.at(itemIndex);

    // Diff есть только у измененного содержимого записей
    if(item.category!=Category::Edit || item.isBranch)
    {
        m_diffView->setPlainText(
            tr("Diff is only available for modified notes."));
        return;
    }

    const QString own=m_ownText ? m_ownText(item) : QString();
    const QString other=m_otherText ? m_otherText(item) : QString();

    if(other.isEmpty())
    {
        m_diffView->setPlainText(tr("The other side's text is unavailable."));
        return;
    }

    // Для наших правок: ихняя версия -> наша; для ихних: наша -> ихняя
    const QString before=item.fromLocal ? other : own;
    const QString after=item.fromLocal ? own : other;

    const QList<SyncDiff::Line> lines=SyncDiff::diff(before, after);

    QStringList out;
    out << tr("=== %1 (other side) -> %2 (this device) ===")
               .arg(tr("old"), tr("new"));

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


Decisions SyncPreviewDialog::decisions(void) const
{
    return m_decisions;
}


bool SyncPreviewDialog::wantSaveDefaults(void) const
{
    return m_saveDefaults->isChecked();
}

#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QTableWidgetItem>

#include "TagsPanel.h"

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/KnowTreeView.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/GlobalParameters.h"

extern GlobalParameters globalParameters;


TagsPanel::TagsPanel(QWidget *parent) : QWidget(parent)
{
    setupUi();
    assembly();
    setupSignals();
}


TagsPanel::~TagsPanel(void)
{

}


void TagsPanel::setupUi(void)
{
    // Строка отбора тегов по подстроке
    filterEdit=new QLineEdit(this);
    filterEdit->setPlaceholderText(tr("Filter tags"));
    filterEdit->setClearButtonEnabled(true);

    // Таблица тег и количество заметок с ним
    tagsTable=new QTableWidget(this);
    tagsTable->setColumnCount(2);
    tagsTable->setHorizontalHeaderLabels(QStringList() << tr("Tag") << tr("Count"));
    tagsTable->horizontalHeader()->setStretchLastSection(true);
    tagsTable->verticalHeader()->setVisible(false);
    tagsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tagsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    tagsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}


void TagsPanel::assembly(void)
{
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->setContentsMargins(2, 2, 2, 2);
    centralLayout->addWidget(filterEdit);
    centralLayout->addWidget(tagsTable);

    this->setLayout(centralLayout);
}


void TagsPanel::setupSignals(void)
{
    connect(filterEdit, &QLineEdit::textChanged,
            this,        &TagsPanel::onFilterChanged);

    connect(tagsTable, &QTableWidget::cellClicked,
            this,      &TagsPanel::onTagClicked);
}


void TagsPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshTags();
}


// Пересборка таблицы по всему дереву
void TagsPanel::refreshTags(void)
{
    QMap<QString, int> counts;
    QMap<QString, QString> display;

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView!=nullptr)
    {
        KnowTreeModel *treeModel=static_cast<KnowTreeModel*>(treeView->model());

        const TreeItem *rootItem=treeModel->getRootItem();

        if(rootItem!=nullptr)
            collectTagCounts(rootItem, counts, display);
    }

    // Теги по алфавиту без учета регистра
    QStringList ordered=display.keys();
    ordered.sort(Qt::CaseInsensitive);

    tagsTable->setRowCount(0);
    tagsTable->setRowCount(ordered.size());

    for(int i=0; i<ordered.size(); i++)
    {
        QTableWidgetItem *tagItem=new QTableWidgetItem(display.value(ordered.at(i)));
        QTableWidgetItem *countItem=new QTableWidgetItem(QString::number(counts.value(ordered.at(i))));

        tagsTable->setItem(i, 0, tagItem);
        tagsTable->setItem(i, 1, countItem);
    }

    tagsTable->resizeColumnToContents(0);

    onFilterChanged(filterEdit->text());
}


// Отбор строк по подстроке без учета регистра
void TagsPanel::onFilterChanged(const QString &text)
{
    for(int i=0; i<tagsTable->rowCount(); i++)
    {
        QTableWidgetItem *tagItem=tagsTable->item(i, 0);

        bool visible=text.isEmpty() ||
                     tagItem->text().contains(text, Qt::CaseInsensitive);

        tagsTable->setRowHidden(i, !visible);
    }
}


// Клик по тегу запускает глобальный поиск как клик по тегу в заметке
void TagsPanel::onTagClicked(int row, int column)
{
    Q_UNUSED(column);

    QTableWidgetItem *tagItem=tagsTable->item(row, 0);

    if(tagItem==nullptr)
        return;

    FindScreen *findScreen=find_object<FindScreen>("findScreenDisp");

    if(findScreen==nullptr)
        return;

    if(!findScreen->isVisible())
        findScreen->widgetShow();

    findScreen->setFindText(tagItem->text());
}


// Собрать словарь тег->количество по ветке и подветкам.
// Регистр сводится: пишется первое встречное написание, счет суммируется.
// Зашифрованные ветки без пароля пропускаются как в поиске
void TagsPanel::collectTagCounts(const TreeItem *curritem,
                                 QMap<QString, int> &counts,
                                 QMap<QString, QString> &display)
{
    if(curritem==nullptr)
        return;

    // Зашифрованная ветка без введенного пароля недоступна
    if(curritem->getField("crypt")=="1" &&
       globalParameters.getCryptKey().length()==0)
        return;

    if(curritem->recordtableGetRowCount() > 0)
    {
        const RecordTableData *recordTable=curritem->recordtableGetTableData();

        for(int i=0; i<static_cast<int>(recordTable->size()); i++)
        {
            QStringList recordTags=FindScreen::splitRecordTags(recordTable->getField("tags", i));

            for(int t=0; t<recordTags.size(); t++)
            {
                QString lowered=recordTags.at(t).toLower();

                if(!display.contains(lowered))
                    display[lowered]=recordTags.at(t);

                counts[lowered]++;
            }
        }
    }

    for(int i=0; i<curritem->childCount(); i++)
        collectTagCounts(curritem->child(i), counts, display);
}

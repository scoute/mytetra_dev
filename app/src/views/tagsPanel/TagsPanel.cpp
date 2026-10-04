#include <QLineEdit>
#include <QTableWidget>
#include <QAbstractScrollArea>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QTableWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QDockWidget>

#include "TagsPanel.h"

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/KnowTreeView.h"
#include "views/tree/TreeScreen.h"
#include "views/record/MetaEditor.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/GlobalParameters.h"

extern GlobalParameters globalParameters;


TagsPanel::TagsPanel(QWidget *parent) : QWidget(parent),
    treeMetadataConnected(false)
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

    // Таблица тег и количество заметок с ним. Строки минимальные
    // чтобы больше влезало. Заголовок у колонки количества пустой:
    // и так понятно что цифры это количество, зато экономия места.
    // Обе колонки по содержимому, без растягивания на всю ширину
    tagsTable=new QTableWidget(this);
    tagsTable->setColumnCount(2);
    tagsTable->setHorizontalHeaderLabels(QStringList() << tr("Tag") << QString());
    tagsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tagsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tagsTable->verticalHeader()->setVisible(false);

    // Таблица подстраивает свой размер под содержимое: док справа
    // обнимает колонки и не занимает лишнюю ширину
    tagsTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    int rowHeight=tagsTable->fontMetrics().height()+2;
    tagsTable->verticalHeader()->setMinimumSectionSize(rowHeight);
    tagsTable->verticalHeader()->setDefaultSectionSize(rowHeight);

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

    // Одинарный клик только выделяет строку средствами таблицы.
    // Глобальный поиск запускается двойным кликом чтобы не сбивать
    // текущий поиск случайным одинарным кликом
    connect(tagsTable, &QTableWidget::cellDoubleClicked,
            this,      &TagsPanel::onTagClicked);

    tagsTable->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tagsTable, &QTableWidget::customContextMenuRequested,
            this,      &TagsPanel::onTagsContextMenu);
}


void TagsPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshTags();
}


// Пересборка таблицы по всему дереву
void TagsPanel::refreshTags(void)
{
    // Живое обновление: метаданные дерева сохраняются при любом
    // изменении тегов записи, панель пересобирается следом.
    // Подписка ленивая и однократная: в конструкторе treeScreen
    // может еще не существовать, а showEvent уже поздно не бывает
    if(!treeMetadataConnected)
    {
        TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

        if(treeScreen!=nullptr)
        {
            treeMetadataConnected=true;

            connect(treeScreen, &TreeScreen::treeMetadataSaved,
                    this,        &TagsPanel::refreshTags);
        }
    }

    QMap<QString, int> counts;
    QMap<QString, QString> display;

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    KnowTreeModel *treeModel=nullptr;
    const TreeItem *rootItem=nullptr;

    if(treeView!=nullptr)
    {
        treeModel=static_cast<KnowTreeModel*>(treeView->model());
        rootItem=treeModel->getRootItem();

        if(rootItem!=nullptr)
            collectTagCounts(rootItem, counts, display);
    }

    // Заголовок дока с общим количеством заметок
    if(treeModel!=nullptr)
    {
        if(QDockWidget *dock=qobject_cast<QDockWidget *>(parentWidget()))
            dock->setWindowTitle(tr("Tags [%1]").arg(treeModel->getAllRecordCount()));
    }

    // Запоминается выделенная метка: живые обновления пересобирают
    // таблицу, а сбрасывать выбор пользователя нельзя
    QString selectedTag;
    int selectedRow=tagsTable->currentRow();

    if(selectedRow>=0)
    {
        QTableWidgetItem *selectedItem=tagsTable->item(selectedRow, 0);

        if(selectedItem!=nullptr)
            selectedTag=selectedItem->text();
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

    tagsTable->resizeColumnsToContents();

    restoreTagSelection(selectedRow, selectedTag);

    onFilterChanged(filterEdit->text());
}


// Восстановить выделение метки после пересборки таблицы.
// Ищется та же строка, затем вся таблица: метка могла съехать
// при сортировке. Выделение программное и поиск не запускает:
// клик по тегу обрабатывается только через cellClicked
void TagsPanel::restoreTagSelection(int selectedRow, const QString &selectedTag)
{
    if(selectedTag.isEmpty())
        return;

    int rowToSelect=-1;
    int rowCount=tagsTable->rowCount();

    if(selectedRow>=0 && selectedRow<rowCount)
    {
        QTableWidgetItem *candidate=tagsTable->item(selectedRow, 0);

        if(candidate!=nullptr && candidate->text()==selectedTag)
            rowToSelect=selectedRow;
    }

    if(rowToSelect<0)
    {
        for(int i=0; i<rowCount; i++)
        {
            QTableWidgetItem *candidate=tagsTable->item(i, 0);

            if(candidate!=nullptr && candidate->text()==selectedTag)
            {
                rowToSelect=i;
                break;
            }
        }
    }

    if(rowToSelect>=0)
    {
        tagsTable->selectRow(rowToSelect);
        tagsTable->scrollToItem(tagsTable->item(rowToSelect, 0));
    }
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


// Двойной клик по тегу запускает глобальный поиск как клик по тегу в заметке
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


// Контекстное меню строки: переименовать и удалить тег.
// Слияние имен отложено и здесь блокируется
void TagsPanel::onTagsContextMenu(const QPoint &pos)
{
    int row=tagsTable->rowAt(pos.y());

    if(row<0)
        return;

    tagsTable->selectRow(row);

    QMenu menu(this);
    QAction *renameAction=menu.addAction(tr("Rename tag..."));
    QAction *deleteAction=menu.addAction(tr("Delete tag"));

    QAction *chosen=menu.exec(tagsTable->viewport()->mapToGlobal(pos));

    if(chosen==renameAction)
        onRenameTag();
    else if(chosen==deleteAction)
        onDeleteTag();
}


// Переименование тега во всех записях с диалогом подтверждения.
// Новое имя обязано быть одним тегом без разделителей. Переименование
// в существующее имя блокируется: это слияние, оно отложено
void TagsPanel::onRenameTag(void)
{
    int row=tagsTable->currentRow();
    QTableWidgetItem *tagItem=(row>=0) ? tagsTable->item(row, 0) : nullptr;

    if(tagItem==nullptr)
        return;

    QString oldSpelling=tagItem->text();
    QString oldLower=oldSpelling.toLower();

    bool ok=false;
    QString newSpelling=QInputDialog::getText(this,
                                             tr("Rename tag"),
                                             tr("New name for tag \"%1\":").arg(oldSpelling),
                                             QLineEdit::Normal,
                                             oldSpelling,
                                             &ok);

    if(!ok)
        return;

    newSpelling=newSpelling.trimmed();

    if(newSpelling==oldSpelling)
        return;

    if(newSpelling.isEmpty())
    {
        QMessageBox::information(this,
                                 tr("Rename tag"),
                                 tr("Empty name removes nothing. Use Delete tag to remove it."));
        return;
    }

    if(newSpelling.contains(',') || newSpelling.contains(';'))
    {
        QMessageBox::information(this,
                                 tr("Rename tag"),
                                 tr("Name must be a single tag without comma or semicolon."));
        return;
    }

    // Записи с тегом собираются до диалога чтобы показать количество
    QList< QPair<TreeItem *, int> > targets;
    collectTagTargetsFromTree(oldLower, targets);

    if(targets.isEmpty())
    {
        refreshTags();
        return;
    }

    // Слияние в существующий тег блокируется
    if(tagExistsInBase(newSpelling.toLower(), oldLower))
    {
        QMessageBox::information(this,
                                 tr("Rename tag"),
                                 tr("Tag \"%1\" already exists. Merging is not implemented yet.").arg(newSpelling));
        return;
    }

    if(QMessageBox::question(this,
                             tr("Rename tag"),
                             tr("Rename tag \"%1\" to \"%2\" in %3 note(s)? Undo is not available.").arg(oldSpelling).arg(newSpelling).arg(targets.size()),
                             QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    for(int i=0; i<targets.size(); i++)
    {
        RecordTableData *table=targets.at(i).first->recordtableGetTableData();
        int recordRow=targets.at(i).second;

        QStringList tags=FindScreen::splitRecordTags(table->getField("tags", recordRow));
        bool changed=false;
        QStringList renamed=replaceTagInList(tags, oldLower, newSpelling, changed);

        if(changed)
        {
            QMap<QString, QString> editFields;
            editFields["tags"]=renamed.join(", ");
            table->editRecordFields(recordRow, editFields);
        }
    }

    saveBaseAndRefresh(oldLower, newSpelling);
}


// Удаление тега из всех записей с диалогом подтверждения
void TagsPanel::onDeleteTag(void)
{
    int row=tagsTable->currentRow();
    QTableWidgetItem *tagItem=(row>=0) ? tagsTable->item(row, 0) : nullptr;

    if(tagItem==nullptr)
        return;

    QString oldSpelling=tagItem->text();
    QString oldLower=oldSpelling.toLower();

    QList< QPair<TreeItem *, int> > targets;
    collectTagTargetsFromTree(oldLower, targets);

    if(targets.isEmpty())
    {
        refreshTags();
        return;
    }

    if(QMessageBox::question(this,
                             tr("Delete tag"),
                             tr("Remove tag \"%1\" from %2 note(s)? Undo is not available.").arg(oldSpelling).arg(targets.size()),
                             QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    for(int i=0; i<targets.size(); i++)
    {
        RecordTableData *table=targets.at(i).first->recordtableGetTableData();
        int recordRow=targets.at(i).second;

        QStringList tags=FindScreen::splitRecordTags(table->getField("tags", recordRow));
        bool changed=false;
        QStringList cleaned=removeTagFromList(tags, oldLower, changed);

        if(changed)
        {
            QMap<QString, QString> editFields;
            editFields["tags"]=cleaned.join(", ");
            table->editRecordFields(recordRow, editFields);
        }
    }

    saveBaseAndRefresh(oldLower, QString());
}


// Собрать записи с тегом через дерево. Пустой список значит тег уже исчез
void TagsPanel::collectTagTargetsFromTree(const QString &tagLower,
                                          QList< QPair<TreeItem *, int> > &targets)
{
    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView==nullptr)
        return;

    KnowTreeModel *treeModel=static_cast<KnowTreeModel*>(treeView->model());

    // Объекты дерева внутри модели неконстантны, константен только доступ
    TreeItem *rootItem=const_cast<TreeItem *>(treeModel->getRootItem());

    if(rootItem!=nullptr)
        collectTagTargets(rootItem, tagLower, targets);
}


// Есть ли в базе тег кроме переименовываемого. Нужно для блокировки слияния
bool TagsPanel::tagExistsInBase(const QString &tagLower, const QString &excludeLower)
{
    if(tagLower==excludeLower)
        return false;

    QMap<QString, int> counts;
    QMap<QString, QString> display;

    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView==nullptr)
        return false;

    KnowTreeModel *treeModel=static_cast<KnowTreeModel*>(treeView->model());
    const TreeItem *rootItem=treeModel->getRootItem();

    if(rootItem==nullptr)
        return false;

    collectTagCounts(rootItem, counts, display);

    return display.contains(tagLower);
}


// Сохранить базу, обновить панель и строку меток открытой заметки
void TagsPanel::saveBaseAndRefresh(const QString &oldLower, const QString &newSpelling)
{
    // Сохранение дерева веток тем же путем что правка полей записи
    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

    if(treeScreen!=nullptr)
        treeScreen->saveKnowTree();

    refreshTags();

    // Строка меток открытой заметки обновляется если тег был в ней
    MetaEditor *metaEditor=find_object<MetaEditor>("editorScreen");

    if(metaEditor==nullptr)
        return;

    QStringList currentTags=metaEditor->getTagsList();
    bool changed=false;
    QStringList updated;

    if(newSpelling.isEmpty())
        updated=removeTagFromList(currentTags, oldLower, changed);
    else
        updated=replaceTagInList(currentTags, oldLower, newSpelling, changed);

    if(changed)
        metaEditor->setTags(updated.join(", "));
}


// Собрать записи с тегом: пары ветка и строка таблицы.
// Сравнение без учета регистра. Зашифрованные ветки пропускаются
void TagsPanel::collectTagTargets(TreeItem *curritem,
                                  const QString &tagLower,
                                  QList< QPair<TreeItem *, int> > &targets)
{
    if(curritem==nullptr)
        return;

    // Зашифрованная ветка без введенного пароля недоступна
    if(curritem->getField("crypt")=="1" &&
       globalParameters.getCryptKey().length()==0)
        return;

    if(curritem->recordtableGetRowCount() > 0)
    {
        RecordTableData *recordTable=curritem->recordtableGetTableData();

        for(int i=0; i<static_cast<int>(recordTable->size()); i++)
        {
            QStringList recordTags=FindScreen::splitRecordTags(recordTable->getField("tags", i));

            for(int t=0; t<recordTags.size(); t++)
            {
                if(recordTags.at(t).toLower()==tagLower)
                {
                    targets.append(qMakePair(curritem, i));
                    break;
                }
            }
        }
    }

    for(int i=0; i<curritem->childCount(); i++)
        collectTagTargets(curritem->child(i), tagLower, targets);
}


// Заменить тег в списке целиком без учета регистра.
// Возвращает новый список, в changed было ли изменение
QStringList TagsPanel::replaceTagInList(const QStringList &tags,
                                        const QString &oldLower,
                                        const QString &newSpelling,
                                        bool &changed)
{
    changed=false;
    QStringList result;

    for(int i=0; i<tags.size(); i++)
    {
        if(tags.at(i).toLower()==oldLower)
        {
            result.append(newSpelling);
            changed=true;
        }
        else
            result.append(tags.at(i));
    }

    return result;
}


// Убрать тег из списка целиком без учета регистра.
// Возвращает новый список, в changed было ли изменение
QStringList TagsPanel::removeTagFromList(const QStringList &tags,
                                         const QString &oldLower,
                                         bool &changed)
{
    changed=false;
    QStringList result;

    for(int i=0; i<tags.size(); i++)
    {
        if(tags.at(i).toLower()==oldLower)
            changed=true;
        else
            result.append(tags.at(i));
    }

    return result;
}

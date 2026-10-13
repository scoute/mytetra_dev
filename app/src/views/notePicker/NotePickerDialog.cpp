#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QTimer>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSet>
#include <QDebug>

#include "views/notePicker/NotePickerDialog.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "views/tree/KnowTreeView.h"


extern GlobalParameters globalParameters;


NotePickerDialog::NotePickerDialog(QWidget *parent,
                                   const QString &excludeId) : QDialog(parent),
  excludeRecordId(excludeId)
{
  setWindowTitle(tr("Select note"));

  filterEdit=new QLineEdit(this);
  filterEdit->setPlaceholderText(tr("Search note"));
  filterEdit->setClearButtonEnabled(true);

  resultsTable=new QTableWidget(this);
  resultsTable->setColumnCount(ColumnCount);
  resultsTable->setHorizontalHeaderLabels(QStringList() << tr("Note") << tr("Branch") << tr("Tags"));
  resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
  resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  resultsTable->setSortingEnabled(true);
  resultsTable->horizontalHeader()->setSectionResizeMode(ColumnName, QHeaderView::Stretch);
  resultsTable->horizontalHeader()->setSectionResizeMode(ColumnBranch, QHeaderView::ResizeToContents);
  resultsTable->horizontalHeader()->setSectionResizeMode(ColumnTags, QHeaderView::ResizeToContents);

  counterLabel=new QLabel(this);

  QPushButton *selectButton=new QPushButton(tr("Select"), this);
  selectButton->setDefault(true);
  QPushButton *cancelButton=new QPushButton(tr("Cancel"), this);

  QDialogButtonBox *buttons=new QDialogButtonBox(this);
  buttons->addButton(selectButton, QDialogButtonBox::AcceptRole);
  buttons->addButton(cancelButton, QDialogButtonBox::RejectRole);

  connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    if(resultsTable->currentRow()>=0)
      accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QVBoxLayout *layout=new QVBoxLayout(this);
  layout->addWidget(filterEdit);
  layout->addWidget(resultsTable);
  layout->addWidget(counterLabel);
  layout->addWidget(buttons);
  setLayout(layout);

  resize(620, 380);

  filterTimer=new QTimer(this);
  filterTimer->setSingleShot(true);
  filterTimer->setInterval(150);

  connect(filterEdit, &QLineEdit::textChanged,
          this,        &NotePickerDialog::scheduleFilter);
  connect(filterTimer, &QTimer::timeout,
          this,        &NotePickerDialog::rebuildResults);
  connect(resultsTable, &QTableWidget::itemActivated,
          this,        &NotePickerDialog::onItemActivated);
  connect(resultsTable, &QTableWidget::itemDoubleClicked,
          this,        &NotePickerDialog::onItemActivated);

  counterLabel->setText(tr("Type a name, branch or tag to search"));
}


QString NotePickerDialog::selectedRecordId(void) const
{
  QTableWidgetItem *item=resultsTable->item(resultsTable->currentRow(), ColumnName);
  if(item==nullptr)
    return QString();

  return item->data(Qt::UserRole).toString();
}


QString NotePickerDialog::selectedRecordName(void) const
{
  QTableWidgetItem *item=resultsTable->item(resultsTable->currentRow(), ColumnName);
  if(item==nullptr)
    return QString();

  return item->text();
}


void NotePickerDialog::applyFilter(const QString &text)
{
  filterTimer->stop();
  filterEdit->setText(text);
  rebuildResults();
}


void NotePickerDialog::setCurrentRow(int row)
{
  resultsTable->selectRow(row);
}


int NotePickerDialog::resultCount(void) const
{
  return resultsTable->rowCount();
}


QString NotePickerDialog::counterText(void) const
{
  return counterLabel->text();
}


void NotePickerDialog::scheduleFilter(void)
{
  filterTimer->start();
}


void NotePickerDialog::onItemActivated(QTableWidgetItem *item)
{
  if(item==nullptr)
    return;

  resultsTable->setCurrentCell(item->row(), ColumnName);
  accept();
}


bool NotePickerDialog::isRecordSkippable(const QString &recordId) const
{
  if(recordId==excludeRecordId)
    return true;

  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");
  if(treeView==nullptr)
    return true;

  KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

  // Зашифрованные ветки без пароля пропускаются как в поиске
  const QStringList branchPath=treeModel->getRecordPath(recordId);
  if(branchPath.isEmpty())
    return true;

  for(const QString &branchId : branchPath)
  {
    TreeItem *branchItem=treeModel->getItemById(branchId);
    if(branchItem!=nullptr &&
       branchItem->getField(QStringLiteral("crypt"))==QStringLiteral("1") &&
       globalParameters.getCryptKey().length()==0)
      return true;
  }

  return false;
}


QList<NotePickerDialog::Hit> NotePickerDialog::searchHits(const QString &query) const
{
  QList<Hit> hits;

  if(query.isEmpty())
    return hits;

  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");
  if(treeView==nullptr)
    return hits;

  KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

  QSharedPointer< QSet<QString> > allIds=treeModel->getAllRecordsIdList();
  if(allIds.isNull())
    return hits;

  for(const QString &id : *allIds.data())
  {
    if(isRecordSkippable(id))
      continue;

    Record *record=treeModel->getRecord(id);
    if(record==nullptr)
      continue;

    const QString name=record->getField(QStringLiteral("name"));
    const QString tags=record->getField(QStringLiteral("tags"));

    QStringList branchNames;
    for(const QString &branchId : treeModel->getRecordPath(id))
    {
      TreeItem *branchItem=treeModel->getItemById(branchId);
      if(branchItem!=nullptr)
        branchNames << branchItem->getField(QStringLiteral("name"));
    }

    // Поиск сразу по трём столбцам: имя, ветка, метки
    const QString branch=branchNames.isEmpty() ? QString() : branchNames.last();

    if(!name.contains(query, Qt::CaseInsensitive) &&
       !branch.contains(query, Qt::CaseInsensitive) &&
       !tags.contains(query, Qt::CaseInsensitive))
      continue;

    Hit hit;
    hit.id=id;
    hit.name=name;
    hit.branch=branch;
    hit.branchPath=branchNames.join(QStringLiteral(" / "));
    hit.tags=tags;
    hits << hit;
  }

  std::sort(hits.begin(), hits.end(),
            [](const Hit &a, const Hit &b) {
    return a.name.toLower()<b.name.toLower();
  });

  return hits;
}


void NotePickerDialog::rebuildResults(void)
{
  resultsTable->setSortingEnabled(false);
  resultsTable->setRowCount(0);

  const QList<Hit> hits=searchHits(filterEdit->text().trimmed());

  if(filterEdit->text().trimmed().isEmpty())
  {
    counterLabel->setText(tr("Type a name, branch or tag to search"));
    return;
  }

  if(hits.isEmpty())
  {
    counterLabel->setText(tr("Nothing found"));
    return;
  }

  const int shown=hits.size()<DISPLAY_LIMIT ? hits.size() : DISPLAY_LIMIT;
  for(int i=0; i<shown; ++i)
  {
    const Hit &hit=hits.at(i);

    const int row=resultsTable->rowCount();
    resultsTable->insertRow(row);

    QTableWidgetItem *nameItem=new QTableWidgetItem(hit.name);
    nameItem->setData(Qt::UserRole, hit.id);
    resultsTable->setItem(row, ColumnName, nameItem);

    QTableWidgetItem *branchItem=new QTableWidgetItem(hit.branch);

    if(!hit.branchPath.isEmpty())
      branchItem->setToolTip(hit.branchPath);

    resultsTable->setItem(row, ColumnBranch, branchItem);

    resultsTable->setItem(row, ColumnTags, new QTableWidgetItem(hit.tags));
  }

  resultsTable->setSortingEnabled(true);

  counterLabel->setText(tr("Shown %1 of %2").arg(shown).arg(hits.size()));
}

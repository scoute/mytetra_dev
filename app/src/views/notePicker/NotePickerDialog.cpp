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

  buildIndex();
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


void NotePickerDialog::buildIndex(void)
{
  noteIndex.clear();

  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

  if(treeView==nullptr)
    return;

  KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

  if(treeModel==nullptr)
    return;

  noteIndex=treeModel->getNoteIndex();
}


QList<NotePickerDialog::Hit> NotePickerDialog::searchHits(const QString &query) const
{
  QList<Hit> hits;

  // Запрос lowerится один раз: на тысячах записей toLower
  // на каждое сравнение заметно стоит
  const QString q=query.trimmed().toLower();

  if(q.isEmpty())
    return hits;

  for(const NoteIndexEntry &entry : noteIndex)
  {
    // Своя запись и закрытый шифр мимо выдачи
    if(entry.id==excludeRecordId || entry.locked)
      continue;

    if(!entry.nameLower.contains(q) &&
       !entry.branchLower.contains(q) &&
       !entry.tagsLower.contains(q))
      continue;

    Hit hit;
    hit.id=entry.id;
    hit.name=entry.name;
    hit.branch=entry.branch;
    hit.branchPath=entry.branchPath;
    hit.tags=entry.tags;
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

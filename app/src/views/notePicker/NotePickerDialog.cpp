#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
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

  resultsList=new QListWidget(this);

  counterLabel=new QLabel(this);

  QPushButton *selectButton=new QPushButton(tr("Select"), this);
  selectButton->setDefault(true);
  QPushButton *cancelButton=new QPushButton(tr("Cancel"), this);

  QDialogButtonBox *buttons=new QDialogButtonBox(this);
  buttons->addButton(selectButton, QDialogButtonBox::AcceptRole);
  buttons->addButton(cancelButton, QDialogButtonBox::RejectRole);

  connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    if(resultsList->currentRow()>=0)
      accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QVBoxLayout *layout=new QVBoxLayout(this);
  layout->addWidget(filterEdit);
  layout->addWidget(resultsList);
  layout->addWidget(counterLabel);
  layout->addWidget(buttons);
  setLayout(layout);

  resize(420, 380);

  filterTimer=new QTimer(this);
  filterTimer->setSingleShot(true);
  filterTimer->setInterval(150);

  connect(filterEdit, &QLineEdit::textChanged,
          this,        &NotePickerDialog::scheduleFilter);
  connect(filterTimer, &QTimer::timeout,
          this,        &NotePickerDialog::rebuildResults);
  connect(resultsList, &QListWidget::itemActivated,
          this,        &NotePickerDialog::onItemActivated);
  connect(resultsList, &QListWidget::itemDoubleClicked,
          this,        &NotePickerDialog::onItemActivated);

  counterLabel->setText(tr("Type a name or tag to search"));
}


QString NotePickerDialog::selectedRecordId(void) const
{
  QListWidgetItem *item=resultsList->currentItem();
  if(item==nullptr)
    return QString();

  return item->data(Qt::UserRole).toString();
}


QString NotePickerDialog::selectedRecordName(void) const
{
  QListWidgetItem *item=resultsList->currentItem();
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
  resultsList->setCurrentRow(row);
}


int NotePickerDialog::resultCount(void) const
{
  return resultsList->count();
}


QString NotePickerDialog::counterText(void) const
{
  return counterLabel->text();
}


void NotePickerDialog::scheduleFilter(void)
{
  filterTimer->start();
}


void NotePickerDialog::onItemActivated(QListWidgetItem *item)
{
  if(item==nullptr)
    return;

  resultsList->setCurrentItem(item);
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

    if(!name.contains(query, Qt::CaseInsensitive) &&
       !tags.contains(query, Qt::CaseInsensitive))
      continue;

    QStringList branchNames;
    for(const QString &branchId : treeModel->getRecordPath(id))
    {
      TreeItem *branchItem=treeModel->getItemById(branchId);
      if(branchItem!=nullptr)
        branchNames << branchItem->getField(QStringLiteral("name"));
    }

    Hit hit;
    hit.id=id;
    hit.name=name;
    hit.branchPath=branchNames.join(QStringLiteral(" / "));
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
  resultsList->clear();

  const QList<Hit> hits=searchHits(filterEdit->text().trimmed());

  if(filterEdit->text().trimmed().isEmpty())
  {
    counterLabel->setText(tr("Type a name or tag to search"));
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
    QListWidgetItem *item=new QListWidgetItem(hits.at(i).name, resultsList);
    item->setData(Qt::UserRole, hits.at(i).id);
    if(!hits.at(i).branchPath.isEmpty())
      item->setToolTip(hits.at(i).branchPath);
  }

  counterLabel->setText(tr("Shown %1 of %2").arg(shown).arg(hits.size()));
}

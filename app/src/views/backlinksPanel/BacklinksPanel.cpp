#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QTimer>
#include <QBrush>
#include <QDebug>

#include "views/backlinksPanel/BacklinksPanel.h"
#include "libraries/BacklinkIndex.h"
#include "libraries/VisitHistory.h"
#include "libraries/helpers/ObjectHelper.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "views/tree/TreeScreen.h"
#include "views/tree/KnowTreeView.h"
#include "views/record/MetaEditor.h"
#include "views/mainWindow/MainWindow.h"


extern VisitHistory visitHistory;


BacklinksPanel::BacklinksPanel(QWidget *parent) : QWidget(parent)
{
  headerLabel=new QLabel(this);

  linksList=new QListWidget(this);
  linksList->setContextMenuPolicy(Qt::CustomContextMenu);

  QVBoxLayout *layout=new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->addWidget(headerLabel);
  layout->addWidget(linksList);
  setLayout(layout);

  connect(linksList, &QListWidget::itemActivated,
          this,      &BacklinksPanel::onItemActivated);
  connect(linksList, &QListWidget::customContextMenuRequested,
          this,      [this](const QPoint &pos) {
    QMenu menu(this);

    QAction *cleanupAction=menu.addAction(tr("Remove missing sources"));
    connect(cleanupAction, &QAction::triggered,
            this,          &BacklinksPanel::onCleanupStale);

    QAction *rebuildAction=menu.addAction(tr("Recheck all links"));
    connect(rebuildAction, &QAction::triggered,
            this,          &BacklinksPanel::onRebuildAll);

    menu.exec(linksList->mapToGlobal(pos));
  });

  connect(&BacklinkIndex::instance(), &BacklinkIndex::backlinksChanged,
          this,                       &BacklinksPanel::refreshDeferred);
}


BacklinksPanel::~BacklinksPanel(void)
{
}


void BacklinksPanel::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);

  setupLazySignals();
  refreshBacklinks();
}


void BacklinksPanel::setupLazySignals(void)
{
  if(signalsConnected)
    return;

  // В конструкторе treeScreen может еще не существовать
  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
  if(treeScreen==nullptr)
    return;

  signalsConnected=true;

  connect(treeScreen, &TreeScreen::treeMetadataSaved,
          this,        &BacklinksPanel::refreshDeferred);

  // Смена записи: читаем id отложенно, редактор подгружается чуть позже
  connect(&visitHistory, &VisitHistory::visitLogged,
          this,          &BacklinksPanel::refreshDeferred);
}


void BacklinksPanel::refreshDeferred(void)
{
  if(!isVisible())
    return;

  QTimer::singleShot(150, this, &BacklinksPanel::refreshBacklinks);
}


QString BacklinksPanel::currentRecordId(void) const
{
  MetaEditor *metaEditor=find_object<MetaEditor>("editorScreen");
  if(metaEditor==nullptr)
    return QString();

  return metaEditor->getMiscField(QStringLiteral("id"));
}


void BacklinksPanel::refreshBacklinks(void)
{
  setupLazySignals();

  linksList->clear();

  const QString recordId=currentRecordId();
  if(recordId.isEmpty())
  {
    headerLabel->setText(tr("Incoming links"));
    return;
  }

  // Доидексировать новые записи (импорт, синхро): только отсутствующие
  BacklinkIndex::instance().ensureIndexed();

  KnowTreeModel *treeModel=BacklinkIndex::instance().treeModel();
  if(treeModel==nullptr)
  {
    headerLabel->setText(tr("Incoming links"));
    return;
  }

  QSet<QString> sources=BacklinkIndex::instance().backlinksOf(recordId);
  headerLabel->setText(tr("Incoming links (%1)").arg(sources.size()));

  QStringList sorted=sources.toList();
  sorted.sort();

  for(const QString &sourceId : sorted)
  {
    Record *record=treeModel->getRecord(sourceId);

    QString title;
    QString hint;
    bool stale=false;

    if(record==nullptr)
    {
      // Источник из сайдкара не найден в дереве: красным, без прыжка
      title=tr("Missing record %1").arg(sourceId);
      stale=true;
    }
    else
    {
      title=record->getField(QStringLiteral("name"));

      QStringList branchPath=treeModel->getRecordPath(sourceId);
      QStringList branchNames;
      for(const QString &branchId : branchPath)
      {
        TreeItem *branchItem=treeModel->getItemById(branchId);
        if(branchItem!=nullptr)
          branchNames << branchItem->getField(QStringLiteral("name"));
      }
      hint=branchNames.join(QStringLiteral(" / "));
    }

    QListWidgetItem *item=new QListWidgetItem(title, linksList);
    item->setData(Qt::UserRole, sourceId);
    if(!hint.isEmpty())
      item->setToolTip(hint);

    if(stale || BacklinkIndex::instance().isSourceStale(sourceId))
      item->setForeground(QBrush(Qt::red));
  }
}


void BacklinksPanel::onItemActivated(QListWidgetItem *item)
{
  if(item==nullptr)
    return;

  const QString sourceId=item->data(Qt::UserRole).toString();

  KnowTreeModel *treeModel=BacklinkIndex::instance().treeModel();
  if(treeModel==nullptr)
    return;

  // Призрачный источник: прыгать некуда
  const QStringList path=treeModel->getRecordPath(sourceId);
  if(path.isEmpty())
    return;

  find_object<MainWindow>("mainwindow")->setTreeAndRecordtablePositions(path, sourceId);
}


void BacklinksPanel::onCleanupStale(void)
{
  BacklinkIndex::instance().purgeMissing();
  refreshBacklinks();
}


void BacklinksPanel::onRebuildAll(void)
{
  BacklinkIndex::instance().buildFull();
  refreshBacklinks();
}

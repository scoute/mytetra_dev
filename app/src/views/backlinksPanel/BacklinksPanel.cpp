#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QTimer>
#include <QBrush>
#include <QDebug>
#include <QTextDocument>

#include "views/backlinksPanel/BacklinksPanel.h"
#include "libraries/BacklinkIndex.h"
#include "libraries/VisitHistory.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/Editor.h"
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
  incomingHeaderLabel=new QLabel(this);
  incomingList=new QListWidget(this);

  outgoingHeaderLabel=new QLabel(this);
  outgoingList=new QListWidget(this);

  QVBoxLayout *layout=new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->addWidget(incomingHeaderLabel);
  layout->addWidget(incomingList);
  layout->addWidget(outgoingHeaderLabel);
  layout->addWidget(outgoingList);
  setLayout(layout);

  connect(incomingList, &QListWidget::itemActivated,
          this,          &BacklinksPanel::onItemActivated);
  connect(outgoingList, &QListWidget::itemActivated,
          this,          &BacklinksPanel::onItemActivated);

  auto showContextMenu=[this](QListWidget *list, const QPoint &pos) {
    QMenu menu(this);

    QAction *cleanupAction=menu.addAction(tr("Remove missing sources"));
    connect(cleanupAction, &QAction::triggered,
            this,          &BacklinksPanel::onCleanupStale);

    QAction *rebuildAction=menu.addAction(tr("Recheck all links"));
    connect(rebuildAction, &QAction::triggered,
            this,          &BacklinksPanel::onRebuildAll);

    menu.exec(list->mapToGlobal(pos));
  };

  incomingList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(incomingList, &QListWidget::customContextMenuRequested,
          this,          [showContextMenu, this](const QPoint &pos) {
    showContextMenu(incomingList, pos);
  });

  outgoingList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(outgoingList, &QListWidget::customContextMenuRequested,
          this,          [showContextMenu, this](const QPoint &pos) {
    showContextMenu(outgoingList, pos);
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

  setupLiveDocument();
}


void BacklinksPanel::setupLiveDocument(void)
{
  MetaEditor *metaEditor=find_object<MetaEditor>("editorScreen");
  if(metaEditor==nullptr)
    return;

  QTextDocument *doc=metaEditor->getTextareaDocument();
  if(doc==nullptr || doc==liveDoc)
    return;

  if(liveDoc!=nullptr)
    disconnect(liveDoc, nullptr, this, nullptr);

  liveDoc=doc;
  connect(liveDoc, &QTextDocument::contentsChanged,
          this,    &BacklinksPanel::onLiveDocumentChanged);
}


void BacklinksPanel::onLiveDocumentChanged(void)
{
  if(!isVisible())
    return;

  // Дебаунс печати: полный refresh тут не нужен (там ensureIndexed
  // по всей базе на каждое нажатие), достаточно перепарсить исходящие
  if(outgoingPending)
    return;

  outgoingPending=true;
  QTimer::singleShot(400, this, &BacklinksPanel::refreshOutgoingLive);
}


QSet<QString> BacklinksPanel::liveOutgoing(bool *ok) const
{
  if(ok!=nullptr)
    *ok=false;

  MetaEditor *metaEditor=find_object<MetaEditor>("editorScreen");
  if(metaEditor==nullptr)
    return QSet<QString>();

  const QString recordId=currentRecordId();
  if(recordId.isEmpty())
    return QSet<QString>();

  QTextDocument *doc=metaEditor->getTextareaDocument();
  if(doc==nullptr)
    return QSet<QString>();

  if(ok!=nullptr)
    *ok=true;

  return BacklinkIndex::parseOutgoing(recordId, doc->toHtml());
}


void BacklinksPanel::refreshOutgoingLive(void)
{
  outgoingPending=false;

  if(!isVisible())
    return;

  setupLiveDocument();

  const QString recordId=currentRecordId();
  if(recordId.isEmpty())
    return;

  bool ok=false;
  const QSet<QString> targets=liveOutgoing(&ok);
  if(!ok)
    return;

  outgoingHeaderLabel->setText(tr("Outgoing links (%1)").arg(targets.size())+QStringLiteral(" \u21d2"));
  outgoingList->clear();
  fillList(outgoingList, targets);
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
  setupLiveDocument();

  incomingList->clear();
  outgoingList->clear();

  const QString recordId=currentRecordId();
  if(recordId.isEmpty())
  {
    incomingHeaderLabel->setText(tr("Incoming links")+QStringLiteral(" \u21d0"));
    outgoingHeaderLabel->setText(tr("Outgoing links")+QStringLiteral(" \u21d2"));
    return;
  }

  // Доидексировать новые записи (импорт, синхро): только отсутствующие
  BacklinkIndex::instance().ensureIndexed();

  KnowTreeModel *treeModel=BacklinkIndex::instance().treeModel();
  if(treeModel==nullptr)
  {
    incomingHeaderLabel->setText(tr("Incoming links")+QStringLiteral(" \u21d0"));
    outgoingHeaderLabel->setText(tr("Outgoing links")+QStringLiteral(" \u21d2"));
    return;
  }

  QSet<QString> sources=BacklinkIndex::instance().backlinksOf(recordId);
  incomingHeaderLabel->setText(tr("Incoming links (%1)").arg(sources.size())+QStringLiteral(" \u21d0"));
  fillList(incomingList, sources);

  // Исходящие живьем из документа: вставка через диалог видна сразу,
  // до сейва. Редактор недоступен - откат на сохраненный индекс
  bool liveOk=false;
  QSet<QString> targets=liveOutgoing(&liveOk);
  if(!liveOk)
    targets=BacklinkIndex::instance().outgoingOf(recordId);
  outgoingHeaderLabel->setText(tr("Outgoing links (%1)").arg(targets.size())+QStringLiteral(" \u21d2"));
  fillList(outgoingList, targets);
}


// Общая отрисовка списка: живые строки с именем и веткой,
// призраки красным без прыжка. Одинаково для входящих и исходящих
void BacklinksPanel::fillList(QListWidget *list,
                              const QSet<QString> &ids)
{
  KnowTreeModel *treeModel=BacklinkIndex::instance().treeModel();
  if(treeModel==nullptr)
    return;

  QStringList sorted=ids.toList();
  sorted.sort();

  for(const QString &linkId : sorted)
  {
    Record *record=treeModel->getRecord(linkId);

    QString title;
    QString hint;
    bool stale=false;

    if(record==nullptr)
    {
      // Запись из сайдкара не найдена в дереве: красным, без прыжка
      title=tr("Missing record %1").arg(linkId);
      stale=true;
    }
    else
    {
      title=record->getField(QStringLiteral("name"));

      QStringList branchPath=treeModel->getRecordPath(linkId);
      QStringList branchNames;
      for(const QString &branchId : branchPath)
      {
        TreeItem *branchItem=treeModel->getItemById(branchId);
        if(branchItem!=nullptr)
          branchNames << branchItem->getField(QStringLiteral("name"));
      }
      hint=branchNames.join(QStringLiteral(" / "));
    }

    QListWidgetItem *item=new QListWidgetItem(title, list);
    item->setData(Qt::UserRole, linkId);
    if(!hint.isEmpty())
      item->setToolTip(hint);

    if(stale || BacklinkIndex::instance().isSourceStale(linkId))
      item->setForeground(QBrush(Qt::red));
  }
}


void BacklinksPanel::onItemActivated(QListWidgetItem *item)
{
  if(item==nullptr)
    return;

  const QString linkId=item->data(Qt::UserRole).toString();

  KnowTreeModel *treeModel=BacklinkIndex::instance().treeModel();
  if(treeModel==nullptr)
    return;

  // Призрачная запись: прыгать некуда
  const QStringList path=treeModel->getRecordPath(linkId);
  if(path.isEmpty())
    return;

  find_object<MainWindow>("mainwindow")->setTreeAndRecordtablePositions(path, linkId);
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

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "libraries/Favorites.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/KnowTreeView.h"
#include "views/tree/TreeScreen.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/GlobalParameters.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


const QString Favorites::StorageSidecar=QStringLiteral("sidecar");
const QString Favorites::StorageXml=QStringLiteral("xml");


Favorites &Favorites::instance(void)
{
  static Favorites inst;

  return inst;
}


Favorites::Favorites(QObject *parent) : QObject(parent)
{

}


QString Favorites::sidecarFileName(void) const
{
  return QDir::cleanPath(mytetraConfig.get_tetradir()+"/favorites.xml");
}


QString Favorites::storage(void) const
{
  if(mytetraConfig.get_favoritesStorage()==StorageXml)
    return StorageXml;

  return StorageSidecar;
}


void Favorites::setStorage(const QString &mode)
{
  const QString target=(mode==StorageXml) ? StorageXml : StorageSidecar;

  if(target==storage())
    return;

  // Объединение обоих бэкендов: что бы ни лежало в заброшенном,
  // звездочки не теряются
  sidecarEnsureLoaded();

  QSet<QString> all=m_sidecarIds;
  all.unite(xmlIdSet());

  if(target==StorageXml)
  {
    xmlWriteAll(all.values());
    sidecarReplace(QSet<QString>());
  }
  else
  {
    sidecarReplace(all);
    xmlWriteAll(QStringList());
  }

  mytetraConfig.set_favoritesStorage(target);

  emit favoritesChanged();
}


QStringList Favorites::ids(void) const
{
  if(storage()==StorageXml)
    return xmlIdSet().values();

  sidecarEnsureLoaded();

  QStringList out=m_sidecarIds.values();
  out.sort(Qt::CaseInsensitive);

  return out;
}


bool Favorites::isFavorite(const QString &id) const
{
  if(id.isEmpty())
    return false;

  if(storage()==StorageXml)
  {
    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView==nullptr)
      return false;

    KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

    Record *record=treeModel->getRecord(id);

    return record!=nullptr &&
           record->getField(QStringLiteral("favorite"))==QStringLiteral("1");
  }

  sidecarEnsureLoaded();

  return m_sidecarIds.contains(id);
}


void Favorites::setFavorite(const QString &id, bool on)
{
  if(id.isEmpty())
    return;

  if(storage()==StorageXml)
  {
    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

    if(treeView==nullptr)
      return;

    KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

    const QStringList path=treeModel->getRecordPath(id);

    if(path.isEmpty())
      return;

    TreeItem *branchItem=treeModel->getItemById(path.last());

    if(branchItem==nullptr)
      return;

    RecordTableData *table=branchItem->recordtableGetTableData();

    if(table==nullptr)
      return;

    const int pos=table->getPosById(id);

    if(pos<0)
      return;

    const bool starred=(table->getField(QStringLiteral("favorite"), pos)==QStringLiteral("1"));

    if(starred==on)
      return;

    table->setField(QStringLiteral("favorite"), on ? QStringLiteral("1") : QString(), pos);

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

    if(treeScreen!=nullptr)
      treeScreen->saveKnowTree();
  }
  else
  {
    sidecarEnsureLoaded();

    const bool starred=m_sidecarIds.contains(id);

    if(starred==on)
      return;

    if(on)
      m_sidecarIds.insert(id);
    else
      m_sidecarIds.remove(id);

    sidecarSave();
  }

  emit favoritesChanged();
}


int Favorites::purgeDead(void)
{
  sidecarEnsureLoaded();

  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

  if(treeView==nullptr)
    return 0;

  KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

  QSharedPointer< QSet<QString> > alive=treeModel->getAllRecordsIdList();

  if(alive.isNull())
    return 0;

  const int before=m_sidecarIds.size();
  m_sidecarIds.intersect(*alive.data());

  if(m_sidecarIds.size()==before)
    return 0;

  sidecarSave();

  return before-m_sidecarIds.size();
}


void Favorites::removeIdsSilent(const QStringList &dead)
{
  if(dead.isEmpty())
    return;

  sidecarEnsureLoaded();

  bool changed=false;

  for(const QString &id : dead)
    changed=m_sidecarIds.remove(id) || changed;

  if(changed)
    sidecarSave();
}


void Favorites::sidecarEnsureLoaded(void) const
{
  if(m_sidecarLoaded)
    return;

  // Отсутствующий файл не ошибка: избранного просто еще нет
  const_cast<Favorites *>(this)->sidecarLoad();
  m_sidecarLoaded=true;
}


bool Favorites::sidecarLoad(void)
{
  QFile file(sidecarFileName());

  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  QSet<QString> fresh;

  QXmlStreamReader xml(&file);

  if(!xml.readNextStartElement() || xml.name()!=QStringLiteral("favorites"))
    return false;

  while(xml.readNextStartElement())
  {
    if(xml.name()==QStringLiteral("note"))
    {
      const QString id=xml.attributes().value(QStringLiteral("id")).toString();

      if(!id.isEmpty())
        fresh.insert(id);

      xml.skipCurrentElement();
    }
    else
      xml.skipCurrentElement();
  }

  if(xml.hasError())
    return false;

  m_sidecarIds=fresh;

  return true;
}


bool Favorites::sidecarSave(void) const
{
  QSaveFile file(sidecarFileName());

  if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    qDebug() << "Favorites::sidecarSave: can't open" << sidecarFileName();

    return false;
  }

  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement(QStringLiteral("favorites"));
  xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));

  QStringList ordered=m_sidecarIds.values();
  ordered.sort(Qt::CaseInsensitive);

  for(const QString &id : ordered)
  {
    xml.writeStartElement(QStringLiteral("note"));
    xml.writeAttribute(QStringLiteral("id"), id);
    xml.writeEndElement();
  }

  xml.writeEndElement();
  xml.writeEndDocument();

  if(!file.commit())
  {
    qDebug() << "Favorites::sidecarSave: commit failed";

    return false;
  }

  return true;
}


void Favorites::sidecarReplace(const QSet<QString> &fresh)
{
  sidecarEnsureLoaded();

  m_sidecarIds=fresh;
  sidecarSave();
}


// Все id с флагом favorite по дереву. Зашифрованные ветки без ключа
// пропускаются: читать их записи нельзя
QSet<QString> Favorites::xmlIdSet(void) const
{
  QSet<QString> out;

  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

  if(treeView==nullptr)
    return out;

  KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

  QSharedPointer< QSet<QString> > allIds=treeModel->getAllRecordsIdList();

  if(allIds.isNull())
    return out;

  for(const QString &id : *allIds.data())
  {
    const QStringList path=treeModel->getRecordPath(id);

    if(path.isEmpty())
      continue;

    bool locked=false;

    for(const QString &branchId : path)
    {
      TreeItem *branchItem=treeModel->getItemById(branchId);

      if(branchItem!=nullptr &&
         branchItem->getField(QStringLiteral("crypt"))==QStringLiteral("1") &&
         globalParameters.getCryptKey().length()==0)
      {
        locked=true;
        break;
      }
    }

    if(locked)
      continue;

    Record *record=treeModel->getRecord(id);

    if(record!=nullptr &&
       record->getField(QStringLiteral("favorite"))==QStringLiteral("1"))
      out.insert(id);
  }

  return out;
}


// Полная перезапись флагов: выставляются только переданные id,
// остальные чистятся. Одно сохранение дерева в конце
void Favorites::xmlWriteAll(const QStringList &fresh)
{
  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

  if(treeView==nullptr)
    return;

  KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

  QSharedPointer< QSet<QString> > allIds=treeModel->getAllRecordsIdList();

  if(allIds.isNull())
    return;

  const QSet<QString> wanted=QSet<QString>::fromList(fresh);

  for(const QString &id : *allIds.data())
  {
    const QStringList path=treeModel->getRecordPath(id);

    if(path.isEmpty())
      continue;

    TreeItem *branchItem=treeModel->getItemById(path.last());

    if(branchItem==nullptr)
      continue;

    RecordTableData *table=branchItem->recordtableGetTableData();

    if(table==nullptr)
      continue;

    const int pos=table->getPosById(id);

    if(pos<0)
      continue;

    const bool starred=(table->getField(QStringLiteral("favorite"), pos)==QStringLiteral("1"));
    const bool want=wanted.contains(id);

    if(starred!=want)
      table->setField(QStringLiteral("favorite"), want ? QStringLiteral("1") : QString(), pos);
  }

  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

  if(treeScreen!=nullptr)
    treeScreen->saveKnowTree();
}

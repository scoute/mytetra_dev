#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>
#include <algorithm>

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
  // звездочки не теряются. Порядок: сначала текущий сайдкар как есть,
  // затем недостающее из XML (номерные по favor, затем по имени)
  sidecarEnsureLoaded();

  QStringList all=m_sidecarOrder;

  for(const QString &id : xmlIdListOrdered())
    if(!all.contains(id))
      all << id;

  if(target==StorageXml)
  {
    xmlWriteAll(all);
    sidecarReplace(QStringList());
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
    return xmlIdListOrdered();

  sidecarEnsureLoaded();

  return m_sidecarOrder;
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

    if(record==nullptr)
      return false;

    // gee12-метка: числовой favor>0 - тоже избранное
    return record->getField(QStringLiteral("favorite"))==QStringLiteral("1") ||
           record->getField(QStringLiteral("favor")).toInt()>0;
  }

  sidecarEnsureLoaded();

  return m_sidecarOrder.contains(id);
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

    const bool starred=(table->getField(QStringLiteral("favorite"), pos)==QStringLiteral("1") ||
                        table->getField(QStringLiteral("favor"), pos).toInt()>0);

    if(starred==on)
      return;

    table->setField(QStringLiteral("favorite"), on ? QStringLiteral("1") : QString(), pos);

    // Выключение чистит и чужой favor: иначе запись с favor>0
    // осталась бы избранной вопреки действию пользователя
    if(!on)
      table->setField(QStringLiteral("favor"), QString(), pos);

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

    if(treeScreen!=nullptr)
      treeScreen->saveKnowTree();
  }
  else
  {
    sidecarEnsureLoaded();

    const bool starred=m_sidecarOrder.contains(id);

    if(starred==on)
      return;

    if(on)
      m_sidecarOrder.append(id);
    else
      m_sidecarOrder.removeOne(id);

    sidecarSave();
  }

  emit favoritesChanged();
}


void Favorites::setOrder(const QStringList &orderedIds)
{
  // В xml-режиме порядок - номера favor в mytetra.xml: сайдкарный
  // список там не канон, молча не правим чужую нумерацию
  if(storage()!=StorageSidecar)
    return;

  sidecarEnsureLoaded();

  QStringList fresh;

  for(const QString &id : orderedIds)
    if(!id.isEmpty() && !fresh.contains(id))
      fresh << id;

  // Потерять звездочку молча нельзя: неупомянутые дописываются в хвост
  for(const QString &id : m_sidecarOrder)
    if(!fresh.contains(id))
      fresh << id;

  if(fresh==m_sidecarOrder)
    return;

  m_sidecarOrder=fresh;
  sidecarSave();

  emit favoritesChanged();
}


void Favorites::seedFromFavorOrder(KnowTreeModel *model)
{
  if(model==nullptr || storage()!=StorageSidecar)
    return;

  // Явное состояние пользователя не трогаем: посев только когда
  // сайдкара еще нет (первое открытие чужой/gee12-базы).
  // Пустой существующий файл - осознанное "нет избранного",
  // его не засеваем
  if(QFile::exists(sidecarFileName()))
    return;

  // Номерные по возрастанию, затем наши favorite="1" по имени.
  // Зашифрованные без ключа тоже сеются: favor не шифруется и читается,
  // панель покажет их когда введут пароль
  QMap<int, QStringList> numbered;
  QMap<QString, QString> flaggedNames; // id -> lower(name)

  QSharedPointer< QSet<QString> > allIds=model->getAllRecordsIdList();

  if(allIds.isNull())
    return;

  for(const QString &id : *allIds.data())
  {
    Record *record=model->getRecord(id);

    if(record==nullptr)
      continue;

    const int favorNum=record->getField(QStringLiteral("favor")).toInt();

    if(favorNum>0)
      numbered[favorNum] << id;
    else if(record->getField(QStringLiteral("favorite"))==QStringLiteral("1"))
      flaggedNames[id]=record->getField(QStringLiteral("name")).toLower();
  }

  if(numbered.isEmpty() && flaggedNames.isEmpty())
    return;

  QStringList fresh;

  for(auto it=numbered.constBegin(); it!=numbered.constEnd(); ++it)
  {
    QStringList group=it.value();
    group.sort();
    fresh << group;
  }

  QStringList flagged=flaggedNames.keys();
  std::sort(flagged.begin(), flagged.end(),
            [&](const QString &a, const QString &b) {
              const int cmp=flaggedNames[a].compare(flaggedNames[b]);
              return (cmp!=0) ? (cmp<0) : (a<b);
            });
  fresh << flagged;

  sidecarReplace(fresh);
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

  QStringList fresh;

  for(const QString &id : m_sidecarOrder)
    if(alive->contains(id))
      fresh << id;

  const int removed=m_sidecarOrder.size()-fresh.size();

  if(removed==0)
    return 0;

  m_sidecarOrder=fresh;
  sidecarSave();

  return removed;
}


void Favorites::removeIdsSilent(const QStringList &dead)
{
  if(dead.isEmpty())
    return;

  sidecarEnsureLoaded();

  bool changed=false;

  for(const QString &id : dead)
    changed=m_sidecarOrder.removeOne(id) || changed;

  if(changed)
    sidecarSave();
}


void Favorites::sidecarEnsureLoaded(void) const
{
  const QString cur=mytetraConfig.get_tetradir();

  // Смена базы или внешний синк: кеш протух
  if(m_sidecarLoaded && m_sidecarTetradir!=cur)
    m_sidecarLoaded=false;

  if(m_sidecarLoaded)
    return;

  // Отсутствующий файл не ошибка: избранного просто еще нет
  const_cast<Favorites *>(this)->sidecarLoad();
  m_sidecarLoaded=true;
  m_sidecarTetradir=cur;
}


bool Favorites::sidecarLoad(void)
{
  QFile file(sidecarFileName());

  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  struct SidecarEntry
  {
    int seq=0;
    int order=0;
    QString id;
  };

  QList<SidecarEntry> entries;
  int seq=0;

  QXmlStreamReader xml(&file);

  if(!xml.readNextStartElement() || xml.name()!=QStringLiteral("favorites"))
    return false;

  while(xml.readNextStartElement())
  {
    if(xml.name()==QStringLiteral("note"))
    {
      const QString id=xml.attributes().value(QStringLiteral("id")).toString();
      bool ok=false;
      const int order=xml.attributes().value(QStringLiteral("n")).toInt(&ok);

      if(!id.isEmpty())
      {
        SidecarEntry entry;
        entry.seq=seq++;
        entry.order=(ok && order>0) ? order : 0;
        entry.id=id;
        entries << entry;
      }

      xml.skipCurrentElement();
    }
    else
      xml.skipCurrentElement();
  }

  if(xml.hasError())
    return false;

  // Порядок - атрибут n, у кого нет - порядок документа в хвост.
  // Дубли режутся, первое вхождение побеждает
  std::sort(entries.begin(), entries.end(),
            [](const SidecarEntry &a, const SidecarEntry &b) {
              const bool ao=(a.order>0);
              const bool bo=(b.order>0);

              if(ao && bo)
                return (a.order!=b.order) ? (a.order<b.order) : (a.seq<b.seq);

              if(ao!=bo)
                return ao;

              return a.seq<b.seq;
            });

  QStringList fresh;

  for(const SidecarEntry &entry : entries)
    if(!fresh.contains(entry.id))
      fresh << entry.id;

  m_sidecarOrder=fresh;

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

  // Порядок пишется как есть плюс номер для читаемости и
  // устойчивости к ручным правкам (загрузка понимает и без n)
  for(int i=0; i<m_sidecarOrder.size(); ++i)
  {
    xml.writeStartElement(QStringLiteral("note"));
    xml.writeAttribute(QStringLiteral("id"), m_sidecarOrder.at(i));
    xml.writeAttribute(QStringLiteral("n"), QString::number(i+1));
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


void Favorites::sidecarReplace(const QStringList &fresh)
{
  sidecarEnsureLoaded();

  QStringList deduped;

  for(const QString &id : fresh)
    if(!id.isEmpty() && !deduped.contains(id))
      deduped << id;

  m_sidecarOrder=deduped;
  sidecarSave();
}


// Все id с флагом favorite/favor по дереву, упорядоченные: сначала
// числовой favor gee12 по возрастанию, затем favorite="1" по имени.
// Зашифрованные ветки без ключа пропускаются: читать их записи нельзя
QStringList Favorites::xmlIdListOrdered(void) const
{
  QMap<int, QStringList> numbered; // n -> ids (дубль номера - по id)
  QMap<QString, QString> flaggedNames; // id -> lower(name)

  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

  if(treeView==nullptr)
    return QStringList();

  KnowTreeModel *treeModel=static_cast<KnowTreeModel *>(treeView->model());

  QSharedPointer< QSet<QString> > allIds=treeModel->getAllRecordsIdList();

  if(allIds.isNull())
    return QStringList();

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

    if(record==nullptr)
      continue;

    const int favorNum=record->getField(QStringLiteral("favor")).toInt();

    if(favorNum>0)
      numbered[favorNum] << id;
    else if(record->getField(QStringLiteral("favorite"))==QStringLiteral("1"))
      flaggedNames[id]=record->getField(QStringLiteral("name")).toLower();
  }

  QStringList out;

  for(auto it=numbered.constBegin(); it!=numbered.constEnd(); ++it)
  {
    QStringList group=it.value();
    group.sort();
    out << group;
  }

  QStringList flagged=flaggedNames.keys();
  std::sort(flagged.begin(), flagged.end(),
            [&](const QString &a, const QString &b) {
              const int cmp=flaggedNames[a].compare(flaggedNames[b]);
              return (cmp!=0) ? (cmp<0) : (a<b);
            });
  out << flagged;

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

    const bool starred=(table->getField(QStringLiteral("favorite"), pos)==QStringLiteral("1") ||
                        table->getField(QStringLiteral("favor"), pos).toInt()>0);
    const bool want=wanted.contains(id);

    if(starred==want)
      continue;

    table->setField(QStringLiteral("favorite"), want ? QStringLiteral("1") : QString(), pos);

    // Чистка чужого favor при снятии: иначе favor>0 оставит запись
    // избранной вопреки перезаписи
    if(!want)
      table->setField(QStringLiteral("favor"), QString(), pos);
  }

  TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");

  if(treeScreen!=nullptr)
    treeScreen->saveKnowTree();
}

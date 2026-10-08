#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QRegExp>
#include <QDebug>

#include "libraries/BacklinkIndex.h"
#include "libraries/FixedParameters.h"
#include "models/tree/KnowTreeModel.h"
#include "models/recordTable/Record.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/helpers/ObjectHelper.h"
#include "views/tree/KnowTreeView.h"


extern AppConfig mytetraConfig;


BacklinkIndex &BacklinkIndex::instance(void)
{
  static BacklinkIndex inst;
  return inst;
}


BacklinkIndex::BacklinkIndex(QObject *parent) : QObject(parent)
{
}


void BacklinkIndex::setTreeModel(KnowTreeModel *model)
{
  m_treeModel=model;
}


KnowTreeModel *BacklinkIndex::treeModel(void)
{
  if(m_treeModel==nullptr)
  {
    // Ленивый подхват: панель и хуки могут сработать раньше явной инициализации
    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");
    if(treeView!=nullptr)
      m_treeModel=static_cast<KnowTreeModel *>(treeView->model());
  }

  return m_treeModel;
}


QString BacklinkIndex::sidecarFileName(void)
{
  return QDir::cleanPath(mytetraConfig.get_tetradir()+"/backlinks.xml");
}


bool BacklinkIndex::isLoaded(void) const
{
  return m_loaded;
}


void BacklinkIndex::ensureLoaded(void)
{
  if(m_loaded)
    return;

  if(!load())
  {
    // Сайдкара нет: первый запуск, строим полным сканом
    m_loaded=true;
    buildFull();
    return;
  }

  m_loaded=true;
}


bool BacklinkIndex::load(void)
{
  QFile file(sidecarFileName());
  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  QMap<QString, QSet<QString>> forward;

  QXmlStreamReader xml(&file);
  if(!xml.readNextStartElement() || xml.name()!=QStringLiteral("backlinks"))
    return false;

  while(xml.readNextStartElement())
  {
    if(xml.name()==QStringLiteral("pair"))
    {
      const QString source=xml.attributes().value(QStringLiteral("source")).toString();
      const QString target=xml.attributes().value(QStringLiteral("target")).toString();

      if(!source.isEmpty() && !target.isEmpty() && source!=target)
        forward[source].insert(target);

      xml.skipCurrentElement();
    }
    else
      xml.skipCurrentElement();
  }

  if(xml.hasError())
    return false;

  m_forward=forward;
  rebuildReverse();

  return true;
}


bool BacklinkIndex::save(void) const
{
  QSaveFile file(sidecarFileName());
  if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    qDebug() << "BacklinkIndex::save: can't open" << sidecarFileName();
    return false;
  }

  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement(QStringLiteral("backlinks"));
  xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));

  for(auto it=m_forward.constBegin(); it!=m_forward.constEnd(); ++it)
    for(const QString &target : it.value())
    {
      xml.writeStartElement(QStringLiteral("pair"));
      xml.writeAttribute(QStringLiteral("source"), it.key());
      xml.writeAttribute(QStringLiteral("target"), target);
      xml.writeEndElement();
    }

  xml.writeEndElement();
  xml.writeEndDocument();

  if(!file.commit())
  {
    qDebug() << "BacklinkIndex::save: commit failed";
    return false;
  }

  return true;
}


void BacklinkIndex::loadOrBuild(void)
{
  ensureLoaded();
}


void BacklinkIndex::rebuildReverse(void)
{
  m_reverse.clear();

  for(auto it=m_forward.constBegin(); it!=m_forward.constEnd(); ++it)
    for(const QString &target : it.value())
      m_reverse[target].insert(it.key());
}


QSet<QString> BacklinkIndex::parseOutgoing(const QString &selfId,
                                          const QString &text)
{
  QSet<QString> result;

  // Тот же формат что в LinkHelper::isHrefInternal,
  // но поиском по всему тексту: mytetra://note/<id>
  QRegExp rx(FixedParameters::appTextId+"://note/(\\w+)");

  int pos=0;
  while((pos=rx.indexIn(text, pos))!=-1)
  {
    const QString id=rx.cap(1);
    pos+=rx.matchedLength();

    // Самоссылки бессмысленны: ссылка умеет только на страницу целиком
    if(id!=selfId)
      result.insert(id);
  }

  return result;
}


bool BacklinkIndex::recordExists(const QString &recordId) const
{
  KnowTreeModel *model=const_cast<BacklinkIndex *>(this)->treeModel();
  if(model==nullptr)
    return true;

  return model->isRecordIdExists(recordId);
}


QString BacklinkIndex::recordTextFileName(const QString &recordId) const
{
  KnowTreeModel *model=const_cast<BacklinkIndex *>(this)->treeModel();
  if(model==nullptr)
    return QString();

  Record *record=model->getRecord(recordId);
  if(record==nullptr)
    return QString();

  // getFullTextFileName() у Record защищен, собирается из публичных полей
  const QString dir=record->getField(QStringLiteral("dir"));
  const QString file=record->getField(QStringLiteral("file"));
  if(dir.isEmpty() || file.isEmpty())
    return QString();

  return QDir::cleanPath(mytetraConfig.get_tetradir()+
                         QStringLiteral("/base/")+dir+
                         QStringLiteral("/")+file);
}


void BacklinkIndex::updateSource(const QString &recordId)
{
  if(recordId.isEmpty())
    return;

  ensureLoaded();

  // Записи нет в дереве: нечего парсить
  if(!recordExists(recordId))
    return;

  const QString fileName=recordTextFileName(recordId);

  QFile file(fileName);
  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    // Файл недоступен (шифр, права): старые данные не трогаем
    return;
  }

  const QSet<QString> fresh=parseOutgoing(recordId,
                                          QString::fromUtf8(file.readAll()));

  if(m_forward.value(recordId)==fresh)
    return;

  if(fresh.isEmpty())
    m_forward.remove(recordId);
  else
    m_forward[recordId]=fresh;

  rebuildReverse();
  save();

  emit backlinksChanged();
}


void BacklinkIndex::removeSources(const QStringList &ids)
{
  if(ids.isEmpty())
    return;

  ensureLoaded();

  bool changed=false;

  for(const QString &id : ids)
  {
    // Исходящие удаленного источника
    if(m_forward.remove(id)>0)
      changed=true;

    // Вхождения id в чужие списки целей
    for(auto it=m_forward.begin(); it!=m_forward.end(); ++it)
      if(it.value().remove(id))
        changed=true;
  }

  if(!changed)
    return;

  rebuildReverse();
  save();

  emit backlinksChanged();
}


int BacklinkIndex::purgeMissing(void)
{
  ensureLoaded();

  int removed=0;

  // Пары с несуществующим источником уходят целиком
  for(auto it=m_forward.begin(); it!=m_forward.end();)
  {
    if(!recordExists(it.key()))
    {
      removed+=it.value().size();
      it=m_forward.erase(it);
      continue;
    }

    // Цели-призраки вычищаются из списков
    for(auto jt=it.value().begin(); jt!=it.value().end();)
    {
      if(!recordExists(*jt))
      {
        jt=it.value().erase(jt);
        removed++;
      }
      else
        ++jt;
    }

    if(it.value().isEmpty())
      it=m_forward.erase(it);
    else
      ++it;
  }

  if(removed>0)
  {
    rebuildReverse();
    save();

    emit backlinksChanged();
  }

  return removed;
}


void BacklinkIndex::collectAllRecordIds(QSet<QString> &ids) const
{
  KnowTreeModel *model=const_cast<BacklinkIndex *>(this)->treeModel();
  if(model==nullptr)
    return;

  QSharedPointer< QSet<QString> > all=model->getAllRecordsIdList();
  if(!all.isNull())
    ids.unite(*all.data());
}


void BacklinkIndex::buildFull(void)
{
  m_forward.clear();

  QSet<QString> ids;
  collectAllRecordIds(ids);

  for(const QString &id : ids)
  {
    const QString fileName=recordTextFileName(id);
    if(fileName.isEmpty())
      continue;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    const QSet<QString> outgoing=parseOutgoing(id, QString::fromUtf8(file.readAll()));
    if(!outgoing.isEmpty())
      m_forward[id]=outgoing;
  }

  rebuildReverse();
  save();

  emit backlinksChanged();
}


int BacklinkIndex::ensureIndexed(void)
{
  ensureLoaded();

  QSet<QString> ids;
  collectAllRecordIds(ids);

  int added=0;

  for(const QString &id : ids)
  {
    if(m_forward.contains(id))
      continue;

    const QString fileName=recordTextFileName(id);
    if(fileName.isEmpty())
      continue;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    const QSet<QString> outgoing=parseOutgoing(id, QString::fromUtf8(file.readAll()));
    if(!outgoing.isEmpty())
    {
      m_forward[id]=outgoing;
      added++;
    }
  }

  if(added>0)
  {
    rebuildReverse();
    save();

    emit backlinksChanged();
  }

  return added;
}


QSet<QString> BacklinkIndex::backlinksOf(const QString &recordId) const
{
  return m_reverse.value(recordId);
}


bool BacklinkIndex::isSourceStale(const QString &sourceId) const
{
  return !recordExists(sourceId);
}


QStringList BacklinkIndex::staleSources(void) const
{
  QStringList result;

  for(auto it=m_forward.constBegin(); it!=m_forward.constEnd(); ++it)
    if(!recordExists(it.key()))
      result << it.key();

  return result;
}

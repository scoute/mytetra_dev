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
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigStorage.h"

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
  return QDir::cleanPath(AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/backlinks.xml");
}

bool BacklinkIndex::isLoaded(void) const
{
  return m_loaded;
}

void BacklinkIndex::ensureLoaded(void)
{
  // Загруженный, но от другой базы (подмена снаружи при живом
  // сайдкаре): грузить врунью нельзя, только перестроение. Проверка
  // дешевая - обход дерева без чтения текстов
  if(m_loaded && isFresh())
    return;

  if(!load() || !isFresh())
  {
    // Сайдкара нет, он бит или от другой базы: полный скан
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

  // Число записей на момент записи сайдкара. Нет атрибута -
  // сайдкар от старой версии, свежесть не проверить
  if(xml.attributes().hasAttribute(QStringLiteral("records")))
    m_sidecarRecords=xml.attributes().value(QStringLiteral("records")).toInt();
  else
    m_sidecarRecords=-1;

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
  m_sidecarTetradir=AppContext::instance().appConfig()->storageSection()->get_tetradir();

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

  // Число записей дерева на момент записи: проверка свежести при
  // загрузке ловит базу, подмененную снаружи при живом сайдкаре.
  // Все мутации идут через save(), счетчик всегда актуален.
  // Член тоже обновляется: свежесть проверяется и без перезагрузки
  const int recordCount=currentRecordCount();
  m_sidecarRecords=recordCount;
  m_sidecarTetradir=AppContext::instance().appConfig()->storageSection()->get_tetradir();
  xml.writeAttribute(QStringLiteral("records"), QString::number(recordCount));

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


void BacklinkIndex::loadOnly(void)
{
  m_forward.clear();
  m_reverse.clear();
  m_sidecarRecords=-1;

  m_loaded=load();
}


bool BacklinkIndex::isFresh(void) const
{
  if(!m_loaded)
    return false;

  // Сайдкар от старой версии без счетчика: свежесть не проверить,
  // честно считается несвежим и перестроится один раз
  if(m_sidecarRecords<0)
    return false;

  // Чужая база с тем же числом записей (fix/544)
  if(!m_sidecarTetradir.isEmpty() &&
     m_sidecarTetradir!=AppContext::instance().appConfig()->storageSection()->get_tetradir())
    return false;

  KnowTreeModel *model=const_cast<BacklinkIndex *>(this)->treeModel();

  if(model==nullptr)
    return true;

  QSet<QString> ids;
  const_cast<BacklinkIndex *>(this)->collectAllRecordIds(ids);

  return ids.size()==m_sidecarRecords;
}


int BacklinkIndex::currentRecordCount(void) const
{
  KnowTreeModel *model=const_cast<BacklinkIndex *>(this)->treeModel();

  if(model==nullptr)
    return -1;

  QSet<QString> ids;
  const_cast<BacklinkIndex *>(this)->collectAllRecordIds(ids);

  return ids.size();
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

  return textFileName(dir, file);
}


// Путь к тексту по паре dir/file без поиска записи в дереве.
// Тяжелым сканам нельзя ходить getRecord(id): рекурсия по дереву
// на каждую из 100k+ записей дает O(n^2) (см. 494)
QString BacklinkIndex::textFileName(const QString &dir, const QString &file)
{
  if(dir.isEmpty() || file.isEmpty())
    return QString();

  return QDir::cleanPath(AppContext::instance().appConfig()->storageSection()->get_tetradir()+
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

  // Без модели как раньше ничего не вычищается (recordExists
  // возвращал true), иначе пустой набор id снес бы весь индекс
  if(treeModel()==nullptr)
    return 0;

  // Все id дерева одним проходом: проверка каждой пары поиском
  // по дереву давала O(n^2) (см. 494)
  QSet<QString> ids;
  collectAllRecordIds(ids);

  int removed=0;

  // Пары с несуществующим источником уходят целиком
  for(auto it=m_forward.begin(); it!=m_forward.end();)
  {
    if(!ids.contains(it.key()))
    {
      removed+=it.value().size();
      it=m_forward.erase(it);
      continue;
    }

    // Цели-призраки вычищаются из списков
    for(auto jt=it.value().begin(); jt!=it.value().end();)
    {
      if(!ids.contains(*jt))
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

void BacklinkIndex::buildFull(QAtomicInt *cancel)
{
  m_forward.clear();

  // Один обход O(n): поиск каждой записи по id рекурсией
  // по дереву давал O(n^2), на 100k+ минуты вместо секунд (см. 494)
  KnowTreeModel *model=treeModel();

  QList<KnowTreeModel::RecordFileRef> refs;

  if(model!=nullptr)
    refs=model->getAllRecordFileRefs();

  const int total=refs.size();
  int done=0;

  for(const auto &ref : refs)
  {
    // Отмена из диалога: прерывание честное, частичный индекс
    // сохраняется ниже, дырки доберет ensureIndexed()
    if(cancel!=nullptr && cancel->loadAcquire()!=0)
      break;

    const QString fileName=textFileName(ref.dir, ref.file);

    if(fileName.isEmpty())
      continue;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    const QSet<QString> outgoing=parseOutgoing(ref.id, QString::fromUtf8(file.readAll()));
    if(!outgoing.isEmpty())
      m_forward[ref.id]=outgoing;

    // Прогресс пачками: сигнал без подписчиков почти бесплатен
    if((++done % 100)==0)
      emit buildProgress(done, total);
  }

  emit buildProgress(done, total);

  rebuildReverse();
  save();

  m_loaded=true;

  emit backlinksChanged();
}

int BacklinkIndex::ensureIndexed(void)
{
  ensureLoaded();

  // Тот же линейный обход что в buildFull: поиск по id рекурсией
  // давал O(n^2) на недостающих записях (см. 494)
  KnowTreeModel *model=treeModel();

  QList<KnowTreeModel::RecordFileRef> refs;

  if(model!=nullptr)
    refs=model->getAllRecordFileRefs();

  int added=0;

  for(const auto &ref : refs)
  {
    if(m_forward.contains(ref.id))
      continue;

    const QString fileName=textFileName(ref.dir, ref.file);
    if(fileName.isEmpty())
      continue;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    const QSet<QString> outgoing=parseOutgoing(ref.id, QString::fromUtf8(file.readAll()));
    if(!outgoing.isEmpty())
    {
      m_forward[ref.id]=outgoing;
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

QSet<QString> BacklinkIndex::outgoingOf(const QString &recordId) const
{
  return m_forward.value(recordId);
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

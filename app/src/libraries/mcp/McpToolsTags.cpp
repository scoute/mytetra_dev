#include <QModelIndex>
#include <QJsonArray>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QThread>
#include <QCryptographicHash>
#include <QSet>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDomDocument>

#include <functional>
#include <algorithm>

#include "McpTools.h"
#include "main.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/recordTable/Record.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/BacklinkIndex.h"
#include "libraries/helpers/UniqueIdHelper.h"

#include <QFile>

// Контекст сниппета поиска с каждой стороны от совпадения
static const int MCP_SNIPPET_CONTEXT=100;

#include "McpToolsPrivate.h"
#include "libraries/AppContext.h"

static QJsonObject duplicateNoteObject(Record *record,
                                       const QString &branchId,
                                       const QString &branchPath)
{
  QJsonObject note;
  note["id"]=record->getField("id");
  note["name"]=record->getField("name");
  note["branch_id"]=branchId;
  note["branch_path"]=branchPath;

  return note;
}

static void collectDuplicates(TreeItem *item,
                              QMap<QString, QList<QJsonObject>> &byName,
                              QMap<QString, QList<QJsonObject>> &byText)
{
  QString branchId=item->getField("id");

  QStringList names=item->getPathAsField("name");
  names.removeAll(QString(""));
  QString path=names.join(" / ");

  RecordTableData *table=item->recordtableGetTableData();

  if(table!=nullptr)
    for(unsigned i=0; i<table->size(); ++i)
    {
      Record *record=table->getRecord(i);

      if(record==nullptr)
        continue;

      QString nameKey=record->getField("name").trimmed().toLower();

      if(!nameKey.isEmpty())
        byName[nameKey].append(duplicateNoteObject(record, branchId, path));

      QString html=record->getTextForStats();

      if(!html.isEmpty())
      {
        QString textKey=QString::fromLatin1(
          QCryptographicHash::hash(html.toUtf8(),
                                   QCryptographicHash::Sha1).toHex());

        if(!record->getField("crypt").isEmpty() && record->getField("crypt")!="0")
          textKey=QString(); // Шифр без ключа читается пусто: ложные дубли не нужны
        else
          byText[textKey].append(duplicateNoteObject(record, branchId, path));
      }
    }

  for(int i=0; i<item->childCount(); ++i)
    collectDuplicates(item->child(i), byName, byText);
}

QJsonObject McpTools::findDuplicates(int limit)
{
  if(limit<1)
    limit=1;

  if(limit>200)
    limit=200;

  QMap<QString, QList<QJsonObject>> byName;
  QMap<QString, QList<QJsonObject>> byText;

  // Точка входа - все ветки верхнего уровня через getBaseStatRows? Нет:
  // проще обойти через QModelIndex как в appendBranches
  QJsonObject result;
  result["by_name"]=QJsonArray();
  result["by_text"]=QJsonArray();

  // Собираем через публичный обход модели
  collectFromModel(byName, byText);

  QJsonArray nameGroups;
  QMap<QString, QList<QJsonObject>>::const_iterator nameIt;

  for(nameIt=byName.constBegin();
      nameIt!=byName.constEnd() && nameGroups.size()<limit;
      ++nameIt)
  {
    if(nameIt.value().size()<2)
      continue;

    QJsonObject group;
    group["name"]=nameIt.value().first().value("name").toString();
    group["count"]=nameIt.value().size();

    QJsonArray members;

    foreach(const QJsonObject &member, nameIt.value())
      members.append(member);

    group["notes"]=members;
    nameGroups.append(group);
  }

  QJsonArray textGroups;
  QMap<QString, QList<QJsonObject>>::const_iterator textIt;

  for(textIt=byText.constBegin();
      textIt!=byText.constEnd() && textGroups.size()<limit;
      ++textIt)
  {
    if(textIt.value().size()<2)
      continue;

    QJsonObject group;
    group["count"]=textIt.value().size();

    QJsonArray members;

    foreach(const QJsonObject &member, textIt.value())
      members.append(member);

    group["notes"]=members;
    textGroups.append(group);
  }

  result["by_name"]=nameGroups;
  result["by_text"]=textGroups;

  return result;
}

void McpTools::collectFromModel(QMap<QString, QList<QJsonObject>> &byName,
                                QMap<QString, QList<QJsonObject>> &byText)
{
  TreeItem *root=modelRoot(m_model);

  if(root!=nullptr)
    collectDuplicates(root, byName, byText);
}

// Тот же разбор меток что в статистике и поиске по клику:
// разделители запятая/точка с запятой, пробелы по краям долой

// Тот же разбор меток что в статистике и поиске по клику:
// разделители запятая/точка с запятой, пробелы по краям долой
static QStringList splitTags(const QString &tagsField)
{
  QStringList result;

  QStringList parts=tagsField.split(QRegularExpression("[,;]+"), Qt::SkipEmptyParts);

  foreach(const QString &part, parts)
  {
    QString tag=part.trimmed();

    if(!tag.isEmpty())
      result.append(tag);
  }

  return result;
}

// Обход всех записей базы колбэком. Файлы не трогаются

QJsonObject McpTools::listTags(void)
{
  // Группировка без учета регистра: имя для показа - первый вариант
  QMap<QString, QPair<QString, int>> aggregate;

  forEachRecord(modelRoot(m_model),
                [&](Record *record, TreeItem *)
  {
    foreach(const QString &tag, splitTags(record->getField("tags")))
    {
      QString key=tag.toLower();
      QMap<QString, QPair<QString, int>>::iterator it=aggregate.find(key);

      if(it==aggregate.end())
        aggregate.insert(key, qMakePair(tag, 1));
      else
        it.value().second++;
    }
  });

  QList<QPair<QString, int>> sorted;

  // Range-for вместо foreach: запятая в QPair ломает макрос Q_FOREACH
  for(const auto &entry : aggregate.values())
    sorted.append(entry);

  std::sort(sorted.begin(), sorted.end(),
            [](const QPair<QString, int> &a, const QPair<QString, int> &b)
  {
    if(a.second!=b.second)
      return a.second>b.second;

    return QString::localeAwareCompare(a.first, b.first)<0;
  });

  QJsonArray tags;

  int shown=0;

  for(const auto &entry : sorted)
  {
    if(shown>=McpTools::MAX_LIST_LIMIT)
      break;

    QJsonObject item;
    item["name"]=entry.first;
    item["count"]=entry.second;
    tags.append(item);
    shown++;
  }

  QJsonObject result;
  result["tags"]=tags;
  result["truncated"]=(sorted.size()>shown);

  return result;
}

QJsonObject McpTools::tagNotes(const QString &tag, int limit, QString &error)
{
  QJsonObject result;

  QString key=tag.trimmed().toLower();

  if(key.isEmpty())
  {
    error="Empty tag";

    return result;
  }

  if(limit<1)
    limit=1;

  if(limit>McpTools::MAX_LIST_LIMIT)
    limit=McpTools::MAX_LIST_LIMIT;

  QJsonArray collected;

  forEachRecord(modelRoot(m_model),
                [&](Record *record, TreeItem *item)
  {
    foreach(const QString &recordTag, splitTags(record->getField("tags")))
      if(recordTag.toLower()==key)
      {
        collected.append(noteObject(record, item->getField("id"),
                                    branchPathOf(item)));
        break;
      }
  });

  result["truncated"]=(collected.size()>limit);

  QJsonArray notes;

  for(int i=0; i<collected.size() && i<limit; ++i)
    notes.append(collected.at(i));

  result["notes"]=notes;

  return result;
}

QJsonObject McpTools::untaggedNotes(int limit)
{
  if(limit<1)
    limit=1;

  if(limit>200)
    limit=200;

  QJsonArray collected;

  forEachRecord(modelRoot(m_model),
                [&](Record *record, TreeItem *item)
  {
    if(splitTags(record->getField("tags")).isEmpty())
      collected.append(noteObject(record, item->getField("id"),
                                  branchPathOf(item)));
  });

  QJsonObject result;
  result["truncated"]=(collected.size()>limit);

  QJsonArray notes;

  for(int i=0; i<collected.size() && i<limit; ++i)
    notes.append(collected.at(i));

  result["notes"]=notes;

  return result;
}

bool McpTools::applyRenameTag(KnowTreeModel *model,
                              const QString &oldTag,
                              const QString &newTag,
                              QString &error,
                              int &renamed,
                              int &skipped)
{
  renamed=0;
  skipped=0;

  QString oldKey=oldTag.trimmed().toLower();
  QString freshTag=newTag.trimmed();

  if(oldKey.isEmpty() || freshTag.isEmpty())
  {
    error="Empty tag name";

    return false;
  }

  if(oldKey==freshTag.toLower())
  {
    error="Same tag name";

    return false;
  }

  forEachRecord(modelRoot(model),
                [&](Record *record, TreeItem *)
  {
    // Шифрованные без ключа исключаются: поле нечитаемо, портить нельзя
    if(record->getField("crypt")=="1")
    {
      skipped++;
      return;
    }

    QStringList parts=splitTags(record->getField("tags"));
    bool hit=false;

    for(int i=0; i<parts.size(); ++i)
      if(parts.at(i).toLower()==oldKey)
      {
        parts[i]=freshTag;
        hit=true;
      }

    if(hit)
    {
      record->setField("tags", parts.join(", "));
      renamed++;
    }
  });

  return true;
}

bool McpTools::applyDeleteTag(KnowTreeModel *model,
                              const QString &tag,
                              QString &error,
                              int &cleared,
                              int &skipped)
{
  cleared=0;
  skipped=0;

  QString key=tag.trimmed().toLower();

  if(key.isEmpty())
  {
    error="Empty tag";

    return false;
  }

  forEachRecord(modelRoot(model),
                [&](Record *record, TreeItem *)
  {
    if(record->getField("crypt")=="1")
    {
      skipped++;
      return;
    }

    QStringList parts=splitTags(record->getField("tags"));
    QStringList kept;

    foreach(const QString &part, parts)
      if(part.toLower()!=key)
        kept.append(part);

    if(kept.size()!=parts.size())
    {
      record->setField("tags", kept.join(", "));
      cleared++;
    }
  });

  return true;
}

int McpTools::countTagNotes(KnowTreeModel *model, const QString &tagLower)
{
  int count=0;

  forEachRecord(modelRoot(model),
                [&](Record *record, TreeItem *)
  {
    if(record->getField("crypt")=="1")
      return;

    foreach(const QString &part, splitTags(record->getField("tags")))
      if(part.toLower()==tagLower)
      {
        count++;
        break;
      }
  });

  return count;
}

QJsonObject McpTools::renameTagDirect(const QString &oldTag,
                                      const QString &newTag,
                                      QString &error)
{
  QJsonObject result;

  int renamed=0;
  int skipped=0;

  if(!applyRenameTag(m_model, oldTag, newTag, error, renamed, skipped))
    return result;

  m_model->save();

  result["renamed_notes"]=renamed;
  result["skipped_locked"]=skipped;

  return result;
}

QJsonObject McpTools::deleteTagDirect(const QString &tag, QString &error)
{
  QJsonObject result;

  int cleared=0;
  int skipped=0;

  if(!applyDeleteTag(m_model, tag, error, cleared, skipped))
    return result;

  m_model->save();

  result["cleared_notes"]=cleared;
  result["skipped_locked"]=skipped;

  return result;
}

// Сайдкар избранного напрямую файлом: синглетон Favorites в
// долгоживущем серверном процессе кэширует и протухает, а в
// headless за деревом вообще не ходит. Формат тот же:
// <favorites version="1"><note id="..."/>...</favorites>

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

QString McpTools::branchPathOf(TreeItem *item)
{
  QStringList names=item->getPathAsField("name");
  names.removeAll(QString(""));

  return names.join(" / ");
}

QJsonObject McpTools::noteObject(Record *record, const QString &branchId, const QString &branchPath)
{
  QJsonObject note;
  note["id"]=record->getField("id");
  note["name"]=record->getField("name");
  note["branch_id"]=branchId;
  note["branch_path"]=branchPath;

  QJsonArray tags;

  QStringList parts=record->getField("tags").split(QRegularExpression("[,;]+"), Qt::SkipEmptyParts);

  foreach(const QString &part, parts)
  {
    QString tag=part.trimmed();

    if(!tag.isEmpty())
      tags.append(tag);
  }

  note["tags"]=tags;

  // Автор, ссылка, время создания и число аттачей.
  // У шифрованных без ключа getField сам вернет пусто:
  // шифротекст наружу не уходит
  note["author"]=record->getField("author");
  note["url"]=record->getField("url");
  note["ctime"]=record->getField("ctime");
  note["attach_count"]=record->getField("attachCount").toInt();

  // Шифр виден только флагом: текст без ключа не отдаётся.
  // Обычные записи ключа не несут, пейлоады не пухнут
  if(record->getField("crypt")=="1")
    note["encrypted"]=true;

  return note;
}

// Оставить только запрошенные ключи. Пустой список = весь объект.
// Неизвестные ключи - явная ошибка, а не молчаливое dropping:
// иначе нейронка просит text у list_notes, не получает его
// и решает что текст прочитать нечем. Текст живет в get_note

// Оставить только запрошенные ключи. Пустой список = весь объект.
// Неизвестные ключи - явная ошибка, а не молчаливое dropping:
// иначе нейронка просит text у list_notes, не получает его
// и решает что текст прочитать нечем. Текст живет в get_note
static QJsonObject filterNoteFields(const QJsonObject &note, const QStringList &fields, QString &error)
{
  if(fields.isEmpty())
    return note;

  static const QSet<QString> knownKeys=QSet<QString>()
    << "id" << "name" << "branch_id" << "branch_path" << "tags"
    << "encrypted" << "author" << "url" << "ctime" << "attach_count"
    << "format" << "text" << "truncated";

  QStringList unknown;

  foreach(const QString &key, fields)
    if(!knownKeys.contains(key))
      unknown.append(key);

  if(!unknown.isEmpty())
  {
    error="Unknown note fields: "+unknown.join(", ")+
          ". Note text lives in get_note, call it with note_id.";
    return QJsonObject();
  }

  QJsonObject result;

  foreach(const QString &key, fields)
    if(note.contains(key))
      result[key]=note.value(key);

  return result;
}

// Одна ветка плоским объектом: id, имя, родитель, итоги из кеша.
// Определение ниже, перед listBranches

// Одна ветка плоским объектом: id, имя, родитель, итоги из кеша.
// Определение ниже, перед listBranches
static QJsonObject flatBranchObject(TreeItem *item, const QString &parentId);

void McpTools::appendBranches(const QModelIndex &parent, const QString &parentId, QJsonArray &out)
{
  int rows=m_model->rowCount(parent);

  for(int r=0; r<rows; ++r)
  {
    QModelIndex index=m_model->index(r, 0, parent);
    TreeItem *item=m_model->getItem(index);

    if(item==nullptr)
      continue;

    QString id=item->getField("id");

    out.append(flatBranchObject(item, parentId));

    appendBranches(index, id, out);
  }
}

// Одна ветка плоским объектом: id, имя, родитель, итоги из кеша

// Одна ветка плоским объектом: id, имя, родитель, итоги из кеша
static QJsonObject flatBranchObject(TreeItem *item, const QString &parentId)
{
  QJsonObject branch;
  branch["id"]=item->getField("id");
  branch["name"]=item->getField("name");
  branch["parent_id"]=parentId;

  if(item->getField("crypt")=="1")
    branch["encrypted"]=true;

  // Свои и общие с подветками: оба из инкрементального кеша, O(1)
  branch["notes_count"]=item->recordtableGetRowCount();
  branch["notes_total"]=item->subtreeRecordCount();

  return branch;
}

QJsonObject McpTools::listBranches(bool recursive, const QString &parentId,
                                   QString &error)
{
  QJsonArray branches;

  // Старт: корень или указанная ветка. Пустой parent_id = корень
  QModelIndex start;
  QString startParentId;

  if(!parentId.isEmpty())
  {
    TreeItem *parentItem=m_model->getItemById(parentId);

    if(parentItem==nullptr)
    {
      error="Unknown branch_id: "+parentId;

      return QJsonObject();
    }

    start=m_model->getIndexByItem(parentItem);
    startParentId=parentId;
  }

  if(!recursive)
  {
    // Только прямой уровень под стартом: без рекурсии вглубь
    const int rows=m_model->rowCount(start);

    for(int r=0; r<rows; ++r)
    {
      TreeItem *item=m_model->getItem(m_model->index(r, 0, start));

      if(item==nullptr)
        continue;

      branches.append(flatBranchObject(item, startParentId));
    }
  }
  else
    appendBranches(start, startParentId, branches); // рекурсивно, по требованию

  QJsonObject result;
  result["branches"]=branches;

  return result;
}

void McpTools::collectBranchNotes(TreeItem *item, bool recursive, QJsonArray &out)
{
  QString branchId=item->getField("id");
  QString path=branchPathOf(item);

  RecordTableData *table=item->recordtableGetTableData();

  if(table!=nullptr)
    for(unsigned i=0; i<table->size(); ++i)
    {
      Record *record=table->getRecord(i);

      if(record==nullptr)
        continue;

      out.append(noteObject(record, branchId, path));
    }

  if(recursive)
    for(int i=0; i<item->childCount(); ++i)
      collectBranchNotes(item->child(i), true, out);
}

QJsonObject McpTools::listNotes(const QString &branchId, bool recursive,
                                int limit, QString &error,
                                const QStringList &fields)
{
  QJsonObject result;

  if(limit<1)
    limit=1;

  if(limit>McpTools::MAX_LIST_LIMIT)
    limit=McpTools::MAX_LIST_LIMIT;

  TreeItem *start=m_model->getItemById(branchId);

  if(start==nullptr)
  {
    error="Unknown branch_id: "+branchId;

    return result;
  }

  QJsonArray collected;
  collectBranchNotes(start, recursive, collected);

  result["truncated"]=(collected.size()>limit);

  QJsonArray notes;

  for(int i=0; i<collected.size() && i<limit; ++i)
  {
    QJsonObject item=filterNoteFields(collected.at(i).toObject(), fields, error);

    if(!error.isEmpty())
      return QJsonObject();

    notes.append(item);
  }

  result["notes"]=notes;

  return result;
}

QString McpTools::readTextWithRetry(Record *record)
{
  QString dir=record->getField("dir");
  QString file=record->getField("file");

  QString fullName;
  QDateTime before;

  if(!dir.isEmpty() && !file.isEmpty())
  {
    fullName=AppContext::instance().appConfig()->get_tetradir()+"/base/"+dir+"/"+file;
    before=QFileInfo(fullName).lastModified();
  }

  QString html=record->getTextForStats();

  // Файл перезаписали прямо во время чтения (с атомарным сейвом
  // рваного состояния не бывает, но перечитать свежак также
  // несложно) - один повтор с паузой
  if(!fullName.isEmpty() && QFileInfo(fullName).lastModified()!=before)
  {
    QThread::msleep(20);
    html=record->getTextForStats();
  }

  return html;
}

QJsonObject McpTools::getNote(const QString &noteId, int maxChars, QString &error,
                               const QStringList &fields)
{
  QJsonObject result;

  if(maxChars<1)
    maxChars=1;

  if(maxChars>McpTools::MAX_TEXT_LENGTH)
    maxChars=McpTools::MAX_TEXT_LENGTH;

  if(!m_model->isRecordIdExists(noteId))
  {
    error="Unknown note_id: "+noteId;

    return result;
  }

  Record *record=m_model->getRecord(noteId);

  if(record==nullptr)
  {
    error="Unknown note_id: "+noteId;

    return result;
  }

  QString branchId;
  QStringList path=m_model->getRecordPath(noteId);

  if(!path.isEmpty())
    branchId=path.last();

  TreeItem *branchItem=m_model->getItemById(branchId);

  QString branchPath=(branchItem!=nullptr) ? branchPathOf(branchItem) : QString();

  QJsonObject note=noteObject(record, branchId, branchPath);
  note["format"]="text";

  QString plain=htmlToPlain(readTextWithRetry(record));

  bool truncated=false;

  if(plain.length()>maxChars)
  {
    plain=plain.left(maxChars);
    truncated=true;
  }

  note["text"]=plain;
  note["truncated"]=truncated;

  result["note"]=filterNoteFields(note, fields, error);

  if(!error.isEmpty())
    return QJsonObject();

  return result;
}

static QString makeSnippet(const QString &plain, int matchPos, int matchLength)
{
  int from=qMax(0, matchPos-MCP_SNIPPET_CONTEXT);
  int to=qMin(plain.length(), matchPos+matchLength+MCP_SNIPPET_CONTEXT);

  QString snippet=plain.mid(from, to-from);

  if(from>0)
    snippet=QString::fromUtf8("…")+snippet;

  if(to<plain.length())
    snippet=snippet+QString::fromUtf8("…");

  return snippet;
}

void McpTools::searchBranch(TreeItem *item, const QString &queryLower, int limit,
                               QJsonArray &noteOut, QJsonArray &branchOut)
{
  QString branchId=item->getField("id");
  QString branchName=item->getField("name");
  QString path=branchPathOf(item);

  // Ветка подходит по имени: отдельным списком, чтобы нейронка
  // не перебирала все ветки ради поиска нужной.
  // Корневые ветки несут пустой parent_id, как в list_branches
  if(branchOut.size()<limit && branchName.toLower().contains(queryLower))
  {
    TreeItem *parentItem=item->parent();
    const bool isRootLevel=(parentItem==nullptr ||
                            parentItem->parent()==nullptr);

    QJsonObject found;
    found["id"]=branchId;
    found["name"]=branchName;
    found["parent_id"]=isRootLevel ? QString() : parentItem->getField("id");
    found["branch_path"]=path;
    branchOut.append(found);
  }

  if(noteOut.size()>=limit)
  {
    // Заметки набраны, но подветки по именам все равно проходятся:
    // ветки дешевые, тексты записей уже не читаются
    for(int i=0; i<item->childCount() && branchOut.size()<limit; ++i)
      searchBranch(item->child(i), queryLower, limit, noteOut, branchOut);

    return;
  }

  RecordTableData *table=item->recordtableGetTableData();

  if(table!=nullptr)
    for(unsigned i=0; i<table->size() && noteOut.size()<limit; ++i)
    {
      Record *record=table->getRecord(i);

      if(record==nullptr)
        continue;

      QString name=record->getField("name");
      QString tagsLine=record->getField("tags");

      QJsonObject hit;
      bool matched=false;

      int namePos=name.toLower().indexOf(queryLower);

      if(namePos>=0)
      {
        hit["snippet"]=makeSnippet(name, namePos, queryLower.length());
        matched=true;
      }
      else
      {
        int tagsPos=tagsLine.toLower().indexOf(queryLower);

        if(tagsPos>=0)
        {
          hit["snippet"]=makeSnippet(tagsLine, tagsPos, queryLower.length());
          matched=true;
        }
        else
        {
          QString plain=htmlToPlain(readTextWithRetry(record));

          int textPos=plain.toLower().indexOf(queryLower);

          if(textPos>=0)
          {
            hit["snippet"]=makeSnippet(plain, textPos, queryLower.length());
            matched=true;
          }
        }
      }

      if(matched)
      {
        hit["id"]=record->getField("id");
        hit["name"]=name;
        hit["branch_path"]=path;

        // branch_id здесь - ветка где реально лежит запись
        hit["branch_id"]=branchId;

        noteOut.append(hit);
      }
    }

  for(int i=0; i<item->childCount() && (noteOut.size()<limit || branchOut.size()<limit); ++i)
    searchBranch(item->child(i), queryLower, limit, noteOut, branchOut);
}

void McpTools::searchModel(const QModelIndex &parent, const QString &queryLower, int limit,
                       QJsonArray &noteOut, QJsonArray &branchOut)
{
  int rows=m_model->rowCount(parent);

  for(int r=0; r<rows && (noteOut.size()<limit || branchOut.size()<limit); ++r)
  {
    QModelIndex index=m_model->index(r, 0, parent);
    TreeItem *item=m_model->getItem(index);

    if(item==nullptr)
      continue;

    searchBranch(item, queryLower, limit, noteOut, branchOut);
  }
}

QJsonObject McpTools::searchNotes(const QString &query, int limit, QString &error)
{
  QJsonObject result;

  QString trimmed=query.trimmed();

  if(trimmed.isEmpty())
  {
    error="Empty query";

    return result;
  }

  if(limit<1)
    limit=1;

  if(limit>100)
    limit=100;

  QJsonArray hits;
  QJsonArray branchHits;
  searchModel(QModelIndex(), trimmed.toLower(), limit, hits, branchHits);
  result["results"]=hits;
  result["branches"]=branchHits;

  return result;
}

QString McpTools::htmlToPlain(const QString &html)
{
  QString text=html;

  // Блоки script/style вырезаются целиком вместе с содержимым
  text.replace(QRegularExpression("<(script|style)[^>]*>.*?</\\1\\s*>",
                                  QRegularExpression::CaseInsensitiveOption |
                                  QRegularExpression::DotMatchesEverythingOption),
               " ");

  // Остальные теги меняются на пробел чтобы слова не склеивались
  text.replace(QRegularExpression("<[^>]*>"), " ");

  // Сущности: сначала &amp;, иначе &amp;lt; раскроется неверно
  text.replace("&amp;", "&");
  text.replace("&nbsp;", " ");
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  text.replace("&quot;", "\"");
  text.replace("&apos;", "'");

  // Десятичные числовые сущности &#1040;
  QRegularExpression decimalPattern("&#(\\d+);");
  QRegularExpressionMatch decimalMatch;

  while((decimalMatch=decimalPattern.match(text)).hasMatch())
  {
    bool ok=false;
    uint code=decimalMatch.captured(1).toUInt(&ok);

    QString replacement=decimalMatch.captured(0);

    if(ok && code>0 && code<=0xFFFF)
      replacement=QString(QChar(code));

    text.replace(decimalMatch.capturedStart(0), decimalMatch.capturedLength(0), replacement);
  }

  // Шестнадцатеричные числовые сущности &#x410;
  QRegularExpression hexPattern("&#x([0-9a-fA-F]+);");
  QRegularExpressionMatch hexMatch;

  while((hexMatch=hexPattern.match(text)).hasMatch())
  {
    bool ok=false;
    uint code=hexMatch.captured(1).toUInt(&ok, 16);

    QString replacement=hexMatch.captured(0);

    if(ok && code>0 && code<=0xFFFF)
      replacement=QString(QChar(code));

    text.replace(hexMatch.capturedStart(0), hexMatch.capturedLength(0), replacement);
  }

  text.replace(QRegularExpression("\\s+"), " ");

  return text.trimmed();
}

QString McpTools::plainToHtml(const QString &plain)
{
  QString html=plain;

  // Порядок важен: сначала амперсанд, иначе &lt; удвоится
  html.replace("&", "&amp;");
  html.replace("<", "&lt;");
  html.replace(">", "&gt;");
  html.replace("\n", "<br/>");

  return html;
}

// Корзина готова принять файлы: есть и пишемся. Иначе удаление
// уперлось бы в criticalError с диалогом внутри, а в headless
// это падение процесса. Создается при отсутствии как при установке

// Корень модели: getRootItem отдает const, модель по факту живая
TreeItem *modelRoot(KnowTreeModel *model)
{
  return const_cast<TreeItem*>(model->getRootItem());
}

// Id сиблинга с таким именем среди детей parent (без учёта регистра,
// пробелы по краям долой). Пусто если свободно. excludeId пропускается:
// переименование самого себя и перенос внутри своего уровня не дубль

// Обход всех записей базы колбэком. Файлы не трогаются
void forEachRecord(TreeItem *item,
                          std::function<void(Record*, TreeItem*)> visit)
{
  if(item==nullptr)
    return;

  RecordTableData *table=item->recordtableGetTableData();

  if(table!=nullptr)
    for(unsigned i=0; i<table->size(); ++i)
    {
      Record *record=table->getRecord(i);

      if(record!=nullptr)
        visit(record, item);
    }

  for(int i=0; i<item->childCount(); ++i)
    forEachRecord(item->child(i), visit);
}

QJsonObject McpTools::debugInfo(void)
{
  QJsonObject result;

  result["app"]=GlobalParameters::getApplicationName();
  result["version"]=QString::number(APPLICATION_RELEASE_VERSION)+"."+
                    QString::number(APPLICATION_RELEASE_SUBVERSION)+"."+
                    QString::number(APPLICATION_RELEASE_MICROVERSION);

  const QString tetradir=AppContext::instance().appConfig()->get_tetradir();
  const QString xmlPath=tetradir+"/mytetra.xml";
  const QFileInfo xmlInfo(xmlPath);

  result["tetradir"]=tetradir;
  result["xml"]=xmlPath;
  result["xml_exists"]=xmlInfo.isFile();

  if(xmlInfo.exists())
    result["xml_mtime"]=xmlInfo.lastModified().toString(Qt::ISODate);

  return result;
}

QJsonObject McpTools::baseStats(bool detailed)
{
  QJsonObject result;

  // Тот же порядок что в статусбаре: ветки, записи, метки,
  // файлы, картинки, внутренние ссылки
  int branches=0;
  int records=0;
  int files=0;
  int images=0;
  int links=0;
  int tags=0;

  m_model->getBaseStats(branches, records, files, images, links, tags);

  result["branches"]=branches;
  result["records"]=records;
  result["tags"]=tags;
  result["files"]=files;
  result["images"]=images;
  result["links"]=links;

  if(detailed)
  {
    QList<BaseStatRow> all=m_model->getBaseStatRows();

    QJsonArray rows;

    int shown=0;

    foreach(const BaseStatRow &row, all)
    {
      if(shown>=McpTools::MAX_LIST_LIMIT)
        break;

      QJsonObject item;
      item["branch_path"]=row.branchPath;
      item["note_name"]=row.noteName;
      item["record_id"]=row.recordId;
      item["files"]=row.files;
      item["images"]=row.images;
      item["links"]=row.links;
      item["tags"]=row.tags;
      rows.append(item);
      shown++;
    }

    result["rows"]=rows;
    result["truncated"]=(all.size()>shown);
  }

  return result;
}

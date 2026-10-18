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

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;

// Контекст сниппета поиска с каждой стороны от совпадения
static const int MCP_SNIPPET_CONTEXT=100;


McpTools::McpTools(KnowTreeModel *model) : m_model(model)
{
}


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
    fullName=mytetraConfig.get_tetradir()+"/base/"+dir+"/"+file;
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
static bool trashReady(QString &error)
{
  QString trashDir=mytetraConfig.get_trashdir();

  if(!QDir(trashDir).exists())
  {
    if(!QDir().mkpath(trashDir))
    {
      error="Trash directory can not be created: "+trashDir;

      return false;
    }
  }

  QString probeName=QDir(trashDir).filePath(".mcp-write-test");

  QFile probe(probeName);

  if(!probe.open(QIODevice::WriteOnly))
  {
    error="Trash directory is not writable: "+trashDir;

    return false;
  }

  probe.close();
  probe.remove();

  return true;
}


// Корень модели: getRootItem отдает const, модель по факту живая
static TreeItem *modelRoot(KnowTreeModel *model)
{
  return const_cast<TreeItem*>(model->getRootItem());
}


// Id сиблинга с таким именем среди детей parent (без учёта регистра,
// пробелы по краям долой). Пусто если свободно. excludeId пропускается:
// переименование самого себя и перенос внутри своего уровня не дубль
static QString takenSiblingId(TreeItem *parent, const QString &name,
                              const QString &excludeId)
{
  if(parent==nullptr)
    return QString();

  QString key=name.trimmed().toLower();

  for(int i=0; i<parent->childCount(); ++i)
  {
    TreeItem *child=parent->child(i);

    if(child==nullptr)
      continue;

    if(!excludeId.isEmpty() && child->getField("id")==excludeId)
      continue;

    if(child->getField("name").trimmed().toLower()==key)
      return child->getField("id");
  }

  return QString();
}


// Id заметки с таким именем внутри ветки (без учёта регистра).
// Пусто если свободно. excludeId пропускается: своё же имя не дубль
static QString takenNoteId(RecordTableData *table, const QString &name,
                           const QString &excludeId)
{
  if(table==nullptr)
    return QString();

  QString key=name.trimmed().toLower();

  for(unsigned i=0; i<table->size(); ++i)
  {
    Record *other=table->getRecord(i);

    if(other==nullptr)
      continue;

    if(!excludeId.isEmpty() && other->getField("id")==excludeId)
      continue;

    if(other->getField("name").trimmed().toLower()==key)
      return other->getField("id");
  }

  return QString();
}


// Ветка, содержащая запись: полный обход, первая найденная
static TreeItem *findBranchOfNote(TreeItem *item, const QString &noteId)
{
  if(item==nullptr)
    return nullptr;

  RecordTableData *table=item->recordtableGetTableData();

  if(table!=nullptr)
    for(unsigned i=0; i<table->size(); ++i)
    {
      Record *record=table->getRecord(i);

      if(record!=nullptr && record->getField("id")==noteId)
        return item;
    }

  for(int i=0; i<item->childCount(); ++i)
  {
    TreeItem *found=findBranchOfNote(item->child(i), noteId);

    if(found!=nullptr)
      return found;
  }

  return nullptr;
}


QJsonObject McpTools::applyCreateBranch(KnowTreeModel *model,
                                        const QString &parentId,
                                        const QString &branchId,
                                        const QString &name,
                                        QString &error)
{
  QJsonObject result;

  if(branchId.isEmpty())
  {
    error="Empty branch id";

    return result;
  }

  if(name.trimmed().isEmpty())
  {
    error="Empty branch name";

    return result;
  }

  if(model->isItemIdExists(branchId))
  {
    error="Branch id collision, retry with a fresh id: "+branchId;

    return result;
  }

  // Невалидный индекс модели это корень: пустой parentId создает
  // ветку верхнего уровня
  QModelIndex parentIndex;
  TreeItem *parentItem=modelRoot(model);

  if(!parentId.isEmpty())
  {
    TreeItem *parent=model->getItemById(parentId);

    if(parent==nullptr)
    {
      error="Unknown parent branch_id: "+parentId;

      return result;
    }

    // В шифрованную ветку без ключа нельзя: switchToEncrypt
    // у новой ветки отработает некорректно
    if(parent->getField("crypt")=="1")
    {
      error="Parent branch is encrypted, creation refused";

      return result;
    }

    parentIndex=model->getIndexByItem(parent);
    parentItem=parent;
  }

  QString dupBranchId=takenSiblingId(parentItem, name, QString());

  if(!dupBranchId.isEmpty())
  {
    error="Branch already exists at this level: "+name.trimmed()+
          " (id: "+dupBranchId+")";

    return result;
  }

  QMap<QString, QString> fields;
  fields["id"]=branchId;
  fields["name"]=name.trimmed();

  model->addNewChildBranch(parentIndex, fields);

  result["id"]=branchId;

  return result;
}


QJsonObject McpTools::applyCreateNote(KnowTreeModel *model,
                                      const QString &branchId,
                                      const QString &noteId,
                                      const QString &name,
                                      const QString &html,
                                      const QString &tags,
                                      QString &error)
{
  QJsonObject result;

  if(noteId.isEmpty())
  {
    error="Empty note id";

    return result;
  }

  if(name.trimmed().isEmpty())
  {
    error="Empty note name";

    return result;
  }

  TreeItem *branch=model->getItemById(branchId);

  if(branch==nullptr)
  {
    error="Unknown branch_id: "+branchId;

    return result;
  }

  // В шифрованную ветку без ключа нельзя: insertNewRecord
  // аварийно завершится внутри
  if(branch->getField("crypt")=="1")
  {
    error="Branch is encrypted, creation refused";

    return result;
  }

  RecordTableData *table=branch->recordtableGetTableData();

  if(table==nullptr)
  {
    error="Branch has no record table";

    return result;
  }

  QString dupNoteId=takenNoteId(table, name, QString());

  if(!dupNoteId.isEmpty())
  {
    error="Note already exists in this branch: "+name.trimmed()+
          " (id: "+dupNoteId+")";

    return result;
  }

  // Тот же набор полей что Clipper::storeRecord при ручном создании
  Record record;
  record.switchToFat();
  record.setText(html);
  record.setField("id", noteId);
  record.setField("dir", getUniqueId());
  record.setField("file", "text.html");
  record.setField("name", name.trimmed());
  record.setField("author", QString());
  record.setField("url", QString());
  record.setField("tags", tags);

  int pos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                 0, record);

  if(pos<0)
  {
    error="Can not insert note into branch: "+branchId;

    return result;
  }

  Record *stored=table->getRecord(pos);

  if(stored==nullptr)
  {
    error="Can not insert note into branch: "+branchId;

    return result;
  }

  // Headless insert всегда заменяет id на свежий (вида нет),
  // в GUI сохраняется переданный: возвращается фактический
  result["id"]=stored->getField("id");

  return result;
}


bool McpTools::applyUpdateNote(KnowTreeModel *model,
                               const QString &noteId,
                               bool hasName, const QString &name,
                               bool hasHtml, const QString &html,
                               bool hasTags, const QString &tags,
                               QString &error,
                               QStringList &updatedFields)
{
  if(!hasName && !hasHtml && !hasTags)
  {
    error="Nothing to update: provide name, text or tags";

    return false;
  }

  if(!model->isRecordIdExists(noteId))
  {
    error="Unknown note_id: "+noteId;

    return false;
  }

  Record *record=model->getRecord(noteId);

  if(record==nullptr)
  {
    error="Unknown note_id: "+noteId;

    return false;
  }

  // Шифрованную запись без ключа нельзя ни прочитать, ни
  // перезаписать без порчи данных
  if(record->getField("crypt")=="1")
  {
    error="Note is encrypted, update refused";

    return false;
  }

  if(hasName)
  {
    if(name.trimmed().isEmpty())
    {
      error="Empty note name";

      return false;
    }

    TreeItem *noteBranch=findBranchOfNote(modelRoot(model), noteId);

    RecordTableData *noteTable=(noteBranch!=nullptr)
                               ? noteBranch->recordtableGetTableData()
                               : nullptr;

    QString dupNoteId=takenNoteId(noteTable, name, noteId);

    if(!dupNoteId.isEmpty())
    {
      error="Note already exists in this branch: "+name.trimmed()+
            " (id: "+dupNoteId+")";

      return false;
    }

    record->setField("name", name.trimmed());
    updatedFields.append("name");
  }

  if(hasTags)
  {
    record->setField("tags", tags);
    updatedFields.append("tags");
  }

  if(hasHtml)
  {
    // Только публичный API: в память, на диск (атомарно через
    // pushFatAttributes), память освободить. Параллельный читатель
    // рваного файла не увидит
    record->switchToFat();
    record->setText(html);
    record->pushFatAttributes();
    record->switchToLite();
    updatedFields.append("text");
  }

  return true;
}


QJsonObject McpTools::createBranchDirect(const QString &parentId,
                                         const QString &branchId,
                                         const QString &name,
                                         QString &error)
{
  QJsonObject result=applyCreateBranch(m_model, parentId, branchId, name, error);

  if(!error.isEmpty())
    return result;

  m_model->save();

  return result;
}


QJsonObject McpTools::createNoteDirect(const QString &branchId,
                                       const QString &noteId,
                                       const QString &name,
                                       const QString &html,
                                       const QString &tags,
                                       QString &error)
{
  QJsonObject result=applyCreateNote(m_model, branchId, noteId, name, html, tags, error);

  if(!error.isEmpty())
    return result;

  m_model->save();

  return result;
}


QJsonObject McpTools::updateNoteDirect(const QString &noteId,
                                       bool hasName, const QString &name,
                                       bool hasHtml, const QString &html,
                                       bool hasTags, const QString &tags,
                                       QString &error)
{
  QJsonObject result;

  QStringList updatedFields;

  if(!applyUpdateNote(m_model, noteId,
                      hasName, name, hasHtml, html, hasTags, tags,
                      error, updatedFields))
    return result;

  m_model->save();

  result["id"]=noteId;
  result["updated_fields"]=QJsonArray::fromStringList(updatedFields);

  return result;
}


bool McpTools::applyDeleteNote(KnowTreeModel *model,
                               const QString &noteId,
                               QString &error)
{
  if(!model->isRecordIdExists(noteId))
  {
    error="Unknown note_id: "+noteId;

    return false;
  }

  if(!trashReady(error))
    return false;

  QStringList path=model->getRecordPath(noteId);

  if(path.isEmpty())
  {
    error="Unknown note_id: "+noteId;

    return false;
  }

  TreeItem *branch=model->getItemById(path.last());

  if(branch==nullptr)
  {
    error="Unknown note_id: "+noteId;

    return false;
  }

  RecordTableData *table=branch->recordtableGetTableData();

  if(table==nullptr)
  {
    error="Branch has no record table";

    return false;
  }

  // Каталог записи уезжает в корзину внутри, запись из таблицы долой
  table->deleteRecordById(noteId);

  // Сайдкар бэклинков: только если есть что чистить, иначе
  // ensureLoaded построит и сохранит пустой индекс без нужды
  QString sidecar=QDir::cleanPath(mytetraConfig.get_tetradir()+"/backlinks.xml");

  if(QFile::exists(sidecar))
    BacklinkIndex::instance().removeSources(QStringList() << noteId);

  return true;
}


QJsonObject McpTools::applyDeleteBranch(KnowTreeModel *model,
                                        const QString &branchId,
                                        QString &error)
{
  QJsonObject result;

  TreeItem *item=model->getItemById(branchId);

  if(item==nullptr)
  {
    error="Unknown branch_id: "+branchId;

    return result;
  }

  if(!trashReady(error))
    return result;

  // Итог из кеша подветки: O(1) вместо рекурсивного
  // построения QSet идентификаторов ради одного числа
  const int notesCount=item->subtreeRecordCount();

  QModelIndexList indexList;
  indexList.append(model->getIndexByItem(item));

  // Каталоги записей уезжают в корзину внутри, detached-окна
  // закрываются сигналом, сайдкар бэклинков чистится
  model->deleteItemsByModelIndexList(indexList);

  result["id"]=branchId;
  result["deleted"]=true;
  result["deleted_notes"]=notesCount;

  return result;
}


QJsonObject McpTools::deleteNoteDirect(const QString &noteId, QString &error)
{
  QJsonObject result;

  if(!applyDeleteNote(m_model, noteId, error))
    return result;

  m_model->save();

  result["id"]=noteId;
  result["deleted"]=true;

  return result;
}


QJsonObject McpTools::deleteBranchDirect(const QString &branchId, QString &error)
{
  QJsonObject result=applyDeleteBranch(m_model, branchId, error);

  if(!error.isEmpty())
    return result;

  m_model->save();

  return result;
}


QJsonObject McpTools::applyRenameBranch(KnowTreeModel *model,
                                        const QString &branchId,
                                        const QString &name,
                                        QString &error)
{
  QJsonObject result;

  if(name.trimmed().isEmpty())
  {
    error="Empty branch name";

    return result;
  }

  TreeItem *item=model->getItemById(branchId);

  if(item==nullptr)
  {
    error="Unknown branch_id: "+branchId;

    return result;
  }

  QString dupBranchId=takenSiblingId(item->parent(), name, branchId);

  if(!dupBranchId.isEmpty())
  {
    error="Branch already exists at this level: "+name.trimmed()+
          " (id: "+dupBranchId+")";

    return result;
  }

  item->setField("name", name.trimmed());

  result["id"]=branchId;

  return result;
}


QJsonObject McpTools::applyMoveBranch(KnowTreeModel *model,
                                      const QString &sourceId,
                                      const QString &targetId,
                                      bool asChild,
                                      QString &error)
{
  QJsonObject result;

  TreeItem *source=model->getItemById(sourceId);

  if(source==nullptr)
  {
    error="Unknown branch_id: "+sourceId;

    return result;
  }

  TreeItem *target=model->getItemById(targetId);

  if(target==nullptr)
  {
    error="Unknown target branch_id: "+targetId;

    return result;
  }

  // Новый родитель: внутрь цели или на её уровень. Дубль имени
  // на целевом уровне запрещён как при создании
  TreeItem *newParent=asChild ? target : target->parent();

  if(newParent==nullptr)
    newParent=modelRoot(model);

  QString dupBranchId=takenSiblingId(newParent, source->getField("name"),
                                    sourceId);

  if(!dupBranchId.isEmpty())
  {
    error="Branch already exists at this level: "+
          source->getField("name").trimmed()+" (id: "+dupBranchId+")";

    return result;
  }

  if(!model->moveBranch(sourceId, targetId, asChild))
  {
    error="Move refused: self-move, root or target inside source";

    return result;
  }

  // Новый родитель: предпоследний id в пути (пусто - верхний уровень)
  QString newParentId;
  TreeItem *moved=model->getItemById(sourceId);

  if(moved!=nullptr)
  {
    QStringList path=moved->getPath();

    if(path.size()>=2)
      newParentId=path.at(path.size()-2);
  }

  result["id"]=sourceId;
  result["new_parent_id"]=newParentId;

  return result;
}


QJsonObject McpTools::applyMoveNote(KnowTreeModel *model,
                                    const QString &noteId,
                                    const QString &targetBranchId,
                                    QString &error)
{
  QJsonObject result;

  if(!model->isRecordIdExists(noteId))
  {
    error="Unknown note_id: "+noteId;

    return result;
  }

  QStringList path=model->getRecordPath(noteId);

  if(path.isEmpty())
  {
    error="Unknown note_id: "+noteId;

    return result;
  }

  QString sourceBranchId=path.last();

  if(sourceBranchId==targetBranchId)
  {
    error="Note is already in this branch";

    return result;
  }

  TreeItem *source=model->getItemById(sourceBranchId);
  TreeItem *target=model->getItemById(targetBranchId);

  if(source==nullptr || target==nullptr)
  {
    error="Unknown branch for move";

    return result;
  }

  // Шифр с обеих сторон закрыт: переходы между шифром и открытыми
  // требуют ключа и преобразований, в MVP отказ
  if(source->getField("crypt")=="1" || target->getField("crypt")=="1")
  {
    error="Move to or from encrypted branch refused";

    return result;
  }

  RecordTableData *sourceTable=source->recordtableGetTableData();
  RecordTableData *targetTable=target->recordtableGetTableData();

  if(sourceTable==nullptr || targetTable==nullptr)
  {
    error="Branch has no record table";

    return result;
  }

  QString movingName;

  for(unsigned i=0; i<sourceTable->size(); ++i)
  {
    Record *candidate=sourceTable->getRecord(i);

    if(candidate!=nullptr && candidate->getField("id")==noteId)
    {
      movingName=candidate->getField("name");
      break;
    }
  }

  QString dupNoteId=takenNoteId(targetTable, movingName, QString());

  if(!dupNoteId.isEmpty())
  {
    error="Note already exists in this branch: "+movingName.trimmed()+
          " (id: "+dupNoteId+")";

    return result;
  }

  if(!sourceTable->moveRecordTo(noteId, targetTable))
  {
    error="Can not move note";

    return result;
  }

  result["id"]=noteId;
  result["branch_id"]=targetBranchId;

  return result;
}


QJsonObject McpTools::renameBranchDirect(const QString &branchId,
                                         const QString &name,
                                         QString &error)
{
  QJsonObject result=applyRenameBranch(m_model, branchId, name, error);

  if(!error.isEmpty())
    return result;

  m_model->save();

  return result;
}


QJsonObject McpTools::moveBranchDirect(const QString &sourceId,
                                       const QString &targetId,
                                       bool asChild,
                                       QString &error)
{
  QJsonObject result=applyMoveBranch(m_model, sourceId, targetId, asChild, error);

  if(!error.isEmpty())
    return result;

  m_model->save();

  return result;
}


QJsonObject McpTools::moveNoteDirect(const QString &noteId,
                                     const QString &targetBranchId,
                                     QString &error)
{
  QJsonObject result=applyMoveNote(m_model, noteId, targetBranchId, error);

  if(!error.isEmpty())
    return result;

  m_model->save();

  return result;
}


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
static void forEachRecord(TreeItem *item,
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
static QString favoritesSidecarPath(void)
{
  return QDir::cleanPath(mytetraConfig.get_tetradir()+"/favorites.xml");
}


static QSet<QString> favoritesSidecarRead(void)
{
  QSet<QString> out;

  QFile file(favoritesSidecarPath());

  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return out;

  QXmlStreamReader xml(&file);

  if(!xml.readNextStartElement() || xml.name()!=QStringLiteral("favorites"))
    return out;

  while(xml.readNextStartElement())
  {
    if(xml.name()==QStringLiteral("note"))
    {
      const QString id=xml.attributes().value(QStringLiteral("id")).toString();

      if(!id.isEmpty())
        out.insert(id);

      xml.skipCurrentElement();
    }
    else
      xml.skipCurrentElement();
  }

  return out;
}


static bool favoritesSidecarWrite(const QSet<QString> &ids)
{
  QSaveFile file(favoritesSidecarPath());

  if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement(QStringLiteral("favorites"));
  xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));

  QStringList ordered=ids.values();
  ordered.sort(Qt::CaseInsensitive);

  foreach(const QString &id, ordered)
  {
    xml.writeStartElement(QStringLiteral("note"));
    xml.writeAttribute(QStringLiteral("id"), id);
    xml.writeEndElement();
  }

  xml.writeEndElement();
  xml.writeEndDocument();

  return file.commit();
}


QJsonObject McpTools::listFavorites(void)
{
  QSet<QString> starIds;

  if(mytetraConfig.get_favoritesStorage()==QStringLiteral("xml"))
  {
    // Флаги прямо в записях: проход по живой модели.
    // Шифрованные без ключа пропускаются: флаг нечитаем
    forEachRecord(modelRoot(m_model),
                  [&](Record *record, TreeItem *)
    {
      if(record->getField("crypt")=="1")
        return;

      if(record->getField("favorite")=="1")
        starIds.insert(record->getField("id"));
    });
  }
  else
    starIds=favoritesSidecarRead();

  QStringList ordered=starIds.values();
  ordered.sort(Qt::CaseInsensitive);

  QJsonArray favorites;

  foreach(const QString &id, ordered)
  {
    // Мертвый id сайдкара: записи уже нет, молча пропускается
    if(!m_model->isRecordIdExists(id))
      continue;

    Record *record=m_model->getRecord(id);

    if(record==nullptr || record->getField("crypt")=="1")
      continue;

    QStringList path=m_model->getRecordPath(id);

    if(path.isEmpty())
      continue;

    TreeItem *branch=m_model->getItemById(path.last());

    if(branch==nullptr)
      continue;

    favorites.append(noteObject(record, path.last(), branchPathOf(branch)));
  }

  QJsonObject result;
  result["favorites"]=favorites;

  return result;
}


QJsonObject McpTools::applyFavoriteNote(KnowTreeModel *model,
                                        const QString &noteId,
                                        bool starred,
                                        QString &error)
{
  QJsonObject result;

  if(!model->isRecordIdExists(noteId))
  {
    error="Unknown note_id: "+noteId;

    return result;
  }

  Record *record=model->getRecord(noteId);

  if(record==nullptr)
  {
    error="Unknown note_id: "+noteId;

    return result;
  }

  if(record->getField("crypt")=="1")
  {
    error="Note is encrypted, favorite refused";

    return result;
  }

  if(mytetraConfig.get_favoritesStorage()==QStringLiteral("xml"))
  {
    QStringList path=model->getRecordPath(noteId);

    if(path.isEmpty())
    {
      error="Unknown note_id: "+noteId;

      return result;
    }

    TreeItem *branch=model->getItemById(path.last());

    if(branch==nullptr)
    {
      error="Unknown note_id: "+noteId;

      return result;
    }

    RecordTableData *table=branch->recordtableGetTableData();

    if(table==nullptr)
    {
      error="Unknown note_id: "+noteId;

      return result;
    }

    const int pos=table->getPosById(noteId);

    if(pos<0)
    {
      error="Unknown note_id: "+noteId;

      return result;
    }

    table->setField(QStringLiteral("favorite"), starred ? QStringLiteral("1") : QString(), pos);
  }
  else
  {
    QSet<QString> ids=favoritesSidecarRead();

    if(ids.contains(noteId)==starred)
    {
      result["id"]=noteId;
      result["favorite"]=starred;

      return result;
    }

    if(starred)
      ids.insert(noteId);
    else
      ids.remove(noteId);

    if(!favoritesSidecarWrite(ids))
    {
      error="Can not write favorites file";

      return result;
    }
  }

  result["id"]=noteId;
  result["favorite"]=starred;

  return result;
}


QJsonObject McpTools::favoriteNoteDirect(const QString &noteId, QString &error)
{
  QJsonObject result=applyFavoriteNote(m_model, noteId, true, error);

  if(!error.isEmpty())
    return result;

  // XML-бэкенд меняет дерево: сайдкар уже записан внутри apply
  if(mytetraConfig.get_favoritesStorage()==QStringLiteral("xml"))
    m_model->save();

  return result;
}


QJsonObject McpTools::unfavoriteNoteDirect(const QString &noteId, QString &error)
{
  QJsonObject result=applyFavoriteNote(m_model, noteId, false, error);

  if(!error.isEmpty())
    return result;

  if(mytetraConfig.get_favoritesStorage()==QStringLiteral("xml"))
    m_model->save();

  return result;
}


QSet<QString> McpTools::sidecarFavoriteIds(void)
{
  return favoritesSidecarRead();
}


QJsonObject McpTools::listAttachments(const QString &noteId, QString &error)
{
  QJsonObject result;

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

  if(record->getField("crypt")=="1")
  {
    error="Note is encrypted, attachments hidden";

    return result;
  }

  AttachTableData table=record->getAttachTable();

  QJsonArray attachments;

  for(int row=0; row<table.size(); ++row)
  {
    QJsonObject one;
    one["id"]=table.getIdByRow(row);
    one["name"]=table.getFileName(row);
    one["size"]=static_cast<qint64>(table.getFileSize(row));
    attachments.append(one);
  }

  result["attachments"]=attachments;

  return result;
}


// Есть ли в поддереве шифр: ветки или записи с crypt=="1".
// Ключа в серверном процессе нет, такое выгружать нельзя -
// внутри выгрузки расшифровка пустым ключом дала бы мусор
static bool subtreeHasLocked(TreeItem *item)
{
  if(item==nullptr)
    return false;

  if(item->getField("crypt")=="1")
    return true;

  RecordTableData *table=item->recordtableGetTableData();

  if(table!=nullptr)
    for(unsigned i=0; i<table->size(); ++i)
    {
      Record *record=table->getRecord(i);

      if(record!=nullptr && record->getField("crypt")=="1")
        return true;
    }

  for(int i=0; i<item->childCount(); ++i)
    if(subtreeHasLocked(item->child(i)))
      return true;

  return false;
}


QJsonObject McpTools::applyExportBranch(KnowTreeModel *model,
                                        const QString &branchId,
                                        const QString &path,
                                        QString &error)
{
  QJsonObject result;

  TreeItem *branch=model->getItemById(branchId);

  if(branch==nullptr)
  {
    error="Unknown branch_id: "+branchId;

    return result;
  }

  // Шифр без ключа: выгрузка попыталась бы расшифровать пустым ключом
  // и отдала бы мусор. Ключа в серверном процессе не бывает
  if(subtreeHasLocked(branch))
  {
    error="Branch contains locked notes, export refused";

    return result;
  }

  // Приемник: каталог создается заранее, иначе внутри выгрузки
  // модальный диалог о незаписываемом файле
  if(!QDir().mkpath(path))
  {
    error="Can not create export directory: "+path;

    return result;
  }

  QFileInfo probe(path);

  if(!probe.isDir() || !probe.isWritable())
  {
    error="Export directory is not writable: "+path;

    return result;
  }

  const int notes=branch->subtreeRecordCount();

  if(!model->exportBranchToDirectory(branch, path))
  {
    error="Export failed, see server log";

    return result;
  }

  QFileInfo xml(path+"/mytetra.xml");

  if(!xml.isReadable())
  {
    error="Export failed, result is missing mytetra.xml";

    return result;
  }

  result["path"]=QDir::cleanPath(path);
  result["notes"]=notes;

  return result;
}


// Одиночное имя без разделителей, точек-вверх и диска:
// то же правило что isSafeImportFileName внутри модели
static bool isSingleFileName(const QString &name)
{
  if(name.isEmpty() || name==QStringLiteral(".") || name==QStringLiteral(".."))
    return false;

  if(name.contains('/') || name.contains('\\') || name.contains(':'))
    return false;

  return true;
}


QString McpTools::checkImportDir(const QString &path)
{
  QFileInfo dirInfo(path);

  if(!dirInfo.isDir() || !dirInfo.isReadable())
    return "Import directory is not readable: "+path;

  const QString xmlPath=path+"/mytetra.xml";
  QFile xmlFile(xmlPath);

  if(!xmlFile.open(QIODevice::ReadOnly))
    return "Import error: unable to read the XML file "+xmlPath+".";

  const QByteArray xmlData=xmlFile.readAll();
  xmlFile.close();

  QDomDocument doc;
  QString errorText;
  int errorLine=0;
  int errorColumn=0;

  if(!doc.setContent(xmlData, true, &errorText, &errorLine, &errorColumn))
    return "Import error: unable to parse the XML file "+xmlPath+".";

  QDomElement format=doc.documentElement().firstChildElement("format");
  const int version=format.attribute("version", "0").toInt();
  const int subversion=format.attribute("subversion", "0").toInt();

  if(version>CURRENT_FORMAT_VERSION || subversion>CURRENT_FORMAT_SUBVERSION)
    return "Import error: unsupported database format.";

  // Имена из чужого обменника проверяются тем же правилом что при
  // ввозе: иначе внутри вылезет модальный диалог вместо ошибки
  QDomNodeList records=doc.elementsByTagName("record");

  for(int i=0; i<records.count(); ++i)
  {
    QDomElement record=records.at(i).toElement();
    const QString dir=record.attribute("dir");
    const QString file=record.attribute("file");

    if(dir.isEmpty() || !isSingleFileName(dir))
      return "Import error: incorrect record directory name \""+dir+"\".";

    if(file.isEmpty() || !isSingleFileName(file))
      return "Import error: incorrect record file name \""+file+"\".";

    if(!QFileInfo(path+"/base/"+dir).isDir())
      return "Import error: missing record directory \""+dir+"\".";

    if(!QFileInfo(path+"/base/"+dir+"/"+file).isReadable())
      return "Import error: unable to read record file \""+file+"\".";
  }

  return QString();
}


QJsonObject McpTools::applyImportBranch(KnowTreeModel *model,
                                        const QString &parentId,
                                        const QString &path,
                                        QString &error)
{
  QJsonObject result;

  if(!parentId.isEmpty())
  {
    TreeItem *parent=model->getItemById(parentId);

    if(parent==nullptr)
    {
      error="Unknown parent branch_id: "+parentId;

      return result;
    }

    if(parent->getField("crypt")=="1")
    {
      error="Parent branch is encrypted, import refused";

      return result;
    }
  }

  const QString problem=checkImportDir(path);

  if(!problem.isEmpty())
  {
    error=problem;

    return result;
  }

  // Ввоз сам сохраняет дерево в конце; пустой итог - отказ
  // (диалоги внутри уже обезврежены проверкой выше)
  TreeItem *parentItem=modelRoot(model);

  if(!parentId.isEmpty())
    parentItem=model->getItemById(parentId);

  const QString newId=model->importBranchFromDirectory(parentItem, path);

  if(newId.isEmpty())
  {
    error="Import failed, see server log";

    return result;
  }

  result["id"]=newId;
  result["parent_id"]=parentId;

  return result;
}


QJsonObject McpTools::importBranchDirect(const QString &parentId,
                                         const QString &path,
                                         QString &error)
{
  // importBranchFromDirectory сохраняет сам, повторный save не нужен
  return applyImportBranch(m_model, parentId, path, error);
}


bool McpTools::applyBatchOp(KnowTreeModel *model,
                                const QString &tool,
                                const QJsonObject &args,
                                QJsonObject &result,
                                QString &error)
{
  result=QJsonObject();

  if(tool==QStringLiteral("create_branch"))
  {
    result=applyCreateBranch(model,
                             args.value("parent_id").toString(),
                             args.value("id").toString(),
                             args.value("name").toString(),
                             error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("create_note"))
  {
    result=applyCreateNote(model,
                           args.value("branch_id").toString(),
                           args.value("id").toString(),
                           args.value("name").toString(),
                           args.value("html").toString(),
                           args.value("tags").toString(),
                           error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("update_note"))
  {
    QStringList updatedFields;

    if(!applyUpdateNote(model,
                        args.value("note_id").toString(),
                        args.value("has_name").toBool(false),
                        args.value("name").toString(),
                        args.value("has_html").toBool(false),
                        args.value("html").toString(),
                        args.value("has_tags").toBool(false),
                        args.value("tags").toString(),
                        error,
                        updatedFields))
      return false;

    result["id"]=args.value("note_id").toString();
    result["updated_fields"]=QJsonArray::fromStringList(updatedFields);

    return true;
  }

  if(tool==QStringLiteral("delete_note"))
  {
    if(!applyDeleteNote(model,
                        args.value("note_id").toString(),
                        error))
      return false;

    result["id"]=args.value("note_id").toString();
    result["deleted"]=true;

    return true;
  }

  if(tool==QStringLiteral("delete_branch"))
  {
    result=applyDeleteBranch(model,
                             args.value("branch_id").toString(),
                             error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("rename_branch"))
  {
    result=applyRenameBranch(model,
                             args.value("branch_id").toString(),
                             args.value("name").toString(),
                             error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("move_branch"))
  {
    result=applyMoveBranch(model,
                           args.value("branch_id").toString(),
                           args.value("target_id").toString(),
                           args.value("as_child").toBool(true),
                           error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("move_note"))
  {
    result=applyMoveNote(model,
                         args.value("note_id").toString(),
                         args.value("branch_id").toString(),
                         error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("favorite_note"))
  {
    result=applyFavoriteNote(model,
                             args.value("note_id").toString(),
                             true,
                             error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("unfavorite_note"))
  {
    result=applyFavoriteNote(model,
                             args.value("note_id").toString(),
                             false,
                             error);
    return error.isEmpty() && !result.isEmpty();
  }

  if(tool==QStringLiteral("rename_tag"))
  {
    int renamed=0;
    int skipped=0;

    if(!applyRenameTag(model,
                       args.value("old").toString(),
                       args.value("new").toString(),
                       error, renamed, skipped))
      return false;

    result["renamed_tag"]=args.value("old").toString().trimmed();
    result["renamed"]=renamed;
    result["skipped"]=skipped;

    return true;
  }

  if(tool==QStringLiteral("delete_tag"))
  {
    int cleared=0;
    int skipped=0;

    if(!applyDeleteTag(model,
                       args.value("tag").toString(),
                       error, cleared, skipped))
      return false;

    result["deleted_tag"]=args.value("tag").toString().trimmed();
    result["cleared"]=cleared;
    result["skipped"]=skipped;

    return true;
  }

  error="Unsupported in batch: "+tool;

  return false;
}


QJsonObject McpTools::debugInfo(void)
{
  QJsonObject result;

  result["app"]=GlobalParameters::getApplicationName();
  result["version"]=QString::number(APPLICATION_RELEASE_VERSION)+"."+
                    QString::number(APPLICATION_RELEASE_SUBVERSION)+"."+
                    QString::number(APPLICATION_RELEASE_MICROVERSION);

  const QString tetradir=mytetraConfig.get_tetradir();
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

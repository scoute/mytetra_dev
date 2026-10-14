#include <QModelIndex>
#include <QJsonArray>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>

#include "McpTools.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "models/recordTable/Record.h"
#include "models/appConfig/AppConfig.h"

extern AppConfig mytetraConfig;

// Максимальная длина текста в ответе get_note, остальное режется
// с флагом truncated
static const int MCP_MAX_TEXT_LENGTH=200000;

// Контекст сниппета поиска с каждой стороны от совпадения
static const int MCP_SNIPPET_CONTEXT=120;


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

  return note;
}


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

    QJsonObject branch;
    branch["id"]=id;
    branch["name"]=item->getField("name");
    branch["parent_id"]=parentId;
    out.append(branch);

    appendBranches(index, id, out);
  }
}


QJsonObject McpTools::listBranches(void)
{
  QJsonArray branches;
  appendBranches(QModelIndex(), QString(), branches);

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


QJsonObject McpTools::listNotes(const QString &branchId, bool recursive, QString &error)
{
  QJsonObject result;

  TreeItem *start=m_model->getItemById(branchId);

  if(start==nullptr)
  {
    error="Unknown branch_id: "+branchId;

    return result;
  }

  QJsonArray notes;
  collectBranchNotes(start, recursive, notes);
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


QJsonObject McpTools::getNote(const QString &noteId, QString &error)
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

  if(plain.length()>MCP_MAX_TEXT_LENGTH)
  {
    plain=plain.left(MCP_MAX_TEXT_LENGTH);
    truncated=true;
  }

  note["text"]=plain;
  note["truncated"]=truncated;

  result["note"]=note;

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


void McpTools::searchBranch(TreeItem *item, const QString &queryLower, int limit, QJsonArray &out)
{
  if(out.size()>=limit)
    return;

  QString branchId=item->getField("id");
  QString path=branchPathOf(item);

  RecordTableData *table=item->recordtableGetTableData();

  if(table!=nullptr)
    for(unsigned i=0; i<table->size() && out.size()<limit; ++i)
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

        out.append(hit);
      }
    }

  for(int i=0; i<item->childCount() && out.size()<limit; ++i)
    searchBranch(item->child(i), queryLower, limit, out);
}


void McpTools::searchModel(const QModelIndex &parent, const QString &queryLower, int limit, QJsonArray &out)
{
  int rows=m_model->rowCount(parent);

  for(int r=0; r<rows && out.size()<limit; ++r)
  {
    QModelIndex index=m_model->index(r, 0, parent);
    TreeItem *item=m_model->getItem(index);

    if(item==nullptr)
      continue;

    searchBranch(item, queryLower, limit, out);
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
  searchModel(QModelIndex(), trimmed.toLower(), limit, hits);
  result["results"]=hits;

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

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
#include "models/recordTable/Record.h"
#include "models/appConfig/AppConfig.h"

#include <QFile>

// Контекст сниппета поиска с каждой стороны от совпадения
static const int MCP_SNIPPET_CONTEXT=100;

#include "McpToolsPrivate.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/appConfig/AppConfigStorage.h"

// Сайдкар избранного напрямую файлом: синглетон Favorites в
// долгоживущем серверном процессе кэширует и протухает, а в
// headless за деревом вообще не ходит. Формат тот же:
// <favorites version="1"><note id="..."/>...</favorites>
static QString favoritesSidecarPath(void)
{
  return QDir::cleanPath(AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/favorites.xml");
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

  if(AppContext::instance().appConfig()->uiSection()->get_favoritesStorage()==QStringLiteral("xml"))
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

  if(AppContext::instance().appConfig()->uiSection()->get_favoritesStorage()==QStringLiteral("xml"))
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
  if(AppContext::instance().appConfig()->uiSection()->get_favoritesStorage()==QStringLiteral("xml"))
    m_model->save();

  return result;
}

QJsonObject McpTools::unfavoriteNoteDirect(const QString &noteId, QString &error)
{
  QJsonObject result=applyFavoriteNote(m_model, noteId, false, error);

  if(!error.isEmpty())
    return result;

  if(AppContext::instance().appConfig()->uiSection()->get_favoritesStorage()==QStringLiteral("xml"))
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

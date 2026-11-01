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
#include "models/appConfig/AppConfigStorage.h"

// Корзина готова принять файлы: есть и пишемся. Иначе удаление
// уперлось бы в criticalError с диалогом внутри, а в headless
// это падение процесса. Создается при отсутствии как при установке
static bool trashReady(QString &error)
{
  QString trashDir=AppContext::instance().appConfig()->storageSection()->get_trashdir();

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
                               BacklinkIndex *backlinks,
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
  QString sidecar=QDir::cleanPath(AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/backlinks.xml");

  if(QFile::exists(sidecar))
    backlinks->removeSources(QStringList() << noteId);

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

  if(!applyDeleteNote(m_model, m_backlinks, noteId, error))
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

bool McpTools::applyBatchOp(KnowTreeModel *model,
                                BacklinkIndex *backlinks,
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
                        backlinks,
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

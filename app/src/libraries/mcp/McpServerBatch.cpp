// MCP-сервер MyTetra: JSON-RPC 2.0 поверх stdio (NDJSON).
// Только чтение базы. Второй процесс рядом с запущенной MyTetra
// безопасен: писатель один (GUI), читатель видит сохраненное
// состояние, структура подтягивается по mtime mytetra.xml

#include <QCoreApplication>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QLocalSocket>
#include <QDataStream>
#include <QLibrary>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <unistd.h>
#endif
#include <cstdio>

#include "McpTools.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"

// Версии протокола MCP которые понимает сервер.
// Запрошенная клиентом поддерживаемая версия эхомится назад,
// незнакомая сводится к самой старой стабильной

#include "McpServerPrivate.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/appConfig/AppConfigRecordTable.h"

QJsonObject McpSession::writeRenameTag(const QString &oldTag,
                                       const QString &newTag,
                                       QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(McpTools::countTagNotes(&m_model, oldTag.trimmed().toLower())==0)
  {
    toolError="Unknown tag: "+oldTag.trimmed();

    return result;
  }

  QJsonObject payload;
  payload["old"]=oldTag;
  payload["new"]=newTag;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpRenameTag", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  // GUI отказал с причиной (валидация, read-only): текст отказа уходит
  // агенту как есть, повтор того же вызова бессмысленен
  if(state==GUI_REFUSED)
  {
    toolError=sendError;

    return result;
  }

  // APPLIED: GUI уже применил и сохранил, модель перечитывается
  // и сверяется (несоответствие здесь - внутренняя ошибка,
  // а не повод для ретрая). DELIVERED: старый GUI без ответа-итога,
  // сейв ждётся по mtime как раньше
  const bool synced=(state==GUI_APPLIED);

  if(state==GUI_DELIVERED || synced)
  {
    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    if(McpTools::countTagNotes(&m_model, oldTag.trimmed().toLower())!=0)
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["renamed_tag"]=oldTag.trimmed();
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject renamed=m_tools.renameTagDirect(oldTag, newTag, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=renamed;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

QJsonObject McpSession::writeDeleteTag(const QString &tag, QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(McpTools::countTagNotes(&m_model, tag.trimmed().toLower())==0)
  {
    toolError="Unknown tag: "+tag.trimmed();

    return result;
  }

  QJsonObject payload;
  payload["tag"]=tag;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpDeleteTag", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  // GUI отказал с причиной (валидация, read-only): текст отказа уходит
  // агенту как есть, повтор того же вызова бессмысленен
  if(state==GUI_REFUSED)
  {
    toolError=sendError;

    return result;
  }

  // APPLIED: GUI уже применил и сохранил, модель перечитывается
  // и сверяется (несоответствие здесь - внутренняя ошибка,
  // а не повод для ретрая). DELIVERED: старый GUI без ответа-итога,
  // сейв ждётся по mtime как раньше
  const bool synced=(state==GUI_APPLIED);

  if(state==GUI_DELIVERED || synced)
  {
    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    if(McpTools::countTagNotes(&m_model, tag.trimmed().toLower())!=0)
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["deleted_tag"]=tag.trimmed();
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject deleted=m_tools.deleteTagDirect(tag, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=deleted;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

bool McpSession::waitGuiSave(int timeoutMs)
{
  QElapsedTimer timer;
  timer.start();

  while(timer.elapsed()<timeoutMs)
  {
    if(QFileInfo(m_xmlPath).lastModified()!=m_xmlMtime)
      return true;

    QThread::msleep(100);
  }

  return QFileInfo(m_xmlPath).lastModified()!=m_xmlMtime;
}

void McpSession::resyncModel(void)
{
  m_model.reload();
  m_xmlMtime=QFileInfo(m_xmlPath).lastModified();
}

QString McpSession::freshBranchId(void)
{
  QString id;

  do { id=getUniqueId(); } while(m_model.isItemIdExists(id));

  return id;
}

QString McpSession::freshNoteId(void)
{
  QString id;

  do { id=getUniqueId(); } while(m_model.isRecordIdExists(id));

  return id;
}

QJsonObject McpSession::writeBranch(const QString &id,
                                    const QString &parentId,
                                    const QString &name,
                                    QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  // Предпроверки по свежему снапшоту: мгновенные ошибки без
  // ложного сообщения живому GUI (там проверки дублируются)
  if(name.trimmed().isEmpty())
  {
    toolError="Empty branch name";

    return result;
  }

  if(!parentId.isEmpty())
  {
    TreeItem *parent=m_model.getItemById(parentId);

    if(parent==nullptr)
    {
      toolError="Unknown parent branch_id: "+parentId;

      return result;
    }

    if(parent->getField("crypt")=="1")
    {
      toolError="Parent branch is encrypted, creation refused";

      return result;
    }
  }

  QJsonObject payload;
  payload["id"]=id;
  payload["parent_id"]=parentId;
  payload["name"]=name;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpCreateBranch", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  // GUI отказал с причиной (валидация, read-only): текст отказа уходит
  // агенту как есть, повтор того же вызова бессмысленен
  if(state==GUI_REFUSED)
  {
    toolError=sendError;

    return result;
  }

  // APPLIED: GUI уже применил и сохранил, модель перечитывается
  // и сверяется (несоответствие здесь - внутренняя ошибка,
  // а не повод для ретрая). DELIVERED: старый GUI без ответа-итога,
  // сейв ждётся по mtime как раньше
  const bool synced=(state==GUI_APPLIED);

  if(state==GUI_DELIVERED || synced)
  {
    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    if(!m_model.isItemIdExists(id))
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["id"]=id;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject created=m_tools.createBranchDirect(parentId, id, name, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=created;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

QJsonObject McpSession::writeNote(const QString &id,
                                  const QString &branchId,
                                  const QString &name,
                                  const QString &html,
                                  const QString &tags,
                                  QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(name.trimmed().isEmpty())
  {
    toolError="Empty note name";

    return result;
  }

  {
    TreeItem *branch=m_model.getItemById(branchId);

    if(branch==nullptr)
    {
      toolError="Unknown branch_id: "+branchId;

      return result;
    }

    if(branch->getField("crypt")=="1")
    {
      toolError="Branch is encrypted, creation refused";

      return result;
    }
  }

  QJsonObject payload;
  payload["id"]=id;
  payload["branch_id"]=branchId;
  payload["name"]=name;
  payload["html"]=html;
  payload["tags"]=tags;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpCreateNote", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  // GUI отказал с причиной (валидация, read-only): текст отказа уходит
  // агенту как есть, повтор того же вызова бессмысленен
  if(state==GUI_REFUSED)
  {
    toolError=sendError;

    return result;
  }

  // APPLIED: GUI уже применил и сохранил, модель перечитывается
  // и сверяется (несоответствие здесь - внутренняя ошибка,
  // а не повод для ретрая). DELIVERED: старый GUI без ответа-итога,
  // сейв ждётся по mtime как раньше
  const bool synced=(state==GUI_APPLIED);

  if(state==GUI_DELIVERED || synced)
  {
    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    if(!m_model.isRecordIdExists(id))
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["id"]=id;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject created=m_tools.createNoteDirect(branchId, id, name, html, tags, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=created;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

QJsonObject McpSession::writeNoteUpdate(const QString &id,
                                        bool hasName, const QString &name,
                                        bool hasHtml, const QString &html,
                                        bool hasTags, const QString &tags,
                                        QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(!hasName && !hasHtml && !hasTags)
  {
    toolError="Nothing to update: provide name, text or tags";

    return result;
  }

  if(!m_model.isRecordIdExists(id))
  {
    toolError="Unknown note_id: "+id;

    return result;
  }

  {
    Record *record=m_model.getRecord(id);

    if(record==nullptr)
    {
      toolError="Unknown note_id: "+id;

      return result;
    }

    if(record->getField("crypt")=="1")
    {
      toolError="Note is encrypted, update refused";

      return result;
    }
  }

  if(hasName && name.trimmed().isEmpty())
  {
    toolError="Empty note name";

    return result;
  }

  QJsonObject payload;
  payload["id"]=id;
  payload["has_name"]=hasName;
  payload["name"]=name;
  payload["has_html"]=hasHtml;
  payload["html"]=html;
  payload["has_tags"]=hasTags;
  payload["tags"]=tags;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpUpdateNote", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  // GUI отказал с причиной (валидация, read-only): текст отказа уходит
  // агенту как есть, повтор того же вызова бессмысленен
  if(state==GUI_REFUSED)
  {
    toolError=sendError;

    return result;
  }

  // APPLIED: GUI уже применил и сохранил, модель перечитывается
  // и сверяется (несоответствие здесь - внутренняя ошибка,
  // а не повод для ретрая). DELIVERED: старый GUI без ответа-итога,
  // сейв ждётся по mtime как раньше
  const bool synced=(state==GUI_APPLIED);

  if(state==GUI_DELIVERED || synced)
  {
    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    Record *record=m_model.isRecordIdExists(id) ? m_model.getRecord(id) : nullptr;

    bool ok=(record!=nullptr);

    if(ok && hasName)
      ok=(record->getField("name")==name.trimmed());

    if(ok && hasTags)
      ok=(record->getField("tags")==tags);

    if(!ok)
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["id"]=id;
    result["applied_via"]="gui";
    result["verified"]=true;

    if(hasName)
      result["updated_name"]=true;

    if(hasHtml)
      result["updated_text"]=true;

    if(hasTags)
      result["updated_tags"]=true;

    return result;
  }

  QString error;
  QJsonObject updated=m_tools.updateNoteDirect(id,
                                                hasName, name,
                                                hasHtml, html,
                                                hasTags, tags,
                                                error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=updated;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

QJsonObject McpSession::writeDeleteNote(const QString &id, QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(!m_model.isRecordIdExists(id))
  {
    toolError="Unknown note_id: "+id;

    return result;
  }

  QJsonObject payload;
  payload["id"]=id;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpDeleteNote", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  // GUI отказал с причиной (валидация, read-only): текст отказа уходит
  // агенту как есть, повтор того же вызова бессмысленен
  if(state==GUI_REFUSED)
  {
    toolError=sendError;

    return result;
  }

  // APPLIED: GUI уже применил и сохранил, модель перечитывается
  // и сверяется (несоответствие здесь - внутренняя ошибка,
  // а не повод для ретрая). DELIVERED: старый GUI без ответа-итога,
  // сейв ждётся по mtime как раньше
  const bool synced=(state==GUI_APPLIED);

  if(state==GUI_DELIVERED || synced)
  {
    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    if(m_model.isRecordIdExists(id))
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["id"]=id;
    result["deleted"]=true;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject deleted=m_tools.deleteNoteDirect(id, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=deleted;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

QJsonObject McpSession::writeDeleteBranch(const QString &id, QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(m_model.getItemById(id)==nullptr)
  {
    toolError="Unknown branch_id: "+id;

    return result;
  }

  QJsonObject payload;
  payload["id"]=id;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpDeleteBranch", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  // GUI отказал с причиной (валидация, read-only): текст отказа уходит
  // агенту как есть, повтор того же вызова бессмысленен
  if(state==GUI_REFUSED)
  {
    toolError=sendError;

    return result;
  }

  // APPLIED: GUI уже применил и сохранил, модель перечитывается
  // и сверяется (несоответствие здесь - внутренняя ошибка,
  // а не повод для ретрая). DELIVERED: старый GUI без ответа-итога,
  // сейв ждётся по mtime как раньше
  const bool synced=(state==GUI_APPLIED);

  if(state==GUI_DELIVERED || synced)
  {
    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    if(m_model.getItemById(id)!=nullptr)
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["id"]=id;
    result["deleted"]=true;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject deleted=m_tools.deleteBranchDirect(id, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=deleted;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

// Проверка одной пакетной операции по свежему состоянию модели.
// Семантика по состоянию, а не по факту исполнения: ok значит
// конечное состояние совпало с желаемым

// Проверка одной пакетной операции по свежему состоянию модели.
// Семантика по состоянию, а не по факту исполнения: ok значит
// конечное состояние совпало с желаемым
static bool verifyBatchOp(KnowTreeModel &model,
                          const QString &tool,
                          const QJsonObject &args,
                          QJsonObject &detail)
{
  detail=QJsonObject();

  if(tool==QStringLiteral("create_branch"))
  {
    TreeItem *item=model.getItemById(args.value("id").toString());
    const bool ok=(item!=nullptr &&
                   item->getField("name")==args.value("name").toString().trimmed());

    if(ok)
      detail["id"]=args.value("id").toString();

    return ok;
  }

  if(tool==QStringLiteral("create_note"))
  {
    Record *record=model.isRecordIdExists(args.value("id").toString()) ?
                   model.getRecord(args.value("id").toString()) : nullptr;
    const bool ok=(record!=nullptr &&
                   record->getField("name")==args.value("name").toString().trimmed());

    if(ok)
      detail["id"]=args.value("id").toString();

    return ok;
  }

  if(tool==QStringLiteral("update_note"))
  {
    Record *record=model.isRecordIdExists(args.value("note_id").toString()) ?
                   model.getRecord(args.value("note_id").toString()) : nullptr;
    bool ok=(record!=nullptr);

    if(ok && args.value("has_name").toBool(false))
      ok=(record->getField("name")==args.value("name").toString().trimmed());

    if(ok && args.value("has_tags").toBool(false))
      ok=(record->getField("tags")==args.value("tags").toString());

    if(ok)
      detail["id"]=args.value("note_id").toString();

    return ok;
  }

  if(tool==QStringLiteral("delete_note"))
  {
    const bool ok=!model.isRecordIdExists(args.value("note_id").toString());

    if(ok)
      detail["id"]=args.value("note_id").toString();

    return ok;
  }

  if(tool==QStringLiteral("delete_branch"))
  {
    const bool ok=(model.getItemById(args.value("branch_id").toString())==nullptr);

    if(ok)
      detail["id"]=args.value("branch_id").toString();

    return ok;
  }

  if(tool==QStringLiteral("rename_branch"))
  {
    TreeItem *item=model.getItemById(args.value("branch_id").toString());
    const bool ok=(item!=nullptr &&
                   item->getField("name")==args.value("name").toString().trimmed());

    if(ok)
      detail["id"]=args.value("branch_id").toString();

    return ok;
  }

  if(tool==QStringLiteral("move_branch"))
  {
    TreeItem *item=model.getItemById(args.value("branch_id").toString());

    if(item==nullptr)
      return false;

    QStringList path=item->getPath();
    const QString actual=(path.size()>=2) ? path.at(path.size()-2) : QString();
    const QString expect=expectedMoveParent(model,
                                            args.value("target_id").toString(),
                                            args.value("as_child").toBool(true));
    const bool ok=(actual==expect);

    if(ok)
      detail["id"]=args.value("branch_id").toString();

    return ok;
  }

  if(tool==QStringLiteral("move_note"))
  {
    QStringList path=model.getRecordPath(args.value("note_id").toString());
    const QString actual=path.isEmpty() ? QString() : path.last();
    const bool ok=(!path.isEmpty() && actual==args.value("branch_id").toString());

    if(ok)
      detail["id"]=args.value("note_id").toString();

    return ok;
  }

  if(tool==QStringLiteral("favorite_note") || tool==QStringLiteral("unfavorite_note"))
  {
    const bool starred=(tool==QStringLiteral("favorite_note"));
    const QString id=args.value("note_id").toString();
    bool ok=false;

    if(AppContext::instance().appConfig()->uiSection()->get_favoritesStorage()==QStringLiteral("xml"))
    {
      Record *record=model.isRecordIdExists(id) ? model.getRecord(id) : nullptr;
      ok=(record!=nullptr && (record->getField("favorite")=="1")==starred);
    }
    else
      ok=(McpTools::sidecarFavoriteIds().contains(id)==starred);

    if(ok)
      detail["id"]=id;

    return ok;
  }

  if(tool==QStringLiteral("rename_tag"))
  {
    const bool ok=(McpTools::countTagNotes(&model,
        args.value("old").toString().trimmed().toLower())==0);

    if(ok)
      detail["renamed_tag"]=args.value("old").toString().trimmed();

    return ok;
  }

  if(tool==QStringLiteral("delete_tag"))
  {
    const bool ok=(McpTools::countTagNotes(&model,
        args.value("tag").toString().trimmed().toLower())==0);

    if(ok)
      detail["deleted_tag"]=args.value("tag").toString().trimmed();

    return ok;
  }

  return false;
}

static const int MCP_BATCH_MAX_OPS=200;

static const QSet<QString> &mcpBatchableTools(void)
{
  static const QSet<QString> batchable=QSet<QString>()
      << QStringLiteral("create_branch") << QStringLiteral("create_note")
      << QStringLiteral("update_note")
      << QStringLiteral("delete_note") << QStringLiteral("delete_branch")
      << QStringLiteral("rename_branch") << QStringLiteral("move_branch")
      << QStringLiteral("move_note")
      << QStringLiteral("favorite_note") << QStringLiteral("unfavorite_note")
      << QStringLiteral("rename_tag") << QStringLiteral("delete_tag");

  return batchable;
}

QJsonObject McpSession::writeApplyBatch(const QJsonArray &operations,
                                        QString &toolError)
{
  QJsonObject result;

  if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(operations.isEmpty())
  {
    toolError="Empty operations";

    return result;
  }

  if(operations.size()>MCP_BATCH_MAX_OPS)
  {
    toolError="Too many operations (max 200)";

    return result;
  }

  struct BatchOp
  {
    QString tool;
    QJsonObject args;
  };

  QList<BatchOp> normalized;
  QSet<QString> batchIds;

  // Нормализация без касания модели: свежие id, html из text/format,
  // флаги has_*. Структурные отказы валят весь пакет до исполнения
  for(const QJsonValue &opValue : operations)
  {
    if(!opValue.isObject())
    {
      toolError="Each operation must be {tool, arguments}";

      return result;
    }

    const QJsonObject op=opValue.toObject();
    const QString tool=op.value("tool").toString();
    const QJsonObject opArgs=op.value("arguments").toObject();

    if(!mcpBatchableTools().contains(tool))
    {
      toolError="Unsupported in batch: "+tool;

      return result;
    }

    if((tool==QStringLiteral("delete_note") ||
        tool==QStringLiteral("delete_branch") ||
        tool==QStringLiteral("delete_tag")) &&
       m_mode!=McpMode::ReadWriteDelete)
    {
      toolError="Batch contains delete, need delete mode";

      return result;
    }

    QJsonObject args=opArgs;

    if(tool==QStringLiteral("create_branch") || tool==QStringLiteral("create_note"))
    {
      QString id;

      if(tool==QStringLiteral("create_branch"))
        do { id=freshBranchId(); } while(batchIds.contains(id));
      else
        do { id=freshNoteId(); } while(batchIds.contains(id));

      batchIds.insert(id);
      args["id"]=id;
    }

    if(tool==QStringLiteral("create_note") || tool==QStringLiteral("update_note"))
    {
      const bool hasText=args.contains("text");
      QString format=args.value("format").toString();

      if(format.isEmpty())
        format="text";

      if(hasText && format!="text" && format!="html")
      {
        toolError="Bad format, use text or html";

        return result;
      }

      if(tool==QStringLiteral("create_note"))
      {
        const QString text=args.value("text").toString();
        args["html"]=(format=="html") ? text : McpTools::plainToHtml(text);
      }
      else
      {
        args["has_name"]=args.contains("name");
        args["has_html"]=hasText;
        args["has_tags"]=args.contains("tags");

        const QString text=args.value("text").toString();
        args["html"]=(format=="html") ? text : McpTools::plainToHtml(text);
      }
    }

    normalized.append({tool, args});
  }

  // Применение: один вызов GUI либо прямой цикл, сейв один раз.
  // Проверяются эффективные аргументы: headless-вставка записей
  // заменяет id на свежий, фактический возвращается в результате
  // Эффективные аргументы примененных операций (по индексу
  // в normalized): фактические id после headless-замен
  QList<QJsonObject> effectiveArgs;
  bool needSave=false;
  bool delivered=false;

  {
    QJsonObject payload;
    QJsonArray normOps;

    for(const BatchOp &op : normalized)
    {
      QJsonObject item;
      item["tool"]=op.tool;
      item["arguments"]=op.args;
      normOps.append(item);
    }

    payload["operations"]=normOps;

    QString sendError;
    GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpApplyBatch", payload, sendError);

    if(state==GUI_FAILED)
    {
      toolError=sendError;

      return result;
    }

    if(state==GUI_REFUSED)
    {
      toolError=sendError;

      return result;
    }

    if(state==GUI_DELIVERED || state==GUI_APPLIED)
    {
      delivered=true;

      // APPLIED: GUI уже применил и сохранил, сейв по mtime не ждём
      if(state==GUI_DELIVERED)
        waitGuiSave(5000);

      resyncModel();
    }
  }

  if(delivered)
  {
    for(const BatchOp &op : normalized)
      effectiveArgs.append(op.args);
  }
  else
  {
    for(const BatchOp &op : normalized)
    {
      QString error;
      QJsonObject applyResult;

      if(!McpTools::applyBatchOp(&m_model, m_backlinks, op.tool, op.args, applyResult, error))
        break;

      QJsonObject effArgs=op.args;

      if(applyResult.contains("id"))
        effArgs["id"]=applyResult.value("id").toString();

      effectiveArgs.append(effArgs);
      needSave=true;
    }

    if(needSave)
      m_model.save();
  }

  // Проверка по состоянию на свежей модели, по порядку, со стопом
  // на первой несверенной: дальше не исполнялось
  QJsonArray opResults;
  int applied=0;
  bool stopped=false;

  for(int i=0; i<normalized.size(); ++i)
  {
    const BatchOp &op=normalized.at(i);
    QJsonObject entry;
    entry["tool"]=op.tool;

    if(stopped)
    {
      entry["ok"]=false;
      entry["skipped"]=true;
      opResults.append(entry);
      continue;
    }

    QJsonObject detail;
    const bool ok=(i<effectiveArgs.size()) &&
                  verifyBatchOp(m_model, op.tool, effectiveArgs.at(i), detail);

    entry["ok"]=ok;

    for(auto it=detail.begin(); it!=detail.end(); ++it)
      entry[it.key()]=it.value();

    if(ok)
      ++applied;
    else
      stopped=true;

    opResults.append(entry);
  }

  result["operations"]=opResults;
  result["applied"]=applied;
  result["total"]=operations.size();
  result["applied_via"]=delivered ? QStringLiteral("gui") : QStringLiteral("direct");
  result["verified"]=true;

  return result;
}

// Pre-валидация базы до initFromXML: в headless-режиме диалоги
// XmlTree::load и criticalError формата роняют процесс,
// поэтому файл проверяется вручную с чистым выходом в stderr

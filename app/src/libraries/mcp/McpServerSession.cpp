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

#include "McpServer.h"
#include "McpTools.h"
#include "main.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/RandomInitter.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"

extern FixedParameters fixedParameters;
extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;
extern DataBaseConfig dataBaseConfig;


// Версии протокола MCP которые понимает сервер.
// Запрошенная клиентом поддерживаемая версия эхомится назад,
// незнакомая сводится к самой старой стабильной

#include "McpServerPrivate.h"


// Дети ветки-родителя (пустой parentId - корень). Только id строками:
// указатели модели после resync инвалидируются и не хранятся
static QSet<QString> branchChildIds(KnowTreeModel &model, const QString &parentId)
{
  QSet<QString> out;

  const TreeItem *parent=nullptr;

  if(parentId.isEmpty())
    parent=model.getRootItem();
  else
    parent=model.getItemById(parentId);

  if(parent==nullptr)
    return out;

  for(int i=0; i<parent->childCount(); ++i)
  {
    TreeItem *childItem=parent->child(i);

    if(childItem!=nullptr)
      out.insert(childItem->getField("id"));
  }

  return out;
}


// Имя верхнего узла обменника: по нему после ввоза находится
// новая ветка в diff детей родителя


// Имя верхнего узла обменника: по нему после ввоза находится
// новая ветка в diff детей родителя
static QString importTopNodeName(const QString &path)
{
  QFile xmlFile(path+"/mytetra.xml");

  if(!xmlFile.open(QIODevice::ReadOnly))
    return QString();

  QDomDocument doc;

  if(!doc.setContent(&xmlFile, true))
    return QString();

  QDomElement content=doc.documentElement().firstChildElement("content");

  return content.firstChildElement("node").attribute("name");
}


// Ожидаемый родитель после move_branch: внутрь цели или соседство


// Ожидаемый родитель после move_branch: внутрь цели или соседство
QString expectedMoveParent(KnowTreeModel &model,
                                  const QString &targetId,
                                  bool asChild)
{
  if(asChild)
    return targetId;

  TreeItem *target=model.getItemById(targetId);

  if(target==nullptr)
    return QString();

  QStringList path=target->getPath();

  if(path.size()>=2)
    return path.at(path.size()-2);

  return QString();
}


QJsonObject McpSession::writeRenameBranch(const QString &id,
                                          const QString &name,
                                          QString &toolError)
{
  QJsonObject result;

  if(mytetraConfig.get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(name.trimmed().isEmpty())
  {
    toolError="Empty branch name";

    return result;
  }

  if(m_model.getItemById(id)==nullptr)
  {
    toolError="Unknown branch_id: "+id;

    return result;
  }

  QJsonObject payload;
  payload["id"]=id;
  payload["name"]=name;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpRenameBranch", payload, sendError);

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

    TreeItem *item=m_model.getItemById(id);

    if(item==nullptr || item->getField("name")!=name.trimmed())
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
  QJsonObject renamed=m_tools.renameBranchDirect(id, name, error);

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


QJsonObject McpSession::writeMoveBranch(const QString &id,
                                        const QString &targetId,
                                        bool asChild,
                                        QString &toolError)
{
  QJsonObject result;

  if(mytetraConfig.get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(m_model.getItemById(id)==nullptr)
  {
    toolError="Unknown branch_id: "+id;

    return result;
  }

  if(m_model.getItemById(targetId)==nullptr)
  {
    toolError="Unknown target branch_id: "+targetId;

    return result;
  }

  QJsonObject payload;
  payload["id"]=id;
  payload["target_id"]=targetId;
  payload["as_child"]=asChild;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpMoveBranch", payload, sendError);

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
    QString expect=expectedMoveParent(m_model, targetId, asChild);

    if(!synced)
      waitGuiSave(5000);
    resyncModel();

    TreeItem *moved=m_model.getItemById(id);
    QString actual;

    if(moved!=nullptr)
    {
      QStringList path=moved->getPath();

      if(path.size()>=2)
        actual=path.at(path.size()-2);
    }

    if(moved==nullptr || actual!=expect)
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["id"]=id;
    result["new_parent_id"]=actual;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject movedResult=m_tools.moveBranchDirect(id, targetId, asChild, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=movedResult;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}


QJsonObject McpSession::writeMoveNote(const QString &id,
                                      const QString &branchId,
                                      QString &toolError)
{
  QJsonObject result;

  if(mytetraConfig.get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  if(!m_model.isRecordIdExists(id))
  {
    toolError="Unknown note_id: "+id;

    return result;
  }

  if(m_model.getItemById(branchId)==nullptr)
  {
    toolError="Unknown branch_id: "+branchId;

    return result;
  }

  QJsonObject payload;
  payload["id"]=id;
  payload["branch_id"]=branchId;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpMoveNote", payload, sendError);

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

    QStringList path=m_model.getRecordPath(id);
    QString actual=path.isEmpty() ? QString() : path.last();

    if(actual!=branchId)
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    result["id"]=id;
    result["branch_id"]=branchId;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject movedResult=m_tools.moveNoteDirect(id, branchId, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=movedResult;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}


// Ожидание звездочки в сайдкаре после живой записи: GUI пишет
// файл сам, mtime базы при этом не сдвигается


// Ожидание звездочки в сайдкаре после живой записи: GUI пишет
// файл сам, mtime базы при этом не сдвигается
static bool waitSidecarFavorite(const QString &id, bool starred, int timeoutMs)
{
  QElapsedTimer timer;
  timer.start();

  while(timer.elapsed()<timeoutMs)
  {
    if(McpTools::sidecarFavoriteIds().contains(id)==starred)
      return true;

    QThread::msleep(100);
  }

  return McpTools::sidecarFavoriteIds().contains(id)==starred;
}


QJsonObject McpSession::writeFavoriteNote(const QString &id,
                                          bool starred,
                                          QString &toolError)
{
  QJsonObject result;

  if(mytetraConfig.get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

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
      toolError="Note is encrypted, favorite refused";

      return result;
    }
  }

  const bool xmlStorage=(mytetraConfig.get_favoritesStorage()==QStringLiteral("xml"));

  QJsonObject payload;
  payload["id"]=id;
  payload["starred"]=starred;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath,
                                     starred ? "mcpFavoriteNote" : "mcpUnfavoriteNote",
                                     payload, sendError);

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
    if(xmlStorage)
    {
      if(!synced)
        waitGuiSave(5000);
      resyncModel();

      Record *record=m_model.isRecordIdExists(id) ? m_model.getRecord(id) : nullptr;
      const bool applied=(record!=nullptr &&
                          (record->getField("favorite")=="1")==starred);

      if(!applied)
      {
        toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

        return result;
      }
    }
    else
    {
      if(!waitSidecarFavorite(id, starred, 5000))
      {
        toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

        return result;
      }
    }

    result["id"]=id;
    result["favorite"]=starred;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject applied=starred ? m_tools.favoriteNoteDirect(id, error)
                              : m_tools.unfavoriteNoteDirect(id, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=applied;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}


QJsonObject McpSession::writeImportBranch(const QString &parentId,
                                            const QString &path,
                                            QString &toolError)
{
  QJsonObject result;

  if(mytetraConfig.get_readOnly())
  {
    toolError="Database is in global read-only mode, writes refused";

    return result;
  }

  // Предпроверки по свежему снапшоту: мгновенные ошибки без
  // ложного сообщения живому GUI (там проверки дублируются)
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
      toolError="Parent branch is encrypted, import refused";

      return result;
    }
  }

  {
    const QString problem=McpTools::checkImportDir(path);

    if(!problem.isEmpty())
    {
      toolError=problem;

      return result;
    }
  }

  // Дети родителя до ввоза и имя верхнего узла обменника: после
  // применения по diff находится id новой ветки (сам ввоз id
  // заранее не знает). Указатели не хранить: resync их инвалидирует
  const QSet<QString> beforeIds=branchChildIds(m_model, parentId);
  const QString topName=importTopNodeName(path);

  QJsonObject payload;
  payload["parent_id"]=parentId;
  payload["path"]=path;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpImportBranch", payload, sendError);

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
    // Ввоз сам сохраняет дерево: у старого GUI сейв ждётся по mtime,
    // у нового (synced) он уже случился, модель просто перечитывается
    if(!synced && !waitGuiSave(5000))
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    resyncModel();

    QStringList fresh;

    foreach(const QString &id, branchChildIds(m_model, parentId))
      if(!beforeIds.contains(id))
      {
        TreeItem *item=m_model.getItemById(id);

        if(item!=nullptr && item->getField("name")==topName)
          fresh.append(id);
      }

    if(fresh.isEmpty())
    {
      toolError=synced ? "GUI reported success but the change is not visible after resync, internal error"
                       : "GUI did not apply the change in time, retry";

      return result;
    }

    // Конкурентный ввоз с тем же именем: id неоднозначен,
    // но применение подтверждено сейвом
    if(fresh.size()==1)
      result["id"]=fresh.first();

    result["parent_id"]=parentId;
    result["applied_via"]="gui";
    result["verified"]=true;

    return result;
  }

  QString error;
  QJsonObject imported=m_tools.importBranchDirect(parentId, path, error);

  if(!error.isEmpty())
  {
    toolError=error;

    return result;
  }

  result=imported;
  result["applied_via"]="direct";
  result["verified"]=true;

  return result;
}

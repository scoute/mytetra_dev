// Цель Windows — до ЛЮБЫХ инклудов: Qt-заголовки сами тянут windows.h,
// и поздний _WIN32_WINNT уже ничего не даёт (QueryFullProcessImageNameA
// из mcpPeerAlive() тогда не объявлена, MinGW32 падает)
#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <QDialog>
#include <QMap>
#include <QString>
#include <QDir>
#include <QString>
#include <QDesktopWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPrintDialog>
#include <QPrinter>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSplitter>
#include <QDockWidget>
#include <QStatusBar>
#include <QActionGroup>
#include <QColorDialog>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>

#ifdef Q_OS_WIN32
#include <windows.h>
#else
#include <signal.h>
#include <errno.h>
#endif
#include <QTime>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>


// Элемент таблицы с числовой сортировкой: по клику на заголовок
// QTableWidget сравнивает operator<, текст "156" < "20" строкой бы врал
class NumberTableItem : public QTableWidgetItem
{
public:
  explicit NumberTableItem(int value) : QTableWidgetItem(QString::number(value))
  {
    setData(Qt::UserRole, value);
  }

  bool operator<(const QTableWidgetItem &other) const override
  {
    return data(Qt::UserRole).toInt() < other.data(Qt::UserRole).toInt();
  }
};

#include "main.h"
#include "models/appConfig/AppConfig.h"
#include "MainWindow.h"
#include "views/printPreview/PrintPreview.h"
#include "views/appConfigWindow/AppConfigDialog.h"
#include "libraries/TrashMonitoring.h"
#include "views/tree/TreeScreen.h"
#include "views/telegram/TelegramImportDialog.h"
#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorToolBarAssistant.h"
#include "libraries/wyedit/EditorConfig.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/historyPanel/HistoryPanel.h"
#include "views/favoritesPanel/FavoritesPanel.h"
#include "views/backlinksPanel/BacklinksPanel.h"
#include "libraries/BacklinkIndex.h"
#include "views/tagsPanel/TagsPanel.h"
#include "views/contentGallery/ContentGallery.h"
#include "models/tree/TreeItem.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/GlobalParameters.h"
#include "views/consoleEmulator/CommandRunner.h"
#include "libraries/WalkHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/WindowSwitcher.h"
#include "views/actionLog/ActionLogScreen.h"
#include "views/databasesManagement/DatabasesManagementScreen.h"
#include "libraries/ShortcutManager.h"
#include "libraries/RandomInitter.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/CssHelper.h"
#include "libraries/mcp/McpTools.h"
#include "libraries/Favorites.h"
#include "models/recordTable/Record.h"

#include "McpGuiBridge.h"

// P3: конструктор, геттеры состояния (мелкие, жили бы в h,
// но Q_OBJECT-класс держит их здесь для читаемости)
McpGuiBridge::McpGuiBridge(KnowTreeModel *model, QObject *parent) : QObject(parent),
    m_model(model)
{

}


McpGuiBridge::~McpGuiBridge(void)
{

}


void McpGuiBridge::setModel(KnowTreeModel *model)
{
    m_model=model;
}


bool McpGuiBridge::hasPeers(void) const
{
    return !m_mcpPeers.isEmpty();
}


const QList<McpHistoryEntry> &McpGuiBridge::history(void) const
{
    return m_mcpHistory;
}


void McpGuiBridge::clearHistory(void)
{
    m_mcpHistory.clear();
}

static QString mcpOwnAppName(void);
static bool mcpPeerAlive(const QString &pid);

// P3: мост MCP-сервер -> живой GUI. Методы переехали из MainWindow
// дословно; отличия: m_model, хуки isReadOnly/mcpAllowed/clearEditor,
// сигналы вместо прямых правок баннера/диалога

// Слот, обрабатывающий внешние сообщения
// Используется для обработки ссобщений, приходящих от других экземпляров MyTetra
// Разбор аргумента MCP-команд: весь текст после первого пробела это
// компактный JSON (пробелы внутри допустимы, переводов строк нет)
QJsonObject McpGuiBridge::parseMcpCommandArgs(const QString &message)
{
  int spacePos=message.indexOf(' ');

  if(spacePos<0)
    return QJsonObject();

  QJsonParseError parseError;
  QJsonDocument doc=QJsonDocument::fromJson(message.mid(spacePos+1).toUtf8(),
                                            &parseError);

  if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
    return QJsonObject();

  return doc.object();
}


// Синхронный ответ на mcp-команду для протокола запрос-ответ.
// Упаковка итога handleMcpCommand в компактный JSON


// Синхронный ответ на mcp-команду для протокола запрос-ответ.
// Упаковка итога handleMcpCommand в компактный JSON
void McpGuiBridge::setFavorites(Favorites *favorites)
{
    m_favorites=favorites;
}


void McpGuiBridge::setBacklinkIndex(BacklinkIndex *backlinks)
{
    m_backlinks=backlinks;
}


QByteArray McpGuiBridge::handleMcpMessage(const QString &message)
{
  QStringList messageParts=message.split(" ");

  if(messageParts.isEmpty())
    return QJsonDocument(QJsonObject{{"ok", false},
                                     {"error", "Empty message"}}).toJson(QJsonDocument::Compact);

  const QString command=messageParts.at(0);

  if(!mcpAllowedCheck())
    return QJsonDocument(QJsonObject{{"ok", false},
                                     {"error", "MCP is disabled in settings"}}).toJson(QJsonDocument::Compact);

  QJsonObject args=parseMcpCommandArgs(message);
  QJsonObject result;
  QString error;

  if(!handleMcpCommand(command, args, result, error))
    return QJsonDocument(QJsonObject{{"ok", false},
                                     {"error", error.isEmpty() ? "Command failed" : error}})
      .toJson(QJsonDocument::Compact);

  result["ok"]=true;

  return QJsonDocument(result).toJson(QJsonDocument::Compact);
}


// Применение одной mcp-команды к живой модели с возвратом итога.
// Тела веток - те же applyXxx что были в messageHandler, но тихие
// return заменены на текст ошибки: сервер отдаёт её агенту как есть,
// вместо гадания "did not apply... retry"


// Применение одной mcp-команды к живой модели с возвратом итога.
// Тела веток - те же applyXxx что были в messageHandler, но тихие
// return заменены на текст ошибки: сервер отдаёт её агенту как есть,
// вместо гадания "did not apply... retry"


// Применение одной mcp-команды к живой модели с возвратом итога.
// Тела веток - те же applyXxx что были в messageHandler, но тихие
// return заменены на текст ошибки: сервер отдаёт её агенту как есть,
// вместо гадания "did not apply... retry"
bool McpGuiBridge::handleMcpCommand(const QString &command,
                                  const QJsonObject &args,
                                  QJsonObject &result,
                                  QString &error)
{
  result=QJsonObject();
  error=QString();

  if(command=="mcpCreateBranch")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject created=McpTools::applyCreateBranch(m_model,
                                                    args.value("parent_id").toString(),
                                                    args.value("id").toString(),
                                                    args.value("name").toString(),
                                                    error);

    if(!error.isEmpty())
      return false;

    if(created.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    m_model->save();
    flashMcpWrite();

    result=created;
    return true;
  }

  else if(command=="mcpCreateNote")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject created=McpTools::applyCreateNote(m_model,
                                                  args.value("branch_id").toString(),
                                                  args.value("id").toString(),
                                                  args.value("name").toString(),
                                                  args.value("html").toString(),
                                                  args.value("tags").toString(),
                                                  error);

    if(!error.isEmpty())
      return false;

    if(created.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    m_model->save();
    flashMcpWrite();

    result=created;
    return true;
  }

  else if(command=="mcpUpdateNote")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QStringList updatedFields;

    // Открытый в редакторе текст не трогается: редактор держит свой
    // буфер до перевыбора (last writer wins), как при любом
    // стороннем изменении файла
    const bool ok=McpTools::applyUpdateNote(m_model,
                                            args.value("id").toString(),
                                            args.value("has_name").toBool(false),
                                            args.value("name").toString(),
                                            args.value("has_html").toBool(false),
                                            args.value("html").toString(),
                                            args.value("has_tags").toBool(false),
                                            args.value("tags").toString(),
                                            error,
                                            updatedFields);

    if(!ok)
    {
      if(error.isEmpty())
        error="Note was not updated";

      return false;
    }

    m_model->save();
    flashMcpWrite();

    result["id"]=args.value("id").toString();

    QJsonArray fields;
    foreach(const QString &field, updatedFields)
      fields.append(field);
    result["updated_fields"]=fields;

    return true;
  }

  else if(command=="mcpDeleteNote")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(id.isEmpty())
    {
      error="Empty id";
      return false;
    }

    if(!McpTools::applyDeleteNote(m_model, m_backlinks, id, error))
    {
      if(error.isEmpty())
        error="Note was not deleted";

      return false;
    }

    emit m_model->doCloseDetachedWindowByIdSet(QSet<QString>() << id);

    m_model->save();
    flashMcpWrite();

    result["id"]=id;
    return true;
  }

  else if(command=="mcpDeleteBranch")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(id.isEmpty())
    {
      error="Empty id";
      return false;
    }

    QJsonObject deleted=McpTools::applyDeleteBranch(m_model,
                                                    id, error);

    if(!error.isEmpty())
      return false;

    if(deleted.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    m_model->save();
    flashMcpWrite();

    result=deleted;
    return true;
  }

  else if(command=="mcpRenameBranch")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject renamed=McpTools::applyRenameBranch(m_model,
                                                    args.value("id").toString(),
                                                    args.value("name").toString(),
                                                    error);

    if(!error.isEmpty())
      return false;

    if(renamed.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    m_model->save();
    flashMcpWrite();

    result=renamed;
    return true;
  }

  else if(command=="mcpMoveBranch")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject moved=McpTools::applyMoveBranch(m_model,
                                                args.value("id").toString(),
                                                args.value("target_id").toString(),
                                                args.value("as_child").toBool(true),
                                                error);

    if(!error.isEmpty())
      return false;

    if(moved.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    m_model->save();
    flashMcpWrite();

    result=moved;
    return true;
  }

  else if(command=="mcpMoveNote")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    // Как при удалении: редактор может держать указатель внутрь
    // таблицы-источника, его состояние сбрасывается до переноса
    if(clearEditor)
        clearEditor();

    QJsonObject moved=McpTools::applyMoveNote(m_model,
                                              args.value("id").toString(),
                                              args.value("branch_id").toString(),
                                              error);

    if(!error.isEmpty())
      return false;

    if(moved.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    m_model->save();
    flashMcpWrite();

    result=moved;
    return true;
  }

  else if(command=="mcpFavoriteNote")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(!m_model->isRecordIdExists(id))
    {
      error="Unknown note_id: "+id;
      return false;
    }

    Record *record=m_model->getRecord(id);

    if(record==nullptr || record->getField("crypt")=="1")
    {
      error="Note is not available";
      return false;
    }

    m_favorites->setFavorite(id, true);

    if(!m_favorites->isFavorite(id))
    {
      error="Note was not starred";
      return false;
    }

    flashMcpWrite();

    result["id"]=id;
    result["favorite"]=true;
    return true;
  }

  else if(command=="mcpUnfavoriteNote")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(!m_model->isRecordIdExists(id))
    {
      error="Unknown note_id: "+id;
      return false;
    }

    m_favorites->setFavorite(id, false);

    if(m_favorites->isFavorite(id))
    {
      error="Note is still starred";
      return false;
    }

    flashMcpWrite();

    result["id"]=id;
    result["favorite"]=false;
    return true;
  }

  else if(command=="mcpImportBranch")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString parentId=args.value("parent_id").toString();
    const QString path=args.value("path").toString();

    if(!parentId.isEmpty())
    {
      TreeItem *parentItem=m_model->getItemById(parentId);

      if(parentItem==nullptr || parentItem->getField("crypt")=="1")
      {
        error="Parent branch is not available";
        return false;
      }
    }

    // Плохой обменник вместо модального диалога - отказ с причиной
    const QString dirError=McpTools::checkImportDir(path);

    if(!dirError.isEmpty())
    {
      error=dirError;
      return false;
    }

    QJsonObject imported=McpTools::applyImportBranch(m_model,
                                                     parentId, path, error);

    if(!error.isEmpty())
      return false;

    if(imported.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    flashMcpWrite();

    result=imported;
    return true;
  }

  else if(command=="mcpRenameTag")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    int renamed=0;
    int skipped=0;

    if(!McpTools::applyRenameTag(m_model,
                                 args.value("old").toString(),
                                 args.value("new").toString(),
                                 error, renamed, skipped))
    {
      if(error.isEmpty())
        error="Tag was not renamed";

      return false;
    }

    m_model->save();
    flashMcpWrite();

    result["renamed"]=renamed;
    result["skipped"]=skipped;
    return true;
  }

  else if(command=="mcpDeleteTag")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    int cleared=0;
    int skipped=0;

    if(!McpTools::applyDeleteTag(m_model,
                                 args.value("tag").toString(),
                                 error, cleared, skipped))
    {
      if(error.isEmpty())
        error="Tag was not deleted";

      return false;
    }

    m_model->save();
    flashMcpWrite();

    result["cleared"]=cleared;
    result["skipped"]=skipped;
    return true;
  }

  else if(command=="mcpApplyBatch")
  {
    if(readOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonArray operations=args.value("operations").toArray();

    if(operations.isEmpty())
    {
      error="Empty operations list";
      return false;
    }

    QSet<QString> closeWindows;
    bool needSave=false;
    QJsonArray opResults;

    for(const QJsonValue &opValue : operations)
    {
      const QJsonObject op=opValue.toObject();
      const QString tool=op.value("tool").toString();
      const QJsonObject opArgs=op.value("arguments").toObject();

      // import/export в пакет не входят
      if(tool==QStringLiteral("import_branch") ||
         tool==QStringLiteral("export_branch"))
      {
        error="Unsupported in batch: "+tool;
        break;
      }

      // Как в одиночке: редактор может держать указатель внутрь
      // таблицы-источника, его состояние сбрасывается до переноса
      if(tool==QStringLiteral("move_note"))
        if(clearEditor)
        clearEditor();

      // Как в одиночке: открепленные окна удаляемой записи
      if(tool==QStringLiteral("delete_note"))
        closeWindows.insert(opArgs.value("note_id").toString());

      QJsonObject dummy;

      if(!McpTools::applyBatchOp(m_model,
                                 m_backlinks,
                                 tool, opArgs, dummy, error))
        break;

      QJsonObject opResult;
      opResult["tool"]=tool;
      opResult["ok"]=true;
      opResults.append(opResult);

      // Избранное сохраняет само (синглетон/сайдкар), остальное
      // требует сейва модели
      if(tool!=QStringLiteral("favorite_note") &&
         tool!=QStringLiteral("unfavorite_note"))
        needSave=true;
    }

    if(!closeWindows.isEmpty())
      emit m_model->doCloseDetachedWindowByIdSet(closeWindows);

    if(needSave)
      m_model->save();

    flashMcpWrite();

    // Стоп на первой ошибке: пакет неатомарен, примененное сохраняется
    result["applied"]=opResults.size();
    result["total"]=operations.size();
    result["operations"]=opResults;

    if(!error.isEmpty() || opResults.size()!=operations.size())
    {
      if(error.isEmpty())
        error="Batch stopped before all operations";

      return false;
    }

    return true;
  }

  else if(command=="mcpHello")
  {
    const QString pid=args.value("pid").toString();

    if(pid.isEmpty())
    {
      error="Empty pid";
      return false;
    }

    // Старт сессии с нуля: история чистая, окно появляется само
    const bool sessionStarts=m_mcpPeers.isEmpty();

    m_mcpPeers[pid]=QDateTime::currentMSecsSinceEpoch();
    m_mcpPeerModes[pid]=args.value("mode").toString();
    emit sessionChanged();

    if(sessionStarts)
    {
      m_mcpHistory.clear();
      emit historyChanged();
      emit sessionStarted();
    }

    return true;
  }

  else if(command=="mcpBye")
  {
    const QString pid=args.value("pid").toString();

    if(pid.isEmpty())
    {
      error="Empty pid";
      return false;
    }

    m_mcpPeers.remove(pid);
    m_mcpPeerModes.remove(pid);
    emit sessionChanged();

    return true;
  }

  else if(command=="mcpHistory")
  {
    addMcpHistoryEntry(args.value("kind").toString(),
                       args.value("tool").toString(),
                       args.value("summary").toString());

    return true;
  }

  error="Unknown command: "+command;
  return false;
}


// Восстанавливается геометрия окна и позиции основных разделителей


// Внешняя MCP-правка: баннер и так висит всю сессию, здесь только
// пересчёт (текст с режимами, видимость, блокировка)


// Текст баннера: сессия активна и режимы пиров в скобках сразу
// после «активна». UI заблокирован всегда пока висит баннер,
// независимо от режима сервера: его записи идут мимо блокировки
// своим каналом
QString McpGuiBridge::bannerText(void)
{
    QStringList modes=m_mcpPeerModes.values();
    modes.removeAll(QString());
    modes.removeDuplicates();
    modes.sort();

    QString modesText;

    if(!modes.isEmpty())
        modesText=" ("+modes.join(", ")+")";

    QStringList peers;

    for(auto it=m_mcpPeers.constBegin(); it!=m_mcpPeers.constEnd(); ++it)
    {
      const QString mode=m_mcpPeerModes.value(it.key());

      peers.append(mode.isEmpty() ? it.key() : it.key()+QStringLiteral(" ")+mode);
    }

    QString text=tr("MCP session is active%1, MyTetra works in read-only mode").arg(modesText);

    if(!peers.isEmpty())
      text+=tr("; peers: %1").arg(peers.join(QStringLiteral(", ")));

    return text;
}


// Баннер MCP-сессии: висит всё время пока жив хоть один пир.
// TTL-килла нет осознанно: долгие нейронные сессии с паузами
// убивались бы на ровном месте вместе с историей. Пир уходит
// только по явному Bye; убитый -9 висит до перезапуска программы.
// Пусто — баннер гаснет и UI разблокируется, есть пиры — баннер
// виден и UI заблокирован на запись до конца сессии
// Имя своего бинаря: MCP-сервер это та же программа, так что
// чужой процесс под recycled pid опознается по имени


// Баннер MCP-сессии: висит всё время пока жив хоть один пир.
// TTL-килла нет осознанно: долгие нейронные сессии с паузами
// убивались бы на ровном месте вместе с историей. Пир уходит
// только по явному Bye; убитый -9 висит до перезапуска программы.
// Пусто — баннер гаснет и UI разблокируется, есть пиры — баннер
// виден и UI заблокирован на запись до конца сессии
// Имя своего бинаря: MCP-сервер это та же программа, так что
// чужой процесс под recycled pid опознается по имени


// Внешняя MCP-правка: баннер и так висит всю сессию, здесь только
// пересчёт (текст с режимами, видимость, блокировка)
void McpGuiBridge::flashMcpWrite(void)
{
    emit sessionChanged();
}


// Текст баннера: сессия активна и режимы пиров в скобках сразу
// после «активна». UI заблокирован всегда пока висит баннер,
// независимо от режима сервера: его записи идут мимо блокировки
// своим каналом


// Текст баннера: сессия активна и режимы пиров в скобках сразу
// после «активна». UI заблокирован всегда пока висит баннер,
// независимо от режима сервера: его записи идут мимо блокировки
// своим каналом


// Накопление истории действий MCP-сессии. Хвост обрезан до трёхсот:
// окну нужен обзор, а не архив. На диск ничего не пишется
void McpGuiBridge::addMcpHistoryEntry(const QString &kind,
                                    const QString &tool,
                                    const QString &summary)
{
    McpHistoryEntry entry;
    entry.msecs=QDateTime::currentMSecsSinceEpoch();
    entry.kind=kind.isEmpty() ? QStringLiteral("other") : kind;
    entry.tool=tool;
    entry.summary=summary;

    m_mcpHistory.append(entry);

    // История действий MCP для диалога: сессия агента легко
    // дает сотни вызовов, сотни строк обрезали бы середину работы
    while(m_mcpHistory.size()>5000)
        m_mcpHistory.removeFirst();

    emit historyChanged();
}


void McpGuiBridge::checkPeers(void)
{
  QList<QString> dead;

  for(auto it=m_mcpPeers.constBegin(); it!=m_mcpPeers.constEnd(); ++it)
    if(!mcpPeerAlive(it.key()))
      dead.append(it.key());

  foreach(const QString &pid, dead)
  {
    m_mcpPeers.remove(pid);
    m_mcpPeerModes.remove(pid);
  }

  emit sessionChanged();
}


// Баннер MCP-сессии: висит всё время пока жив хоть один пир.
// TTL-килла нет осознанно: долгие нейронные сессии с паузами
// убивались бы на ровном месте вместе с историей. Пир уходит
// только по явному Bye; убитый -9 висит до перезапуска программы.
// Пусто — баннер гаснет и UI разблокируется, есть пиры — баннер
// виден и UI заблокирован на запись до конца сессии
// Имя своего бинаря: MCP-сервер это та же программа, так что
// чужой процесс под recycled pid опознается по имени
static QString mcpOwnAppName(void)
{
  return QFileInfo(QCoreApplication::applicationFilePath()).baseName();
}


// Жив ли пир: процесс с таким pid существует И это наша программа.
// Имя сверяется чтобы переиспользованный pid чужого процесса не
// держал висящий баннер. Не прочиталось имя — считаем живым:
// лучше лишний баннер, чем выкинутый живой пир
static bool mcpPeerAlive(const QString &pid)
{
  const qint64 id=pid.toLongLong();

  if(id<=0)
    return false;

#ifdef Q_OS_WIN32
  HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                            static_cast<DWORD>(id));

  if(handle==nullptr)
    return GetLastError()==ERROR_ACCESS_DENIED;

  char imagePath[MAX_PATH];
  DWORD imageSize=sizeof(imagePath);
  const bool nameRead=(QueryFullProcessImageNameA(handle, 0,
                                                  imagePath, &imageSize)!=0);
  CloseHandle(handle);

  if(!nameRead)
    return true;

  return QFileInfo(QString::fromLocal8Bit(imagePath)).baseName()
         .compare(mcpOwnAppName(), Qt::CaseInsensitive)==0;
#elif defined(Q_OS_LINUX)
  if(::kill(static_cast<pid_t>(id), 0)!=0)
    return errno==EPERM;

  QFile commFile(QStringLiteral("/proc/%1/comm").arg(id));

  if(!commFile.open(QIODevice::ReadOnly | QIODevice::Text))
    return true;

  // comm режется ядром до 15 символов: сравнивается столько же
  const QString comm=QString::fromLocal8Bit(commFile.readAll()).trimmed();

  return comm==mcpOwnAppName().left(15);
#else
  if(::kill(static_cast<pid_t>(id), 0)==0)
    return true;

  return errno==EPERM;
#endif
}


bool McpGuiBridge::readOnly(void) const
{
    // Хук не задан (пробы без конфига): записи запрещены.
    // Fail-closed: лучше отклонить, чем писать без спроса
    return isReadOnly ? isReadOnly() : true;
}


bool McpGuiBridge::mcpAllowedCheck(void) const
{
    return mcpAllowed ? mcpAllowed() : false;
}


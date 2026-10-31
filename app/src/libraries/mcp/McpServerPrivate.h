#ifndef MCP_SERVER_PRIVATE_H
#define MCP_SERVER_PRIVATE_H

// P3: внутренности MCP-сервера, общие между TU.
// Класс сессии + ранее file-static хелперы с кросс-TU использованием.

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QSet>
#include <QFileInfo>
#include <QDateTime>

class KnowTreeModel;
class McpTools;

// Режимы сервера: read-only, read-write, read-write с удалением.
enum class McpMode { ReadOnly=0, ReadWrite=1, ReadWriteDelete=2 };

enum GuiWriteState { GUI_NOT_RUNNING, GUI_DELIVERED, GUI_APPLIED, GUI_REFUSED, GUI_FAILED };

QString serverVersionString(void);
QString negotiateVersion(const QString &asked);
QJsonObject errorObject(int code, const QString &message);
McpMode mcpModeFromArgs(int argc, char **argv);
QString mcpModeName(McpMode mode);
bool toolAllowed(McpMode mode, const QString &toolName);
QStringList readFields(const QJsonObject &arguments);
bool isKnownTool(const QString &toolName);
bool isWriteTool(const QString &toolName);
QJsonArray buildToolsList(McpMode mode);
GuiWriteState sendGuiCommand(const QString &binaryPath,
                                    const QString &command,
                                    const QJsonObject &payload,
                                    QString &error,
                                    QJsonObject *reply=nullptr);
void notifyMcpPresence(const QString &binaryPath, McpMode mode,
                              const QString &command);
QString expectedMoveParent(KnowTreeModel &model,
                                  const QString &targetId,
                                  bool asChild);
bool validateDatabase(const QString &xmlPath);

// Сессия обработки сообщений (диспетчер handleMessage - в Gui TU,
// write-методы - в Session/Batch TU)
class McpSession
{
public:
  McpSession(KnowTreeModel &model, const QString &xmlPath, McpTools &tools,
             const QString &binaryPath, McpMode mode)
    : m_model(model), m_xmlPath(xmlPath), m_tools(tools),
      m_binaryPath(binaryPath), m_mode(mode)
  {
    m_xmlMtime=QFileInfo(xmlPath).lastModified();
  }

  // Обработка одного сообщения. Возвращает true если ответ нужно
  // отправить (уведомления без id ответа не требуют)
  bool handleMessage(const QByteArray &line, QByteArray &response);

private:
  KnowTreeModel &m_model;
  QString m_xmlPath;
  McpTools &m_tools;
  QString m_binaryPath;
  McpMode m_mode;
  QDateTime m_xmlMtime;

  // Ожидание сейва GUI после доставки команды: mtime базы сдвигается
  bool waitGuiSave(int timeoutMs);

  // Перечитывание структуры с диска и синхронизация метки
  void resyncModel(void);

  // Свежие id под текущий (возможно только что перечитанный) снапшот
  QString freshBranchId(void);
  QString freshNoteId(void);

  // Запись с роутингом GUI/напрямую, applied_via+verified в результате
  QJsonObject writeBranch(const QString &id, const QString &parentId,
                          const QString &name, QString &toolError);
  QJsonObject writeNote(const QString &id, const QString &branchId,
                        const QString &name, const QString &html,
                        const QString &tags, QString &toolError);
  QJsonObject writeNoteUpdate(const QString &id,
                              bool hasName, const QString &name,
                              bool hasHtml, const QString &html,
                              bool hasTags, const QString &tags,
                              QString &toolError);
  QJsonObject writeDeleteNote(const QString &id, QString &toolError);
  QJsonObject writeDeleteBranch(const QString &id, QString &toolError);
  QJsonObject writeApplyBatch(const QJsonArray &operations, QString &toolError);
  QJsonObject writeRenameBranch(const QString &id, const QString &name,
                                QString &toolError);
  QJsonObject writeMoveBranch(const QString &id, const QString &targetId,
                              bool asChild, QString &toolError);
  QJsonObject writeMoveNote(const QString &id, const QString &branchId,
                            QString &toolError);
  QJsonObject writeRenameTag(const QString &oldTag, const QString &newTag,
                             QString &toolError);
  QJsonObject writeDeleteTag(const QString &tag, QString &toolError);
  QJsonObject writeFavoriteNote(const QString &id, bool starred,
                                QString &toolError);
  QJsonObject writeImportBranch(const QString &parentId,
                                const QString &path,
                                QString &toolError);

  static QByteArray makeResponse(const QJsonValue &id,
                                 const QJsonValue &result,
                                 const QJsonValue &error)
  {
    QJsonObject envelope;
    envelope["jsonrpc"]="2.0";

    if(!id.isUndefined())
      envelope["id"]=id;
    else
      envelope["id"]=QJsonValue();

    if(!result.isUndefined() && !result.isNull())
      envelope["result"]=result;
    else if(result.isObject() && result.toObject().isEmpty())
      envelope["result"]=result;

    if(error.isObject())
      envelope["error"]=error.toObject();

    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
  }

  void handleNotification(const QString &method, const QJsonObject &params)
  {
    Q_UNUSED(method);
    Q_UNUSED(params);
    // notifications/initialized и прочие уведомления:
    // серверу состояние клиента не нужно, игнорируются
  }
};

#endif // MCP_SERVER_PRIVATE_H

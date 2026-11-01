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
#include "models/tree/KnowTreeModel.h"

// Версии протокола MCP которые понимает сервер.
// Запрошенная клиентом поддерживаемая версия эхомится назад,
// незнакомая сводится к самой старой стабильной

#include "McpServerPrivate.h"

// Проверка свежести структуры: mytetra.xml перезаписали
// (сохранились из GUI) - модель перечитывается
static void refreshModelIfNeeded(KnowTreeModel &model,
                                 const QString &xmlPath,
                                 QDateTime &xmlMtime)
{
  QDateTime current=QFileInfo(xmlPath).lastModified();

  if(current.isValid() && current!=xmlMtime)
  {
    model.reload();
    xmlMtime=current;
  }
}

// --- Маршрутизация записи: через работающий GUI или напрямую ---
// Имя сокета single-instance повторяет QtLocalPeer: GUI слушает,
// MCP только подключается и отправляет

// --- Маршрутизация записи: через работающий GUI или напрямую ---
// Имя сокета single-instance повторяет QtLocalPeer: GUI слушает,
// MCP только подключается и отправляет
static QString socketNameForId(const QString &id)
{
  QString prefix=id.section(QLatin1Char('/'), -1);
  prefix.remove(QRegularExpression("[^a-zA-Z]"));
  prefix.truncate(6);

  QByteArray idc=id.toUtf8();
  quint16 idNum=qChecksum(idc.constData(), idc.size());

  QString name=QLatin1String("qtsingleapp-")+prefix+
               QLatin1Char('-')+QString::number(idNum, 16);

#if defined(Q_OS_WIN)
  typedef BOOL(WINAPI*PProcessIdToSessionId)(DWORD, DWORD*);
  static PProcessIdToSessionId pProcessIdToSessionId=0;

  if(!pProcessIdToSessionId)
  {
    QLibrary lib("kernel32");
    pProcessIdToSessionId=(PProcessIdToSessionId)lib.resolve("ProcessIdToSessionId");
  }

  if(pProcessIdToSessionId)
  {
    DWORD sessionId=0;
    pProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    name+=QLatin1Char('-')+QString::number(sessionId, 16);
  }
#else
  name+=QLatin1Char('-')+QString::number(::getuid(), 16);
#endif

  return name;
}

// Кандидаты имени сокета: applicationFilePath (та же функция что
// у GUI) плюс canonical argv[0]. Пробуются по порядку: относительный
// argv[0] резолвится от чужого cwd и для сокета ненадежен, поэтому
// одного кандидата мало. Какой коннектится - тот и живой GUI из
// того же файла. Разные файлы (копия бинарника) дают другое имя:
// безопасное падение в прямой путь

// Кандидаты имени сокета: applicationFilePath (та же функция что
// у GUI) плюс canonical argv[0]. Пробуются по порядку: относительный
// argv[0] резолвится от чужого cwd и для сокета ненадежен, поэтому
// одного кандидата мало. Какой коннектится - тот и живой GUI из
// того же файла. Разные файлы (копия бинарника) дают другое имя:
// безопасное падение в прямой путь
static QStringList controlSocketCandidates(const QString &argvPath)
{
  QStringList ids;

  QString appPath=QCoreApplication::applicationFilePath();

  if(!appPath.isEmpty())
    ids.append(appPath);

  QString canonPath=QFileInfo(argvPath).canonicalFilePath();

  if(!canonPath.isEmpty() && !ids.contains(canonPath))
    ids.append(canonPath);

  QStringList names;

  foreach(const QString &id, ids)
    names.append(socketNameForId(id));

  return names;
}

// Отправка команды живому GUI по control-каналу. Два протокола ответа:
// - новый GUI: фрейм с JSON {"ok":true,...} или {"ok":false,"error":"..."}.
//   Итог применения известен сразу: APPLIED (применено и сохранено) или
//   REFUSED (GUI отказал с причиной, повтор бессмысленен);
// - старый GUI: голые 3 байта "ack". Только доставка без итога: DELIVERED,
//   дальше старый путь ожидания сейва по mtime с перепроверкой.
// Таймаут ответа покрывает применение с сейвом большой базы. Его истечение
// означает что нить GUI заблокирована (модальный диалог, синхронизация):
// изменение НЕ применено, повторять тот же вызов можно, дубли не грозят

// Отправка команды живому GUI по control-каналу. Два протокола ответа:
// - новый GUI: фрейм с JSON {"ok":true,...} или {"ok":false,"error":"..."}.
//   Итог применения известен сразу: APPLIED (применено и сохранено) или
//   REFUSED (GUI отказал с причиной, повтор бессмысленен);
// - старый GUI: голые 3 байта "ack". Только доставка без итога: DELIVERED,
//   дальше старый путь ожидания сейва по mtime с перепроверкой.
// Таймаут ответа покрывает применение с сейвом большой базы. Его истечение
// означает что нить GUI заблокирована (модальный диалог, синхронизация):
// изменение НЕ применено, повторять тот же вызов можно, дубли не грозят
GuiWriteState sendGuiCommand(const QString &binaryPath,
                                    const QString &command,
                                    const QJsonObject &payload,
                                    QString &error,
                                    QJsonObject *reply)
{
  QLocalSocket socket;

  bool connected=false;

  foreach(const QString &socketName, controlSocketCandidates(binaryPath))
  {
    socket.connectToServer(socketName);

    if(socket.waitForConnected(800))
    {
      connected=true;
      break;
    }
  }

  if(!connected)
    return GUI_NOT_RUNNING;

  QString message=command+" "+
                  QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));

  QByteArray uMsg=message.toUtf8();

  QDataStream ds(&socket);
  ds.writeBytes(uMsg.constData(), uMsg.size());

  if(!socket.waitForBytesWritten(2000))
  {
    error="GUI is running but the command was not sent";
    return GUI_FAILED;
  }

  // Ждём именно итог применения, а не эхо доставки
  QByteArray answer;
  QElapsedTimer timer;
  timer.start();

  const int totalMs=30000;
  bool frameComplete=false;
  bool legacyAck=false;

  while(timer.elapsed()<totalMs && !frameComplete && !legacyAck)
  {
    const int remainMs=totalMs-static_cast<int>(timer.elapsed());

    if(remainMs<=0 || !socket.waitForReadyRead(remainMs))
      break;

    answer.append(socket.readAll());

    if(answer==QByteArray("ack"))
    {
      legacyAck=true;
      break;
    }

    if(answer.size()>=4)
    {
      QDataStream frame(answer);
      quint32 frameLen=0;
      frame >> frameLen;

      if(answer.size()>=4+static_cast<int>(frameLen))
        frameComplete=true;
    }
  }

  if(legacyAck)
    return GUI_DELIVERED;

  if(!frameComplete)
  {
    error="GUI is running but did not respond in time (modal dialog open? sync in progress?). "
          "The change was NOT applied, safe to retry the same call later";
    return GUI_FAILED;
  }

  QDataStream frame(answer);
  quint32 frameLen=0;
  frame >> frameLen;
  const QByteArray body=answer.mid(4, frameLen);

  QJsonParseError parseError;
  QJsonDocument doc=QJsonDocument::fromJson(body, &parseError);

  if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
  {
    error="GUI sent a broken reply";
    return GUI_FAILED;
  }

  const QJsonObject response=doc.object();

  if(reply!=nullptr)
    *reply=response;

  if(response.value("ok").toBool(false))
    return GUI_APPLIED;

  error=response.value("error").toString();

  if(error.isEmpty())
    error="GUI refused the change";

  return GUI_REFUSED;
}

// Присутствие MCP для GUI-баннера: Hello при старте и при каждой записи
// (дешевый локальный IPC), Bye при чистом выходе. GUI держит TTL:
// убитый -9 сервер гаснет сам

// Присутствие MCP для GUI-баннера: Hello при старте и при каждой записи
// (дешевый локальный IPC), Bye при чистом выходе. GUI держит TTL:
// убитый -9 сервер гаснет сам
void notifyMcpPresence(const QString &binaryPath, McpMode mode,
                              const QString &command)
{
  QJsonObject payload;
  payload["pid"]=QString::number(QCoreApplication::applicationPid());
  payload["mode"]=mcpModeName(mode);

  QString error;

  sendGuiCommand(binaryPath, command, payload, error);
}

// История действий для GUI-окна: чтение и запись, итог каждого
// tools/call. Без ожидания ack (иначе каждое чтение висло бы под
// модальным диалогом GUI): сообщение ждёт обработки в буфере сокета.
// GUI нет - тихий пропуск

// История действий для GUI-окна: чтение и запись, итог каждого
// tools/call. Без ожидания ack (иначе каждое чтение висло бы под
// модальным диалогом GUI): сообщение ждёт обработки в буфере сокета.
// GUI нет - тихий пропуск
static void notifyMcpHistory(const QString &binaryPath,
                             const QString &toolName,
                             const QString &kind,
                             const QString &summary)
{
  QLocalSocket socket;

  bool connected=false;

  foreach(const QString &socketName, controlSocketCandidates(binaryPath))
  {
    socket.connectToServer(socketName);

    if(socket.waitForConnected(300))
    {
      connected=true;
      break;
    }
  }

  if(!connected)
    return;

  QJsonObject payload;
  payload["tool"]=toolName;
  payload["kind"]=kind;
  payload["summary"]=summary;

  QString message=QStringLiteral("mcpHistory ")+
                  QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));

  QByteArray uMsg=message.toUtf8();

  QDataStream ds(&socket);
  ds.writeBytes(uMsg.constData(), uMsg.size());
  socket.waitForBytesWritten(2000);
}

bool McpSession::handleMessage(const QByteArray &line, QByteArray &response)
  {
    QJsonParseError parseError;
    QJsonDocument doc=QJsonDocument::fromJson(line, &parseError);

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
    {
      response=makeResponse(QJsonValue(), QJsonValue(), errorObject(-32700, "Parse error"));
      return true;
    }

    QJsonObject request=doc.object();

    if(!request.contains("jsonrpc") || request.value("jsonrpc").toString()!="2.0" ||
       !request.contains("method") || !request.value("method").isString())
    {
      QJsonValue id=request.contains("id") ? request.value("id") : QJsonValue();
      response=makeResponse(id, QJsonValue(), errorObject(-32600, "Invalid Request"));
      return true;
    }

    QString method=request.value("method").toString();
    QJsonObject params=request.value("params").toObject();

    // Уведомление: ответа нет
    if(!request.contains("id"))
    {
      handleNotification(method, params);
      return false;
    }

    QJsonValue id=request.value("id");

    if(method=="initialize")
    {
      QString asked=params.value("protocolVersion").toString();

      QJsonObject capabilities;
      capabilities["tools"]=QJsonObject();

      QJsonObject serverInfo;
      serverInfo["name"]="mytetra";
      serverInfo["version"]=serverVersionString();

      QJsonObject result;
      result["protocolVersion"]=negotiateVersion(asked);
      result["capabilities"]=capabilities;
      result["serverInfo"]=serverInfo;

      response=makeResponse(id, result, QJsonValue());
      return true;
    }

    if(method=="ping")
    {
      response=makeResponse(id, QJsonObject(), QJsonValue());
      return true;
    }

    if(method=="tools/list")
    {
      QJsonObject result;
      result["tools"]=buildToolsList(m_mode);

      // Сколько тулов реально ушло клиенту и в каком режиме:
      // если нейронка клянется что чего-то нет, сверять сюда,
      // а не гадать. Урезание списка дальше - уже дело клиента
      fprintf(stderr, "mcp: tools/list served %d tools in %s mode\n",
              result["tools"].toArray().size(),
              qPrintable(mcpModeName(m_mode)));

      response=makeResponse(id, result, QJsonValue());
      return true;
    }

    if(method=="tools/call")
    {
      refreshModelIfNeeded(m_model, m_xmlPath, m_xmlMtime);

      QString toolName=params.value("name").toString();
      QJsonObject arguments=params.value("arguments").toObject();

      QJsonObject result;
      QString toolError;
      bool known=true;

      // История для GUI-окна: что спросили и чем кончилось.
      // Сводка это компактный JSON аргументов (до 120 символов)
      auto historyKind=[&]() -> QString {
        if(isWriteTool(toolName))
          return QStringLiteral("write");

        if(isKnownTool(toolName))
          return QStringLiteral("read");

        return QStringLiteral("other");
      };

      auto historyNotify=[&](const QString &outcome) {
        QString argStr=QString::fromUtf8(
          QJsonDocument(arguments).toJson(QJsonDocument::Compact));

        if(argStr.size()>120)
          argStr=argStr.left(120)+QStringLiteral("...");

        notifyMcpHistory(m_binaryPath, toolName, historyKind(),
                         argStr+QStringLiteral(" -> ")+outcome);
      };

      if(!toolAllowed(m_mode, toolName))
      {
        if(isKnownTool(toolName))
        {
          historyNotify(QStringLiteral("error: not enabled in ")+mcpModeName(m_mode));
          response=makeResponse(id, QJsonValue(),
                                errorObject(-32602, "Tool '"+toolName+
                                            "' is not enabled in "+
                                            mcpModeName(m_mode)+" mode"));
          return true;
        }

        known=false;
      }

      // Жив и пишу: GUI держит липкий баннер пока идут записи.
      // Чтения не пингуют (см. isWriteTool)
      if(known && isWriteTool(toolName))
        notifyMcpPresence(m_binaryPath, m_mode, "mcpHello");

      if(known && toolName=="list_branches")
      {
        result=m_tools.listBranches(arguments.value("recursive").toBool(false),
                                    arguments.value("branch_id").toString(),
                                    toolError);
      }
      else if(toolName=="list_notes")
      {
        bool recursive=false;

        if(arguments.contains("recursive"))
          recursive=arguments.value("recursive").toBool(false);

        int limit=McpTools::DEFAULT_LIST_LIMIT;

        if(arguments.contains("limit"))
          limit=arguments.value("limit").toInt(McpTools::DEFAULT_LIST_LIMIT);

        result=m_tools.listNotes(arguments.value("branch_id").toString(),
                                 recursive, limit, toolError, readFields(arguments));
      }
      else if(toolName=="get_note")
      {
        int maxChars=McpTools::DEFAULT_NOTE_CHARS;

        if(arguments.contains("max_chars"))
          maxChars=arguments.value("max_chars").toInt(McpTools::DEFAULT_NOTE_CHARS);

        result=m_tools.getNote(arguments.value("note_id").toString(),
                               maxChars, toolError, readFields(arguments));
      }
      else if(toolName=="search")
      {
        int limit=10;

        if(arguments.contains("limit"))
          limit=arguments.value("limit").toInt(10);

        result=m_tools.searchNotes(arguments.value("query").toString(), limit, toolError);
      }
      else if(toolName=="find_duplicates")
      {
        int limit=50;

        if(arguments.contains("limit"))
          limit=arguments.value("limit").toInt(50);

        result=m_tools.findDuplicates(limit);
      }
      else if(toolName=="stats")
      {
        result=m_tools.baseStats(arguments.value("detailed").toBool(false));
      }
      else if(toolName=="debug_info")
      {
        result=m_tools.debugInfo();
      }
      else if(toolName=="list_tags")
      {
        result=m_tools.listTags();
      }
      else if(toolName=="tag_notes")
      {
        int limit=100;

        if(arguments.contains("limit"))
          limit=arguments.value("limit").toInt(100);

        result=m_tools.tagNotes(arguments.value("tag").toString(), limit, toolError);
      }
      else if(toolName=="untagged_notes")
      {
        int limit=50;

        if(arguments.contains("limit"))
          limit=arguments.value("limit").toInt(50);

        result=m_tools.untaggedNotes(limit);
      }
      else if(toolName=="list_favorites")
      {
        result=m_tools.listFavorites();
      }
      else if(toolName=="list_attachments")
      {
        result=m_tools.listAttachments(arguments.value("note_id").toString(),
                                       toolError);
      }
      else if(toolName=="create_branch")
      {
        QString parentId=arguments.value("parent_id").toString();
        QString name=arguments.value("name").toString();

        result=writeBranch(freshBranchId(), parentId, name, toolError);
      }
      else if(toolName=="create_note")
      {
        QString branchId=arguments.value("branch_id").toString();
        QString name=arguments.value("name").toString();
        QString text=arguments.value("text").toString();
        QString format=arguments.value("format").toString();

        if(format.isEmpty())
          format="text";

        if(format!="text" && format!="html")
        {
          historyNotify(QStringLiteral("error: bad format, use text or html"));
          response=makeResponse(id, QJsonValue(), errorObject(-32602, "Bad format, use text or html"));
          return true;
        }

        QString html=(format=="html") ? text : McpTools::plainToHtml(text);

        result=writeNote(freshNoteId(), branchId, name, html,
                         arguments.value("tags").toString(), toolError);
      }
      else if(toolName=="update_note")
      {
        QString noteId=arguments.value("note_id").toString();

        bool hasName=arguments.contains("name");
        bool hasText=arguments.contains("text");
        bool hasTags=arguments.contains("tags");

        QString format=arguments.value("format").toString();

        if(format.isEmpty())
          format="text";

        if(hasText && format!="text" && format!="html")
        {
          historyNotify(QStringLiteral("error: bad format, use text or html"));
          response=makeResponse(id, QJsonValue(), errorObject(-32602, "Bad format, use text or html"));
          return true;
        }

        QString text=arguments.value("text").toString();
        QString html=(format=="html") ? text : McpTools::plainToHtml(text);

        result=writeNoteUpdate(noteId,
                               hasName, arguments.value("name").toString(),
                               hasText, html,
                               hasTags, arguments.value("tags").toString(),
                               toolError);
      }
      else if(toolName=="rename_branch")
      {
        result=writeRenameBranch(arguments.value("branch_id").toString(),
                                 arguments.value("name").toString(),
                                 toolError);
      }
      else if(toolName=="move_branch")
      {
        bool asChild=true;

        if(arguments.contains("as_child"))
          asChild=arguments.value("as_child").toBool(true);

        result=writeMoveBranch(arguments.value("branch_id").toString(),
                               arguments.value("target_id").toString(),
                               asChild, toolError);
      }
      else if(toolName=="move_note")
      {
        result=writeMoveNote(arguments.value("note_id").toString(),
                             arguments.value("branch_id").toString(),
                             toolError);
      }
      else if(toolName=="favorite_note")
      {
        result=writeFavoriteNote(arguments.value("note_id").toString(),
                                 true, toolError);
      }
      else if(toolName=="unfavorite_note")
      {
        result=writeFavoriteNote(arguments.value("note_id").toString(),
                                 false, toolError);
      }
      else if(toolName=="export_branch")
      {
        QString error;
        result=McpTools::applyExportBranch(&m_model,
                                           arguments.value("branch_id").toString(),
                                           arguments.value("path").toString(),
                                           error);

        if(!error.isEmpty())
          toolError=error;
      }
      else if(toolName=="import_branch")
      {
        result=writeImportBranch(arguments.value("parent_id").toString(),
                                 arguments.value("path").toString(),
                                 toolError);
      }
      else if(toolName=="rename_tag")
      {
        result=writeRenameTag(arguments.value("old").toString(),
                              arguments.value("new").toString(),
                              toolError);
      }
      else if(toolName=="delete_tag")
      {
        result=writeDeleteTag(arguments.value("tag").toString(), toolError);
      }
      else if(toolName=="delete_note")
      {
        result=writeDeleteNote(arguments.value("note_id").toString(), toolError);
      }      else if(toolName=="delete_branch")
      {
        result=writeDeleteBranch(arguments.value("branch_id").toString(), toolError);
      }
      else if(toolName=="apply_batch")
      {
        result=writeApplyBatch(arguments.value("operations").toArray(), toolError);
      }
      else
      {
        known=false;
      }

      if(!known)
      {
        historyNotify(QStringLiteral("error: unknown tool"));
        response=makeResponse(id, QJsonValue(), errorObject(-32602, "Unknown tool: "+toolName));
        return true;
      }

      if(!toolError.isEmpty())
      {
        historyNotify(QStringLiteral("error: ")+toolError);
        response=makeResponse(id, QJsonValue(), errorObject(-32602, toolError));
        return true;
      }

      {
        QString outcome=QStringLiteral("ok");

        if(result.contains(QStringLiteral("applied_via")))
          outcome+=QStringLiteral(" via ")+result.value(QStringLiteral("applied_via")).toString();

        historyNotify(outcome);
      }

      QJsonObject content;
      content["type"]="text";
      content["text"]=QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));

      QJsonArray contentList;
      contentList.append(content);

      QJsonObject callResult;
      callResult["content"]=contentList;

      response=makeResponse(id, callResult, QJsonValue());
      return true;
    }

    if(method.startsWith("notifications/"))
    {
      // Уведомление с id оформлено неверно, но клиента лучше
      // не подвешивать: пустой результат вместо Method not found
      response=makeResponse(id, QJsonObject(), QJsonValue());
      return true;
    }

    response=makeResponse(id, QJsonValue(), errorObject(-32601, "Method not found: "+method));
    return true;
  }

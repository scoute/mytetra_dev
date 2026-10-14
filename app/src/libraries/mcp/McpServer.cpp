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
#include <QLocalSocket>
#include <QDataStream>
#include <QRegularExpression>
#include <cstdio>

#include <unistd.h>

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
static const char *MCP_SUPPORTED_VERSIONS[]={
  "2024-11-05",
  "2025-03-26",
  "2025-06-18"
};

static const int MCP_SUPPORTED_VERSIONS_COUNT=3;
static const char *MCP_FALLBACK_VERSION="2024-11-05";


static QString serverVersionString(void)
{
  return QString("%1.%2.%3").arg(APPLICATION_RELEASE_VERSION)
                            .arg(APPLICATION_RELEASE_SUBVERSION)
                            .arg(APPLICATION_RELEASE_MICROVERSION);
}


static QString negotiateVersion(const QString &asked)
{
  for(int i=0; i<MCP_SUPPORTED_VERSIONS_COUNT; ++i)
    if(asked==MCP_SUPPORTED_VERSIONS[i])
      return asked;

  return MCP_FALLBACK_VERSION;
}


static QJsonObject errorObject(int code, const QString &message)
{
  QJsonObject error;
  error["code"]=code;
  error["message"]=message;

  return error;
}


static QJsonObject toolDescription(const QString &name,
                                   const QString &description,
                                   const QJsonObject &schema)
{
  QJsonObject tool;
  tool["name"]=name;
  tool["description"]=description;
  tool["inputSchema"]=schema;

  return tool;
}


static QJsonObject objectSchema(const QJsonObject &properties,
                                const QJsonArray &required)
{
  QJsonObject schema;
  schema["type"]="object";
  schema["properties"]=properties;

  if(!required.isEmpty())
    schema["required"]=required;

  schema["additionalProperties"]=false;

  return schema;
}


// Режимы сервера: read-only, read-write, read-write с удалением.
// Флаг --mcp оставлен для совместимости и означает read-only
enum class McpMode { ReadOnly=0, ReadWrite=1, ReadWriteDelete=2 };


static McpMode mcpModeFromArgs(int argc, char **argv)
{
  // Уровень числом: у scoped enum нет операторов сравнения
  int level=0;
  bool any=false;

  for(int i=1; i<argc; ++i)
  {
    QString arg=QString::fromLocal8Bit(argv[i]);

    if(arg=="--mcp-ro" || arg=="--mcp")
    {
      any=true;
      level=qMax(level, 0);
    }
    else if(arg=="--mcp-rw")
    {
      any=true;
      level=qMax(level, 1);
    }
    else if(arg=="--mcp-rw-with-delete")
    {
      any=true;
      level=qMax(level, 2);
    }
  }

  // Сюда попадают только при наличии флага (проверено в main),
  // дефолт read-only на всякий случай самый безопасный
  Q_UNUSED(any);

  if(level>=2)
    return McpMode::ReadWriteDelete;

  if(level>=1)
    return McpMode::ReadWrite;

  return McpMode::ReadOnly;
}


static QString mcpModeName(McpMode mode)
{
  if(mode==McpMode::ReadWriteDelete)
    return "--mcp-rw-with-delete";

  if(mode==McpMode::ReadWrite)
    return "--mcp-rw";

  return "--mcp-ro";
}


// Доступность инструмента в режиме. Чтение везде, запись кроме ro,
// удаление только в старшем режиме
static bool toolAllowed(McpMode mode, const QString &toolName)
{
  if(toolName=="list_branches" || toolName=="list_notes" ||
     toolName=="get_note" || toolName=="search" ||
     toolName=="find_duplicates" || toolName=="stats" ||
     toolName=="list_tags" || toolName=="tag_notes" ||
     toolName=="untagged_notes")
    return true;

  if(toolName=="create_branch" || toolName=="create_note" ||
     toolName=="update_note" ||
     toolName=="rename_branch" || toolName=="move_branch" ||
     toolName=="move_note" ||
     toolName=="rename_tag" || toolName=="delete_tag")
    return mode==McpMode::ReadWrite || mode==McpMode::ReadWriteDelete;

  if(toolName=="delete_note" || toolName=="delete_branch")
    return mode==McpMode::ReadWriteDelete;

  return false;
}


static bool isKnownTool(const QString &toolName)
{
  static const char *names[]={
    "list_branches", "list_notes", "get_note", "search",
    "find_duplicates", "stats", "list_tags", "tag_notes", "untagged_notes",
    "create_branch", "create_note", "update_note",
    "rename_branch", "move_branch", "move_note",
    "rename_tag", "delete_tag",
    "delete_note", "delete_branch"
  };

  for(unsigned i=0; i<sizeof(names)/sizeof(names[0]); ++i)
    if(toolName==names[i])
      return true;

  return false;
}


static QJsonArray buildToolsList(McpMode mode)
{
  QJsonArray tools;

  {
    QJsonObject properties;
    tools.append(toolDescription(
      "list_branches",
      "List all branches of the MyTetra knowledge tree in tree order. "
      "Read-only, reflects the last saved state.",
      objectSchema(properties, QJsonArray())));
  }
  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id from list_branches";
    properties["branch_id"]=branchId;
    QJsonObject recursive;
    recursive["type"]="boolean";
    recursive["description"]="Include notes from subbranches (default true)";
    recursive["default"]=true;
    properties["recursive"]=recursive;
    QJsonArray required;
    required.append("branch_id");
    tools.append(toolDescription(
      "list_notes",
      "List notes in a branch, optionally with subbranches. "
      "Read-only, reflects the last saved state.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id from list_notes or search";
    properties["note_id"]=noteId;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "get_note",
      "Read one note as plain text. Notes currently being edited in the GUI "
      "are visible only after they are saved there. Encrypted locked notes "
      "and missing texts return empty text.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject query;
    query["type"]="string";
    query["description"]="Case-insensitive substring matched against note names, tags and texts";
    properties["query"]=query;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Maximum number of hits (default 20, max 100)";
    limit["minimum"]=1;
    limit["maximum"]=100;
    limit["default"]=20;
    properties["limit"]=limit;
    QJsonArray required;
    required.append("query");
    tools.append(toolDescription(
      "search",
      "Search notes by name, tags and text. Read-only, reflects the last "
      "saved state.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject parentId;
    parentId["type"]="string";
    parentId["description"]="Parent branch id from list_branches, empty means top level";
    properties["parent_id"]=parentId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="Name of the new branch";
    properties["name"]=name;
    QJsonArray required;
    required.append("name");
    tools.append(toolDescription(
      "create_branch",
      "Create a new branch. Refused in global read-only mode and inside "
      "encrypted branches. If the GUI is running the change is applied "
      "through it, otherwise directly.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Target branch id from list_branches";
    properties["branch_id"]=branchId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="Note title";
    properties["name"]=name;
    QJsonObject text;
    text["type"]="string";
    text["description"]="Note body, plain text or HTML according to format (default empty)";
    text["default"]="";
    properties["text"]=text;
    QJsonObject format;
    format["type"]="string";
    format["enum"]=QJsonArray::fromStringList(QStringList() << "text" << "html");
    format["description"]="Body format (default text)";
    format["default"]="text";
    properties["format"]=format;
    QJsonObject tags;
    tags["type"]="string";
    tags["description"]="Comma-separated tags (default empty)";
    tags["default"]="";
    properties["tags"]=tags;
    QJsonArray required;
    required.append("branch_id");
    required.append("name");
    tools.append(toolDescription(
      "create_note",
      "Create a new note. Refused in global read-only mode and inside "
      "encrypted branches. If the GUI is running the change is applied "
      "through it, otherwise directly.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id from list_notes or search";
    properties["note_id"]=noteId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="New title (absent means keep)";
    properties["name"]=name;
    QJsonObject text;
    text["type"]="string";
    text["description"]="New body (absent means keep)";
    properties["text"]=text;
    QJsonObject format;
    format["type"]="string";
    format["enum"]=QJsonArray::fromStringList(QStringList() << "text" << "html");
    format["description"]="Body format, used only with text (default text)";
    format["default"]="text";
    properties["format"]=format;
    QJsonObject tags;
    tags["type"]="string";
    tags["description"]="New comma-separated tags, empty clears (absent means keep)";
    properties["tags"]=tags;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "update_note",
      "Update title, body and/or tags of a note. Only provided fields "
      "change. Refused in global read-only mode and for encrypted notes. "
      "If the note is currently open in the GUI editor, the editor keeps "
      "its buffer until reselected (last writer wins).",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id from list_notes or search";
    properties["note_id"]=noteId;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "delete_note",
      "Delete a note: its files move to trash, the XML entry is removed. "
      "There is no confirmation beyond this call: the --mcp-rw-with-delete "
      "flag IS the confirmation. Refused in other modes and in read-only mode.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id from list_branches, with all subbranches and notes";
    properties["branch_id"]=branchId;
    QJsonArray required;
    required.append("branch_id");
    tools.append(toolDescription(
      "delete_branch",
      "Delete a branch with all its subbranches and notes: files move to "
      "trash, entries are removed. Same confirmation rule as delete_note.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id from list_branches";
    properties["branch_id"]=branchId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="New branch name";
    properties["name"]=name;
    QJsonArray required;
    required.append("branch_id");
    required.append("name");
    tools.append(toolDescription(
      "rename_branch",
      "Rename a branch. Views in a running GUI refresh on branch reselection.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id to move";
    properties["branch_id"]=branchId;
    QJsonObject targetId;
    targetId["type"]="string";
    targetId["description"]="Target branch id (required)";
    properties["target_id"]=targetId;
    QJsonObject asChild;
    asChild["type"]="boolean";
    asChild["description"]="True: move inside target, false: place after target (default true)";
    asChild["default"]=true;
    properties["as_child"]=asChild;
    QJsonArray required;
    required.append("branch_id");
    required.append("target_id");
    tools.append(toolDescription(
      "move_branch",
      "Move a branch to another place in the tree. Cycles, self-move and "
      "the root are refused. Views in a running GUI refresh on reselection.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id from list_notes or search";
    properties["note_id"]=noteId;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Target branch id";
    properties["branch_id"]=branchId;
    QJsonArray required;
    required.append("note_id");
    required.append("branch_id");
    tools.append(toolDescription(
      "move_note",
      "Move a note to another branch without touching files: id and "
      "directory are kept, internal links stay valid. To the same branch "
      "and to/from encrypted branches is refused.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Maximum groups per list (default 50, max 200)";
    limit["minimum"]=1;
    limit["maximum"]=200;
    limit["default"]=50;
    properties["limit"]=limit;
    tools.append(toolDescription(
      "find_duplicates",
      "Groups of notes with the same name (case-insensitive) or the same "
      "text (sha1). Empty names and texts are skipped.",
      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    QJsonObject detailed;
    detailed["type"]="boolean";
    detailed["description"]="Include per-note breakdown rows (default false)";
    detailed["default"]=false;
    properties["detailed"]=detailed;
    tools.append(toolDescription(
      "stats",
      "Database totals as in the status bar: branches, records, tags, "
      "files, images, internal links. Optionally with per-note rows.",
      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    tools.append(toolDescription(
      "list_tags",
      "All tags with note counts, most popular first. Read-only.",
      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    QJsonObject tag;
    tag["type"]="string";
    tag["description"]="Tag name, exact whole-tag match, case-insensitive";
    properties["tag"]=tag;
    QJsonArray required;
    required.append("tag");
    tools.append(toolDescription(
      "tag_notes",
      "Notes carrying exactly this tag. Read-only.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Maximum notes (default 50, max 200)";
    limit["minimum"]=1;
    limit["maximum"]=200;
    limit["default"]=50;
    properties["limit"]=limit;
    tools.append(toolDescription(
      "untagged_notes",
      "Notes without any tags. Read-only, handy for cleanup.",
      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    QJsonObject oldTag;
    oldTag["type"]="string";
    oldTag["description"]="Existing tag name";
    properties["old"]=oldTag;
    QJsonObject newTag;
    newTag["type"]="string";
    newTag["description"]="New tag name (renaming to an existing one merges)";
    properties["new"]=newTag;
    QJsonArray required;
    required.append("old");
    required.append("new");
    tools.append(toolDescription(
      "rename_tag",
      "Rename a tag across all notes. Locked encrypted notes are skipped "
      "(see skipped_locked). Refused in read-only mode.",
      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject tag;
    tag["type"]="string";
    tag["description"]="Tag name to remove everywhere";
    properties["tag"]=tag;
    QJsonArray required;
    required.append("tag");
    tools.append(toolDescription(
      "delete_tag",
      "Remove a tag from all notes. Locked encrypted notes are skipped. "
      "Refused in read-only mode.",
      objectSchema(properties, required)));
  }

  // Режимы младше полного видят не все: фильтр по toolAllowed
  if(mode==McpMode::ReadWriteDelete)
    return tools;
  QJsonArray filtered;

  foreach(const QJsonValue &value, tools)
    if(toolAllowed(mode, value.toObject().value("name").toString()))
      filtered.append(value);

  return filtered;
}


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


enum GuiWriteState { GUI_NOT_RUNNING, GUI_DELIVERED, GUI_FAILED };


// Отправка команды живому GUI по control-каналу. Ack подтверждает
// доставку (обработчик GUI срабатывает сразу после), не применение:
// применение проверяется отдельно по mtime базы
static GuiWriteState sendGuiCommand(const QString &binaryPath,
                                    const QString &command,
                                    const QJsonObject &payload,
                                    QString &error)
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

  if(!socket.waitForReadyRead(8000))
  {
    error="GUI is running but did not acknowledge the command";
    return GUI_FAILED;
  }

  static const char *ack="ack";

  if(socket.read(qstrlen(ack))!=QByteArray(ack))
  {
    error="GUI is running but sent a broken acknowledgement";
    return GUI_FAILED;
  }

  return GUI_DELIVERED;
}


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
  bool handleMessage(const QByteArray &line, QByteArray &response)
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

      if(!toolAllowed(m_mode, toolName))
      {
        if(isKnownTool(toolName))
        {
          response=makeResponse(id, QJsonValue(),
                                errorObject(-32602, "Tool '"+toolName+
                                            "' is not enabled in "+
                                            mcpModeName(m_mode)+" mode"));
          return true;
        }

        known=false;
      }

      if(known && toolName=="list_branches")
      {
        result=m_tools.listBranches();
      }
      else if(toolName=="list_notes")
      {
        bool recursive=true;

        if(arguments.contains("recursive"))
          recursive=arguments.value("recursive").toBool(true);

        result=m_tools.listNotes(arguments.value("branch_id").toString(), recursive, toolError);
      }
      else if(toolName=="get_note")
      {
        result=m_tools.getNote(arguments.value("note_id").toString(), toolError);
      }
      else if(toolName=="search")
      {
        int limit=20;

        if(arguments.contains("limit"))
          limit=arguments.value("limit").toInt(20);

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
      else if(toolName=="list_tags")
      {
        result=m_tools.listTags();
      }
      else if(toolName=="tag_notes")
      {
        result=m_tools.tagNotes(arguments.value("tag").toString(), toolError);
      }
      else if(toolName=="untagged_notes")
      {
        int limit=50;

        if(arguments.contains("limit"))
          limit=arguments.value("limit").toInt(50);

        result=m_tools.untaggedNotes(limit);
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
      else
      {
        known=false;
      }

      if(!known)
      {
        response=makeResponse(id, QJsonValue(), errorObject(-32602, "Unknown tool: "+toolName));
        return true;
      }

      if(!toolError.isEmpty())
      {
        response=makeResponse(id, QJsonValue(), errorObject(-32602, toolError));
        return true;
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
  QJsonObject writeRenameBranch(const QString &id, const QString &name,
                                QString &toolError);
  QJsonObject writeMoveBranch(const QString &id, const QString &targetId,
                              bool asChild, QString &toolError);
  QJsonObject writeMoveNote(const QString &id, const QString &branchId,
                            QString &toolError);
  QJsonObject writeRenameTag(const QString &oldTag, const QString &newTag,
                             QString &toolError);
  QJsonObject writeDeleteTag(const QString &tag, QString &toolError);

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


// Ожидаемый родитель после move_branch: внутрь цели или соседство
static QString expectedMoveParent(KnowTreeModel &model,
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

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    TreeItem *item=m_model.getItemById(id);

    if(item==nullptr || item->getField("name")!=name.trimmed())
    {
      toolError="GUI did not apply the change in time, retry";

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

  if(state==GUI_DELIVERED)
  {
    QString expect=expectedMoveParent(m_model, targetId, asChild);

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
      toolError="GUI did not apply the change in time, retry";

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

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    QStringList path=m_model.getRecordPath(id);
    QString actual=path.isEmpty() ? QString() : path.last();

    if(actual!=branchId)
    {
      toolError="GUI did not apply the change in time, retry";

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


QJsonObject McpSession::writeRenameTag(const QString &oldTag,
                                       const QString &newTag,
                                       QString &toolError)
{
  QJsonObject result;

  if(mytetraConfig.get_readOnly())
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

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    if(McpTools::countTagNotes(&m_model, oldTag.trimmed().toLower())!=0)
    {
      toolError="GUI did not apply the change in time, retry";

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

  if(mytetraConfig.get_readOnly())
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

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    if(McpTools::countTagNotes(&m_model, tag.trimmed().toLower())!=0)
    {
      toolError="GUI did not apply the change in time, retry";

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

  if(mytetraConfig.get_readOnly())
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

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    if(!m_model.isItemIdExists(id))
    {
      toolError="GUI did not apply the change in time, retry";

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

  if(mytetraConfig.get_readOnly())
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

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    if(!m_model.isRecordIdExists(id))
    {
      toolError="GUI did not apply the change in time, retry";

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

  if(mytetraConfig.get_readOnly())
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

  if(state==GUI_DELIVERED)
  {
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
      toolError="GUI did not apply the change in time, retry";

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

  QJsonObject payload;
  payload["id"]=id;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpDeleteNote", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    if(m_model.isRecordIdExists(id))
    {
      toolError="GUI did not apply the change in time, retry";

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

  QJsonObject payload;
  payload["id"]=id;

  QString sendError;
  GuiWriteState state=sendGuiCommand(m_binaryPath, "mcpDeleteBranch", payload, sendError);

  if(state==GUI_FAILED)
  {
    toolError=sendError;

    return result;
  }

  if(state==GUI_DELIVERED)
  {
    waitGuiSave(5000);
    resyncModel();

    if(m_model.getItemById(id)!=nullptr)
    {
      toolError="GUI did not apply the change in time, retry";

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


// Pre-валидация базы до initFromXML: в headless-режиме диалоги
// XmlTree::load и criticalError формата роняют процесс,
// поэтому файл проверяется вручную с чистым выходом в stderr
static bool validateDatabase(const QString &xmlPath)
{
  QFile xmlFile(xmlPath);

  if(!xmlFile.open(QIODevice::ReadOnly))
  {
    fprintf(stderr, "mcp: can not open database file %s\n", xmlPath.toLocal8Bit().data());
    return false;
  }

  QByteArray xmlData=xmlFile.readAll();
  xmlFile.close();

  QDomDocument doc;
  QString errorText;
  int errorLine=0;
  int errorColumn=0;

  if(!doc.setContent(xmlData, true, &errorText, &errorLine, &errorColumn))
  {
    fprintf(stderr, "mcp: can not parse database file %s: %s at %d:%d\n",
            xmlPath.toLocal8Bit().data(), errorText.toLocal8Bit().data(),
            errorLine, errorColumn);
    return false;
  }

  QDomElement format=doc.documentElement().firstChildElement("format");
  int version=format.attribute("version", "0").toInt();
  int subversion=format.attribute("subversion", "0").toInt();

  // Та же проверка что KnowTreeModel::checkFormat
  if(version>CURRENT_FORMAT_VERSION || subversion>CURRENT_FORMAT_SUBVERSION)
  {
    fprintf(stderr, "mcp: unsupported database format %d.%d\n", version, subversion);
    return false;
  }

  return true;
}


int runMcpServer(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  globalParameters.setMainProgramFile(QString::fromLocal8Bit(argv[0]));

  // Headless boot: initHeadless never opens InstallDialog,
  // missing conf.ini is a clean exit instead of a crash
  if(!globalParameters.initHeadless())
  {
    fprintf(stderr, "mcp: conf.ini not found, can not locate MyTetra data\n");
    return 2;
  }

  mytetraConfig.init();
  dataBaseConfig.init();

  QString xmlPath=mytetraConfig.get_tetradir()+"/mytetra.xml";

  if(!validateDatabase(xmlPath))
    return 3;

  // Инициализация ГПСЧ для генерации id (сдвиг pid чтобы два
  // одновременно стартовавших сервера не дали одну серию rand)
  RandomInitter::init(static_cast<long>(QCoreApplication::applicationPid()));

  McpMode mode=mcpModeFromArgs(argc, argv);

  KnowTreeModel model;
  model.initFromXML(xmlPath);

  McpTools tools(&model);
  McpSession session(model, xmlPath, tools,
                     QString::fromLocal8Bit(argv[0]), mode);

  QFile stdIn;
  stdIn.open(stdin, QIODevice::ReadOnly);

  QFile stdOut;
  stdOut.open(stdout, QIODevice::WriteOnly);

  // Только ответы протокола уходят в stdout, диагностика - в stderr
  while(true)
  {
    QByteArray line=stdIn.readLine();

    if(line.isNull())
      break; // EOF: клиент закрыл канал

    line=line.trimmed();

    if(line.isEmpty())
      continue;

    QByteArray response;

    if(session.handleMessage(line, response))
    {
      stdOut.write(response+"\n");
      stdOut.flush();
    }
  }

  return 0;
}

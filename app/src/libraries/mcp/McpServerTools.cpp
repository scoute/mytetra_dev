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

// Версии протокола MCP которые понимает сервер.
// Запрошенная клиентом поддерживаемая версия эхомится назад,
// незнакомая сводится к самой старой стабильной

#include "McpServerPrivate.h"
#include "libraries/AppContext.h"

static const char *MCP_SUPPORTED_VERSIONS[]={
  "2024-11-05",
  "2025-03-26",
  "2025-06-18"
};

static const int MCP_SUPPORTED_VERSIONS_COUNT=3;
static const char *MCP_FALLBACK_VERSION="2024-11-05";

QString serverVersionString(void)
{
  return QString("%1.%2.%3").arg(APPLICATION_RELEASE_VERSION)
                            .arg(APPLICATION_RELEASE_SUBVERSION)
                            .arg(APPLICATION_RELEASE_MICROVERSION);
}

QString negotiateVersion(const QString &asked)
{
  for(int i=0; i<MCP_SUPPORTED_VERSIONS_COUNT; ++i)
    if(asked==MCP_SUPPORTED_VERSIONS[i])
      return asked;

  return MCP_FALLBACK_VERSION;
}

QJsonObject errorObject(int code, const QString &message)
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

McpMode mcpModeFromArgs(int argc, char **argv)
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

QString mcpModeName(McpMode mode)
{
  if(mode==McpMode::ReadWriteDelete)
    return "--mcp-rw-with-delete";

  if(mode==McpMode::ReadWrite)
    return "--mcp-rw";

  return "--mcp-ro";
}

// Доступность инструмента в режиме. Чтение везде, запись кроме ro,
// удаление только в старшем режиме

// Доступность инструмента в режиме. Чтение везде, запись кроме ro,
// удаление только в старшем режиме
bool toolAllowed(McpMode mode, const QString &toolName)
{
  if(toolName=="list_branches" || toolName=="list_notes" ||
     toolName=="get_note" || toolName=="search" ||
     toolName=="find_duplicates" || toolName=="stats" ||
     toolName=="debug_info" ||
     toolName=="list_tags" || toolName=="tag_notes" ||
     toolName=="untagged_notes" || toolName=="list_favorites" ||
     toolName=="list_attachments")
    return true;

  if(toolName=="apply_batch")
    return mode==McpMode::ReadWrite || mode==McpMode::ReadWriteDelete;

  if(toolName=="create_branch" || toolName=="create_note" ||
     toolName=="update_note" ||
     toolName=="rename_branch" || toolName=="move_branch" ||
     toolName=="move_note" || toolName=="favorite_note" ||
     toolName=="unfavorite_note" || toolName=="export_branch" ||
     toolName=="import_branch" ||
     toolName=="rename_tag")
    return mode==McpMode::ReadWrite || mode==McpMode::ReadWriteDelete;

  if(toolName=="delete_note" || toolName=="delete_branch" ||
     toolName=="delete_tag")
    return mode==McpMode::ReadWriteDelete;

  return false;
}

// Опциональный список ключей выдачи (get_note, list_notes).
// Пусто или нет поля = весь объект целиком

// Опциональный список ключей выдачи (get_note, list_notes).
// Пусто или нет поля = весь объект целиком
QStringList readFields(const QJsonObject &arguments)
{
  QStringList fields;

  foreach(const QJsonValue &value, arguments.value("fields").toArray())
  {
    QString key=value.toString().trimmed();

    if(!key.isEmpty() && !fields.contains(key))
      fields.append(key);
  }

  return fields;
}

bool isKnownTool(const QString &toolName)
{
  static const char *names[]={
    "list_branches", "list_notes", "get_note", "search",
    "find_duplicates", "stats", "debug_info", "list_tags", "tag_notes", "untagged_notes",
    "list_favorites", "list_attachments",
    "create_branch", "create_note", "update_note",
    "rename_branch", "move_branch", "move_note",
    "favorite_note", "unfavorite_note",
    "export_branch", "import_branch",
    "apply_batch",
    "rename_tag", "delete_tag",
    "delete_note", "delete_branch"
  };

  for(unsigned i=0; i<sizeof(names)/sizeof(names[0]); ++i)
    if(toolName==names[i])
      return true;

  return false;
}

// Пишущий инструмент (для присутствия: чтения GUI не дёргают,
// иначе каждое чтение ждало бы ack event loop и висло под модалкой)

// Пишущий инструмент (для присутствия: чтения GUI не дёргают,
// иначе каждое чтение ждало бы ack event loop и висло под модалкой)
bool isWriteTool(const QString &toolName)
{
  return toolName=="create_branch" || toolName=="create_note" ||
         toolName=="update_note" ||
         toolName=="rename_branch" || toolName=="move_branch" ||
         toolName=="move_note" ||
         toolName=="favorite_note" || toolName=="unfavorite_note" ||
         toolName=="export_branch" || toolName=="import_branch" ||
         toolName=="apply_batch" ||
         toolName=="rename_tag" || toolName=="delete_tag" ||
         toolName=="delete_note" || toolName=="delete_branch";
}

QJsonArray buildToolsList(McpMode mode)
{
  QJsonArray tools;

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Parent id: direct children of this branch (default: top level)";
    properties["branch_id"]=branchId;
    QJsonObject recursive;
    recursive["type"]="boolean";
    recursive["description"]="Full subtree walk (default false: one level only)";
    recursive["default"]=false;
    properties["recursive"]=recursive;
    tools.append(toolDescription(
      "list_branches",
      QObject::tr(
      "Top-level branches with notes_count (own) and "
      "notes_total (with subbranches). Encrypted ones carry encrypted:true. "
      "Pass recursive:true for the full tree, "
      "branch_id for children of one branch."
      ),      objectSchema(properties, QJsonArray())));
  }
  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id";
    properties["branch_id"]=branchId;
    QJsonObject recursive;
    recursive["type"]="boolean";
    recursive["description"]="With subbranches (default false)";
    recursive["default"]=false;
    properties["recursive"]=recursive;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Max notes (default 50, max 200)";
    limit["minimum"]=1;
    limit["maximum"]=200;
    limit["default"]=50;
    properties["limit"]=limit;
    QJsonObject fields;
    fields["type"]="array";
    fields["description"]="Note keys to return (default all, unknown keys are an error): id, name, branch_id, branch_path, tags, encrypted, author, url, ctime, attach_count. No text here, use get_note for it.";
    QJsonObject fieldItems;
    fieldItems["type"]="string";
    fields["items"]=fieldItems;
    properties["fields"]=fields;
    QJsonArray required;
    required.append("branch_id");
    tools.append(toolDescription(
      "list_notes",
      QObject::tr(
      "Notes in a branch WITHOUT texts (id, name, tags, ...). "
      "To read a note text call get_note with its note_id. "
      "Encrypted ones carry encrypted:true."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id";
    properties["note_id"]=noteId;
    QJsonObject maxChars;
    maxChars["type"]="integer";
    maxChars["description"]="Max text chars (default 20000)";
    maxChars["minimum"]=1;
    maxChars["default"]=20000;
    properties["max_chars"]=maxChars;
    QJsonObject fields;
    fields["type"]="array";
    fields["description"]="Note keys to return (default all, unknown keys are an error): id, name, branch_id, branch_path, tags, encrypted, author, url, ctime, attach_count, format, text, truncated";
    QJsonObject fieldItems;
    fieldItems["type"]="string";
    fields["items"]=fieldItems;
    properties["fields"]=fields;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "get_note",
      QObject::tr(
      "One note as plain text. Unsaved GUI edits invisible, locked notes read empty. Encrypted notes carry encrypted:true."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject query;
    query["type"]="string";
    query["description"]="Case-insensitive substring";
    properties["query"]=query;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Max hits (default 10, max 100)";
    limit["minimum"]=1;
    limit["maximum"]=100;
    limit["default"]=10;
    properties["limit"]=limit;
    QJsonArray required;
    required.append("query");
    tools.append(toolDescription(
      "search",
      QObject::tr(
      "Substring search over note names, tags, texts. "
      "Matching branches go in a separate branches list, no need to list all branches. "
      "Hits carry snippets only, call get_note with note_id to read the full text."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject parentId;
    parentId["type"]="string";
    parentId["description"]="Parent id, empty = top level";
    properties["parent_id"]=parentId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="Name";
    properties["name"]=name;
    QJsonArray required;
    required.append("name");
    tools.append(toolDescription(
      "create_branch",
      QObject::tr(
      "Create a branch. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id";
    properties["branch_id"]=branchId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="Title, free-form, no length cap (80+ chars is fine; "
      "a narrow desktop column may visually clip it, the stored name stays intact)";
    properties["name"]=name;
    QJsonObject text;
    text["type"]="string";
    text["description"]="Body (default empty)";
    text["default"]="";
    properties["text"]=text;
    QJsonObject format;
    format["type"]="string";
    format["enum"]=QJsonArray::fromStringList(QStringList() << "text" << "html");
    format["description"]="text or html (default text)";
    format["default"]="text";
    properties["format"]=format;
    QJsonObject tags;
    tags["type"]="string";
    tags["description"]="Tags, comma-separated";
    tags["default"]="";
    properties["tags"]=tags;
    QJsonArray required;
    required.append("branch_id");
    required.append("name");
    tools.append(toolDescription(
      "create_note",
      QObject::tr(
      "Create a note. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id";
    properties["note_id"]=noteId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="New title, free-form, no length cap (80+ chars is fine; "
      "a narrow desktop column may visually clip it, the stored name stays intact)";
    properties["name"]=name;
    QJsonObject text;
    text["type"]="string";
    text["description"]="New body";
    properties["text"]=text;
    QJsonObject format;
    format["type"]="string";
    format["enum"]=QJsonArray::fromStringList(QStringList() << "text" << "html");
    format["description"]="text or html";
    format["default"]="text";
    properties["format"]=format;
    QJsonObject tags;
    tags["type"]="string";
    tags["description"]="New tags, empty clears";
    properties["tags"]=tags;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "update_note",
      QObject::tr(
      "Update note fields. Only provided fields change; open GUI editor keeps its buffer. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id";
    properties["note_id"]=noteId;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "delete_note",
      QObject::tr(
      "Delete a note to trash. No confirmation beyond this call. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id with subtree";
    properties["branch_id"]=branchId;
    QJsonArray required;
    required.append("branch_id");
    tools.append(toolDescription(
      "delete_branch",
      QObject::tr(
      "Delete a branch subtree to trash. No confirmation beyond this call. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject operations;
    operations["type"]="array";
    operations["description"]="Operations in order: {tool, arguments}. "
      "Same argument shapes as single tools; creates get server ids. "
      "No intra-batch references: later ops can not use ids created "
      "earlier in the same batch. import/export unsupported.";
    operations["maxItems"]=200;
    properties["operations"]=operations;
    QJsonArray required;
    required.append("operations");
    tools.append(toolDescription(
      "apply_batch",
      QObject::tr(
      "PREFERRED for any 2+ writes: one call, one GUI roundtrip, one save. "
      "Do not call single-write tools in a loop. "
      "Example: {\"operations\": [{\"tool\": \"create_note\", \"arguments\": "
      "{\"branch_id\": \"abc123\", \"name\": \"Shopping\", \"text\": \"milk, bread\"}}, "
      "{\"tool\": \"rename_tag\", \"arguments\": {\"old\": \"todo\", \"new\": \"done\"}}]}. "
      "Batchable: create/update/delete/move/favorite note, create/rename/move/delete branch, "
      "rename/delete tag (no export/import). "
      "Creates get server ids, later ops can not reference them. Deletes need delete mode. "
      "Not atomic: stops at first error, applied prefix persists. "
      "Per-operation results in order."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id";
    properties["branch_id"]=branchId;
    QJsonObject name;
    name["type"]="string";
    name["description"]="New name";
    properties["name"]=name;
    QJsonArray required;
    required.append("branch_id");
    required.append("name");
    tools.append(toolDescription(
      "rename_branch",
      QObject::tr(
      "Rename a branch. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id to move";
    properties["branch_id"]=branchId;
    QJsonObject targetId;
    targetId["type"]="string";
    targetId["description"]="Target id";
    properties["target_id"]=targetId;
    QJsonObject asChild;
    asChild["type"]="boolean";
    asChild["description"]="Inside target, else after (default true)";
    asChild["default"]=true;
    properties["as_child"]=asChild;
    QJsonArray required;
    required.append("branch_id");
    required.append("target_id");
    tools.append(toolDescription(
      "move_branch",
      QObject::tr(
      "Move a branch. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id";
    properties["note_id"]=noteId;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Target id";
    properties["branch_id"]=branchId;
    QJsonArray required;
    required.append("note_id");
    required.append("branch_id");
    tools.append(toolDescription(
      "move_note",
      QObject::tr(
      "Move a note, id and links kept. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id";
    properties["note_id"]=noteId;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "favorite_note",
      QObject::tr(
      "Star a note. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id";
    properties["note_id"]=noteId;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "unfavorite_note",
      QObject::tr(
      "Unstar a note. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject branchId;
    branchId["type"]="string";
    branchId["description"]="Branch id with subtree";
    properties["branch_id"]=branchId;
    QJsonObject path;
    path["type"]="string";
    path["description"]="Target directory, created if missing";
    properties["path"]=path;
    QJsonArray required;
    required.append("branch_id");
    required.append("path");
    tools.append(toolDescription(
      "export_branch",
      QObject::tr(
      "Export a branch subtree to a directory in MyTetra exchange format. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject parentId;
    parentId["type"]="string";
    parentId["description"]="Parent id, empty = top level";
    properties["parent_id"]=parentId;
    QJsonObject path;
    path["type"]="string";
    path["description"]="Exchange directory with mytetra.xml";
    properties["path"]=path;
    QJsonArray required;
    required.append("path");
    tools.append(toolDescription(
      "import_branch",
      QObject::tr(
      "Import a branch subtree from a MyTetra exchange directory. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Max groups (default 50)";
    limit["minimum"]=1;
    limit["maximum"]=200;
    limit["default"]=50;
    properties["limit"]=limit;
    tools.append(toolDescription(
      "find_duplicates",
      QObject::tr(
      "Same-name or same-text note groups."
      ),      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    QJsonObject detailed;
    detailed["type"]="boolean";
    detailed["description"]="With per-note rows";
    detailed["default"]=false;
    properties["detailed"]=detailed;
    tools.append(toolDescription(
      "stats",
      QObject::tr(
      "Totals: branches, notes, tags, files, images, links."
      ),      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    tools.append(toolDescription(
      "debug_info",
      QObject::tr(
      "Server debug: app version and database path."
      ),      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    tools.append(toolDescription(
      "list_tags",
      QObject::tr(
      "Tags with counts."
      ),      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    QJsonObject tag;
    tag["type"]="string";
    tag["description"]="Tag, exact match";
    properties["tag"]=tag;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Max notes (default 100, max 200)";
    limit["minimum"]=1;
    limit["maximum"]=200;
    limit["default"]=100;
    properties["limit"]=limit;
    QJsonArray required;
    required.append("tag");
    tools.append(toolDescription(
      "tag_notes",
      QObject::tr(
      "Notes with exactly this tag."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject limit;
    limit["type"]="integer";
    limit["description"]="Max notes (default 50)";
    limit["minimum"]=1;
    limit["maximum"]=200;
    limit["default"]=50;
    properties["limit"]=limit;
    tools.append(toolDescription(
      "untagged_notes",
      QObject::tr(
      "Notes without tags."
      ),      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    tools.append(toolDescription(
      "list_favorites",
      QObject::tr(
      "Starred notes with branch context."
      ),      objectSchema(properties, QJsonArray())));
  }

  {
    QJsonObject properties;
    QJsonObject noteId;
    noteId["type"]="string";
    noteId["description"]="Note id";
    properties["note_id"]=noteId;
    QJsonArray required;
    required.append("note_id");
    tools.append(toolDescription(
      "list_attachments",
      QObject::tr(
      "Attachments of a note: file names with sizes."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject oldTag;
    oldTag["type"]="string";
    oldTag["description"]="Old tag";
    properties["old"]=oldTag;
    QJsonObject newTag;
    newTag["type"]="string";
    newTag["description"]="New tag (existing merges)";
    properties["new"]=newTag;
    QJsonArray required;
    required.append("old");
    required.append("new");
    tools.append(toolDescription(
      "rename_tag",
      QObject::tr(
      "Rename a tag everywhere. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
  }

  {
    QJsonObject properties;
    QJsonObject tag;
    tag["type"]="string";
    tag["description"]="Tag";
    properties["tag"]=tag;
    QJsonArray required;
    required.append("tag");
    tools.append(toolDescription(
      "delete_tag",
      QObject::tr(
      "Remove a tag everywhere. Irreversible. For 2+ operations in a row use apply_batch instead of separate calls."
      ),      objectSchema(properties, required)));
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

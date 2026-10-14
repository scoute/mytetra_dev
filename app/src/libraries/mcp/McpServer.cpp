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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdio>

#include "McpServer.h"
#include "McpTools.h"
#include "main.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/tree/KnowTreeModel.h"

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


static QJsonArray buildToolsList(void)
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

  return tools;
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


class McpSession
{
public:
  McpSession(KnowTreeModel &model, const QString &xmlPath, McpTools &tools)
    : m_model(model), m_xmlPath(xmlPath), m_tools(tools)
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
      result["tools"]=buildToolsList();

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

      if(toolName=="list_branches")
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
  QDateTime m_xmlMtime;

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

  KnowTreeModel model;
  model.initFromXML(xmlPath);

  McpTools tools(&model);
  McpSession session(model, xmlPath, tools);

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

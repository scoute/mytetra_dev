#include "McpServer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QModelIndex>
#include <QTextStream>

#include "main.h"

#if defined(__has_include)
#  if __has_include("buildinfo/buildinfo.h")
#    include "buildinfo/buildinfo.h"
#  endif
#endif

#include "libraries/GlobalParameters.h"
#include "libraries/RandomInitter.h"
#include "libraries/TagSuggester.h"
#include "libraries/TextDiff.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/qtSingleApplication5/qtsingleapplication.h"
#include "models/appConfig/AppConfig.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/subscription/SubscriptionRegistry.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"

#include <QSet>

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;
extern DataBaseConfig dataBaseConfig;
extern SubscriptionRegistry subscriptionRegistry;


// Максимальный размер текста заметки в ответах (защита от огромных выдач)
static const int MaxTextChars=200000;

// Лимит результатов поиска по умолчанию
static const int DefaultSearchLimit=20;

// Пишущие инструменты (объявление; определение ниже, рядом со схемами)
static bool isWriteTool(const QString &name);


int runMcpMode(QtSingleApplication &app)
{
    QTextStream errStream(stderr);

    // Один писатель: параллельная запись GUI-экземпляра и MCP-процесса
    // в whole-file mytetra.xml несовместима. Чтение при запущенном GUI
    // тоже запрещаем (модель GUI держит несохранённое состояние в памяти —
    // MCP увидел бы устаревшие данные с диска)
    if(app.isRunning())
    {
        errStream << "mcp: another MyTetra instance is running. "
                     "Close the GUI before starting MCP mode.\n";
        errStream.flush();
        return 5;
    }

    QString dbPathOverride;
    const QStringList args=app.arguments();
    const int dbIndex=args.indexOf("--db-path");
    if(dbIndex>=0)
    {
        if(dbIndex+1>=args.size())
        {
            errStream << "mcp: --db-path requires a directory argument\n";
            errStream.flush();
            return 2;
        }
        dbPathOverride=args.at(dbIndex+1);
    }

    globalParameters.init();

    // Генератор случайных чисел: в GUI сидируется позицией курсора
    // (MainWindow), в headless курсора нет — сидируем PID+миллисекундами.
    // Без этого rand() идёт с дефолтным сидом: ID/имена корзины повторяются
    // между запусками в одну секунду (коллизии, затирание данных).
    // Вызывать ДО mytetraConfig.init(): стартовые копии конфигов в корзину
    // тоже именуются через getUniqueId()
    RandomInitter::init(static_cast<long>(QCoreApplication::applicationPid())
                        + static_cast<long>(QDateTime::currentMSecsSinceEpoch()));

    mytetraConfig.init();

    // Переопределение БД действует только в памяти (sync не вызывается,
    // перед выходом значение возвращается обратно)
    const QString originalTetradir=mytetraConfig.get_tetradir();
    QString tetradir=originalTetradir;
    if(!dbPathOverride.isEmpty())
    {
        if(!QFileInfo::exists(dbPathOverride+"/mytetra.xml")
           || !QFileInfo::exists(dbPathOverride+"/database.ini")
           || !QFileInfo::exists(dbPathOverride+"/base"))
        {
            errStream << "mcp: not a MyTetra database directory: " << dbPathOverride << "\n";
            errStream.flush();
            return 6;
        }
        mytetraConfig.set_tetradir(dbPathOverride);
        tetradir=dbPathOverride;
    }

    dataBaseConfig.init();

    // Реестр подписок — для гигиены карт при удалении/перемещении
    // (файл создаётся только при первой реальной записи)
    subscriptionRegistry.init();

    // Безопасный дефолт: запись только с явным --mcp-rw.
    // Голый --mcp и --mcp-ro — read-only; при противоречии (--mcp-ro --mcp-rw)
    // побеждает безопасный вариант
    const bool readOnly=!app.arguments().contains("--mcp-rw")
                         || app.arguments().contains("--mcp-ro");
    McpServer server(tetradir, readOnly);
    QString initError;
    if(!server.init(&initError))
    {
        errStream << "mcp: " << initError << "\n";
        errStream.flush();
        mytetraConfig.set_tetradir(originalTetradir);
        mytetraConfig.syncAndDisableExitSync();
        dataBaseConfig.syncAndDisableExitSync();
        return 6;
    }

    const int exitCode=server.run();

    // Возврат переопределения и досрочная синхронизация конфигов:
    // запись из глобальных деструкторов небезопасна (мёртвый QTextCodec),
    // а --db-path на диск попадать не должен
    mytetraConfig.set_tetradir(originalTetradir);
    mytetraConfig.syncAndDisableExitSync();
    dataBaseConfig.syncAndDisableExitSync();
    return exitCode;
}


McpServer::McpServer(const QString &tetradir, bool readOnly)
    : tetradir(tetradir),
      readOnly(readOnly)
{
    model=new KnowTreeModel();
}


bool McpServer::init(QString *errorText)
{
    if(!QFileInfo::exists(tetradir+"/mytetra.xml"))
    {
        if(errorText)
            *errorText=QStringLiteral("mytetra.xml not found in ")+tetradir;
        return false;
    }

    model->initFromXML(tetradir+"/mytetra.xml");
    return true;
}


int McpServer::run(void)
{
    QFile inputFile;
    inputFile.open(stdin, QIODevice::ReadOnly);
    QTextStream outputStream(stdout);
    QTextStream errStream(stderr);

    while(!inputFile.atEnd())
    {
        const QByteArray line=inputFile.readLine().trimmed();
        if(line.isEmpty())
            continue;

        QJsonParseError parseError;
        const QJsonDocument doc=QJsonDocument::fromJson(line, &parseError);
        if(parseError.error!=QJsonParseError::NoError)
        {
            QJsonObject errorResponse;
            errorResponse["jsonrpc"]=QStringLiteral("2.0");
            errorResponse["id"]=QJsonValue::Null;
            QJsonObject errorObject;
            errorObject["code"]=-32700;
            errorObject["message"]=QStringLiteral("Parse error");
            errorResponse["error"]=errorObject;
            outputStream << QString::fromUtf8(QJsonDocument(errorResponse).toJson(QJsonDocument::Compact)) << "\n";
            outputStream.flush();
            continue;
        }

        // Одиночное сообщение либо пакет (JSON-массив): элементы обрабатываются
        // по одному, ответы уходят сразу; уведомления ответов не требуют
        if(doc.isArray())
        {
            const QJsonArray batch=doc.array();
            for(const QJsonValue &element : batch)
            {
                if(element.isObject())
                    handleMessage(element.toObject());
            }
        }
        else if(doc.isObject())
            handleMessage(doc.object());

        outputStream.flush();
    }

    errStream << "mcp: stdin closed, exiting\n";
    errStream.flush();
    return 0;
}


QString McpServer::versionString(void)
{
    QString version=QStringLiteral("%1.%2.%3").arg(APPLICATION_RELEASE_VERSION)
                                              .arg(APPLICATION_RELEASE_SUBVERSION)
                                              .arg(APPLICATION_RELEASE_MICROVERSION);
#ifdef APP_GIT_HASH
    version+=QStringLiteral("+")+QString::fromLatin1(APP_GIT_HASH);
#endif
    return version;
}


void McpServer::handleMessage(const QJsonObject &message)
{
    handleRequest(message);
}


void McpServer::handleRequest(const QJsonObject &message)
{
    const QString method=message.value("method").toString();
    const QJsonValue id=message.value("id");
    const bool isNotification=id.isUndefined() || id.isNull();
    const QJsonObject params=message.value("params").toObject();

    if(method==QStringLiteral("initialize"))
    {
        QJsonObject result;
        result["protocolVersion"]=QStringLiteral("2024-11-05");
        QJsonObject capabilities;
        capabilities["tools"]=QJsonObject();
        result["capabilities"]=capabilities;
        QJsonObject serverInfo;
        serverInfo["name"]=QStringLiteral("mytetra");
        serverInfo["version"]=versionString();
        result["serverInfo"]=serverInfo;
        initialized=true;
        if(!isNotification)
            sendResult(id, result);
        return;
    }

    if(method.startsWith(QStringLiteral("notifications/")))
        return; // Уведомления без id: ответа не требуют (initialized и др.)

    if(method==QStringLiteral("ping"))
    {
        if(!isNotification)
            sendResult(id, QJsonObject());
        return;
    }

    if(method==QStringLiteral("tools/list"))
    {
        if(isNotification)
            return;
        QJsonObject result;
        result["tools"]=toolSchemas();
        sendResult(id, result);
        return;
    }

    if(method==QStringLiteral("tools/call"))
    {
        if(isNotification)
            return;

        // Свежая модель с диска перед каждым вызовом
        QString reloadError;
        if(!reloadModel(&reloadError))
        {
            sendError(id, -32603, reloadError);
            return;
        }

        const QString toolName=params.value("name").toString();
        const QJsonObject toolArgs=params.value("arguments").toObject();

        // Read-only режим: пишущие инструменты отклоняются явно
        // (в tools/list они вообще не показываются)
        if(readOnly && isWriteTool(toolName))
        {
            sendError(id, -32602, QStringLiteral("MCP server runs in read-only mode (--mcp-ro). "
                                                 "Restart without --mcp-ro for write tools."));
            return;
        }

        bool isError=false;
        const QJsonObject toolResult=callTool(toolName, toolArgs, &isError);

        if(toolResult.isEmpty() && !isError)
        {
            sendError(id, -32602, QStringLiteral("Unknown tool: ")+toolName);
            return;
        }

        QJsonObject result;
        result["content"]=QJsonArray({toolResult});
        if(isError)
            result["isError"]=true;
        sendResult(id, result);
        return;
    }

    if(!isNotification)
        sendError(id, -32601, QStringLiteral("Method not found: ")+method);
}


void McpServer::sendResult(const QJsonValue &id, const QJsonObject &result)
{
    QTextStream outputStream(stdout);
    QJsonObject response;
    response["jsonrpc"]=QStringLiteral("2.0");
    response["id"]=id;
    response["result"]=result;
    outputStream << QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) << "\n";
    outputStream.flush();
}


void McpServer::sendError(const QJsonValue &id, int code, const QString &messageText)
{
    QTextStream outputStream(stdout);
    QJsonObject response;
    response["jsonrpc"]=QStringLiteral("2.0");
    if(!id.isUndefined())
        response["id"]=id;
    else
        response["id"]=QJsonValue::Null;
    QJsonObject errorObject;
    errorObject["code"]=code;
    errorObject["message"]=messageText;
    response["error"]=errorObject;
    outputStream << QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)) << "\n";
    outputStream.flush();
}


static QJsonObject stringProperty(const QString &description)
{
    QJsonObject property;
    property["type"]=QStringLiteral("string");
    property["description"]=description;
    return property;
}


// Пишущие инструменты (скрываются и отклоняются в --mcp-ro)
static bool isWriteTool(const QString &name)
{
    static const QSet<QString> writeTools=QSet<QString>()
        << "create_note" << "update_note_text" << "set_tags"
        << "move_note" << "rename_branch" << "rename_note"
        << "delete_note" << "move_branch" << "delete_branch";
    return writeTools.contains(name);
}


QJsonArray McpServer::toolSchemas(void)
{
    QJsonArray tools;

    auto addTool=[&](const QString &name, const QString &description,
                     const QJsonObject &properties, const QStringList &required)
    {
        // В read-only режиме пишущие инструменты не рекламируются
        if(readOnly && isWriteTool(name))
            return;

        QJsonObject tool;
        tool["name"]=name;
        tool["description"]=description;
        QJsonObject schema;
        schema["type"]=QStringLiteral("object");
        schema["properties"]=properties;
        QJsonArray requiredArray;
        for(const QString &field : required)
            requiredArray.append(field);
        schema["required"]=requiredArray;
        tool["inputSchema"]=schema;
        tools.append(tool);
    };

    {
        QJsonObject props;
        addTool("list_branches",
                "List all branches (sections) of the MyTetra tree: id, name, note counts.",
                props, {});
    }
    {
        QJsonObject props;
        props["branch_id"]=stringProperty("Branch id (see list_branches).");
        addTool("list_records",
                "List notes (records) inside a branch: id, name, author, url, tags, creation time.",
                props, {"branch_id"});
    }
    {
        QJsonObject props;
        props["record_id"]=stringProperty("Record id (see list_records).");
        addTool("read_note",
                "Read a full note: metadata fields and the HTML text body.",
                props, {"record_id"});
    }
    {
        QJsonObject props;
        props["query"]=stringProperty("Case-insensitive substring to find in branch names, note fields and note texts.");
        props["limit"]=stringProperty("Maximum matches to return (default 20).");
        addTool("search",
                "Full-text search across the whole database. Returns matches with snippets.",
                props, {"query"});
    }
    {
        QJsonObject props;
        props["branch_id"]=stringProperty("Branch id where the note will be created.");
        props["name"]=stringProperty("Note title.");
        props["text"]=stringProperty("Note body (HTML or plain text, optional).");
        props["author"]=stringProperty("Author (optional).");
        props["tags"]=stringProperty("Comma-separated tags (optional).");
        props["url"]=stringProperty("URL (optional).");
        addTool("create_note",
                "Create a new note in a branch. Refused in encrypted branches (no password in MCP mode).",
                props, {"branch_id", "name"});
    }
    {
        QJsonObject props;
        props["record_id"]=stringProperty("Record id.");
        props["text"]=stringProperty("New note body (HTML or plain text). Replaces the previous body.");
        addTool("update_note_text",
                "Replace the text body of a note. Refused for encrypted notes.",
                props, {"record_id", "text"});
    }
    {
        QJsonObject props;
        props["record_id"]=stringProperty("Record id.");
        props["tags"]=stringProperty("New comma-separated tags (replaces previous tags).");
        addTool("set_tags",
                "Replace the tags of a note.",
                props, {"record_id", "tags"});
    }
    {
        QJsonObject props;
        props["record_id"]=stringProperty("Record id to suggest tags for.");
        props["top_n"]=stringProperty("How many suggestions to return (default 5).");
        addTool("suggest_tags",
                "Suggest tags for a note from the existing tag vocabulary, based on similar words in already tagged notes. Returns ranked tags with scores and matched words. Does not modify anything.",
                props, {"record_id"});
    }
    {
        QJsonObject props;
        props["record_id"]=stringProperty("Record id to move.");
        props["target_branch_id"]=stringProperty("Branch id to move the note into.");
        addTool("move_note",
                "Move a note to another branch (same id, files relocated, old directory goes to trash).",
                props, {"record_id", "target_branch_id"});
    }
    {
        QJsonObject props;
        props["branch_id"]=stringProperty("Branch id to rename.");
        props["name"]=stringProperty("New branch name.");
        addTool("rename_branch",
                "Rename a branch.",
                props, {"branch_id", "name"});
    }
    {
        QJsonObject props;
        props["record_id"]=stringProperty("Record id to rename.");
        props["name"]=stringProperty("New note title.");
        addTool("rename_note",
                "Rename a note (title only).",
                props, {"record_id", "name"});
    }
    {
        QJsonObject props;
        props["record_id"]=stringProperty("Record id to delete.");
        addTool("delete_note",
                "Delete a note: its files go to the trash directory. Ask the user for confirmation first — this cannot be undone from MCP.",
                props, {"record_id"});
    }
    {
        QJsonObject props;
        props["branch_id"]=stringProperty("Branch id to move.");
        props["new_parent_branch_id"]=stringProperty("Branch id of the new parent (\"0\" for top level).");
        addTool("move_branch",
                "Move a branch (with its subtree) under another branch. Refused on cycles.",
                props, {"branch_id", "new_parent_branch_id"});
    }
    {
        QJsonObject props;
        props["branch_id"]=stringProperty("Branch id to delete (the whole subtree).");
        addTool("delete_branch",
                "Delete a branch with its entire subtree: files go to the trash directory. Ask the user for confirmation first — this cannot be undone from MCP.",
                props, {"branch_id"});
    }
    {
        QJsonObject props;
        addTool("find_duplicates",
                "Audit: groups of notes with identical names or identical text content (by sha256). Read-only, changes nothing. Start here when tidying a large database.",
                props, {});
    }
    {
        QJsonObject props;
        addTool("find_empty_branches",
                "Audit: branches without notes (with or without sub-branches). Read-only.",
                props, {});
    }
    {
        QJsonObject props;
        props["limit"]=stringProperty("Maximum notes to return (default 200).");
        addTool("find_untagged_notes",
                "Audit: notes with empty tags. Read-only. Pair with suggest_tags to tidy them up.",
                props, {});
    }

    return tools;
}


QJsonObject McpServer::callTool(const QString &name, const QJsonObject &args, bool *isError)
{
    if(name==QStringLiteral("list_branches"))
        return toolListBranches(args, isError);
    if(name==QStringLiteral("list_records"))
        return toolListRecords(args, isError);
    if(name==QStringLiteral("read_note"))
        return toolReadNote(args, isError);
    if(name==QStringLiteral("search"))
        return toolSearch(args, isError);
    if(name==QStringLiteral("create_note"))
        return toolCreateNote(args, isError);
    if(name==QStringLiteral("update_note_text"))
        return toolUpdateNoteText(args, isError);
    if(name==QStringLiteral("set_tags"))
        return toolSetTags(args, isError);
    if(name==QStringLiteral("suggest_tags"))
        return toolSuggestTags(args, isError);
    if(name==QStringLiteral("move_note"))
        return toolMoveNote(args, isError);
    if(name==QStringLiteral("rename_branch"))
        return toolRenameBranch(args, isError);
    if(name==QStringLiteral("rename_note"))
        return toolRenameNote(args, isError);
    if(name==QStringLiteral("delete_note"))
        return toolDeleteNote(args, isError);
    if(name==QStringLiteral("move_branch"))
        return toolMoveBranch(args, isError);
    if(name==QStringLiteral("delete_branch"))
        return toolDeleteBranch(args, isError);
    if(name==QStringLiteral("find_duplicates"))
        return toolFindDuplicates(args, isError);
    if(name==QStringLiteral("find_empty_branches"))
        return toolFindEmptyBranches(args, isError);
    if(name==QStringLiteral("find_untagged_notes"))
        return toolFindUntaggedNotes(args, isError);

    return QJsonObject();
}


QJsonObject McpServer::textContent(const QString &text)
{
    QJsonObject content;
    content["type"]=QStringLiteral("text");
    content["text"]=text;
    return content;
}


QJsonObject McpServer::textError(const QString &text)
{
    // Ошибка инструмента возвращается через тот же textContent;
    // вызывающий код ставит isError=true
    return textContent(text);
}


bool McpServer::reloadModel(QString *errorText)
{
    if(!QFileInfo::exists(tetradir+"/mytetra.xml"))
    {
        if(errorText)
            *errorText=QStringLiteral("mytetra.xml disappeared from ")+tetradir;
        return false;
    }

    model->reload();
    return true;
}


bool McpServer::isBranchChainCrypt(TreeItem *branchItem)
{
    TreeItem *item=branchItem;
    while(item)
    {
        if(item->getField("crypt")=="1")
            return true;
        item=item->parent();
    }
    return false;
}


QString McpServer::readRecordTextFile(const QString &dir, const QString &file,
                                      bool *truncated)
{
    QString fileName=file.isEmpty() ? QStringLiteral("text.html") : file;
    QFile textFile(tetradir+"/base/"+dir+"/"+fileName);
    if(!textFile.open(QIODevice::ReadOnly))
        return QString();

    QByteArray content=textFile.readAll();
    textFile.close();

    QString text=QString::fromUtf8(content);
    if(truncated)
        *truncated=false;
    if(text.size()>MaxTextChars)
    {
        if(truncated)
            *truncated=true;
        text=text.left(MaxTextChars);
    }
    return text;
}


McpServer::RecordLocation McpServer::locateRecord(const QString &recordId)
{
    RecordLocation location;

    // Обход всего дерева (рекурсивно от корневых веток)
    QList<TreeItem*> stack;
    TreeItem *rootItem=model->getItem(QModelIndex());
    if(!rootItem)
        return location;
    for(int i=0; i<rootItem->childCount(); ++i)
        stack.append(rootItem->child(i));

    while(!stack.isEmpty())
    {
        TreeItem *item=stack.takeLast();
        RecordTableData *table=item->recordtableGetTableData();
        if(table)
        {
            const int pos=table->getPosById(recordId);
            if(pos>=0)
            {
                location.branchItem=item;
                location.table=table;
                location.pos=pos;
                return location;
            }
        }
        for(int i=0; i<item->childCount(); ++i)
            stack.append(item->child(i));
    }

    return location;
}


// Рекурсивный листинг веток: "  id name (N notes)"
static void appendBranchLines(KnowTreeModel *model, TreeItem *item, int depth, QStringList &lines)
{
    QString indent;
    for(int i=0; i<depth; ++i)
        indent+=QStringLiteral("  ");

    const int noteCount=item->recordtableGetRowCount();
    lines.append(QStringLiteral("%1- %2 %3 (%4 notes)").arg(indent)
                 .arg(item->getField("id"))
                 .arg(item->getField("name"))
                 .arg(noteCount));

    for(int i=0; i<item->childCount(); ++i)
        appendBranchLines(model, item->child(i), depth+1, lines);
}


QJsonObject McpServer::toolListBranches(const QJsonObject &args, bool *isError)
{
    Q_UNUSED(args)
    Q_UNUSED(isError)

    QStringList lines;
    TreeItem *rootItem=model->getItem(QModelIndex());
    if(rootItem)
    {
        for(int i=0; i<rootItem->childCount(); ++i)
            appendBranchLines(model, rootItem->child(i), 0, lines);
    }

    return textContent(lines.join("\n"));
}


QJsonObject McpServer::toolListRecords(const QJsonObject &args, bool *isError)
{
    const QString branchId=args.value("branch_id").toString();
    TreeItem *branchItem=model->getItemById(branchId);
    if(!branchItem)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Branch not found: ")+branchId);
    }

    RecordTableData *table=branchItem->recordtableGetTableData();
    QStringList lines;
    if(table)
    {
        for(unsigned int i=0; i<table->size(); ++i)
        {
            Record *record=table->getRecord(static_cast<int>(i));
            if(!record)
                continue;
            lines.append(QStringLiteral("- %1 | %2 | author=%3 | tags=%4 | ctime=%5 | url=%6")
                         .arg(record->getField("id"))
                         .arg(record->getField("name"))
                         .arg(record->getField("author"))
                         .arg(record->getField("tags"))
                         .arg(record->getField("ctime"))
                         .arg(record->getField("url")));
        }
    }

    if(lines.isEmpty())
        return textContent(QStringLiteral("Branch %1 (%2) has no notes.")
                           .arg(branchId).arg(branchItem->getField("name")));

    return textContent(QStringLiteral("Notes in branch %1 (%2):\n%3")
                       .arg(branchId).arg(branchItem->getField("name"))
                       .arg(lines.join("\n")));
}


QJsonObject McpServer::toolReadNote(const QJsonObject &args, bool *isError)
{
    const QString recordId=args.value("record_id").toString();
    const RecordLocation location=locateRecord(recordId);
    if(!location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    Record *record=location.table->getRecord(location.pos);
    if(!record)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    if(record->getField("crypt")=="1" || isBranchChainCrypt(location.branchItem))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record is encrypted; MCP mode has no password. Unlock it in the GUI."));
    }

    bool truncated=false;
    const QString text=readRecordTextFile(record->getField("dir"),
                                          record->getField("file"),
                                          &truncated);

    QStringList output;
    output << QStringLiteral("id: %1").arg(record->getField("id"));
    output << QStringLiteral("name: %1").arg(record->getField("name"));
    output << QStringLiteral("branch: %1 (%2)").arg(location.branchItem->getField("id"))
                                                .arg(location.branchItem->getField("name"));
    output << QStringLiteral("author: %1").arg(record->getField("author"));
    output << QStringLiteral("url: %1").arg(record->getField("url"));
    output << QStringLiteral("tags: %1").arg(record->getField("tags"));
    output << QStringLiteral("ctime: %1").arg(record->getField("ctime"));
    output << QStringLiteral("text:");
    output << text;
    if(truncated)
        output << QStringLiteral("\n[... truncated at %1 chars ...]").arg(MaxTextChars);

    return textContent(output.join("\n"));
}


QJsonObject McpServer::toolSearch(const QJsonObject &args, bool *isError)
{
    Q_UNUSED(isError)

    const QString query=args.value("query").toString();
    if(query.isEmpty())
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Empty query."));
    }

    int limit=DefaultSearchLimit;
    if(args.contains("limit"))
    {
        bool ok=false;
        const int parsed=args.value("limit").toString().toInt(&ok);
        if(ok && parsed>0 && parsed<=200)
            limit=parsed;
    }

    QStringList hits;

    // Обход дерева: имена веток, поля записей, затем тексты
    QList<TreeItem*> stack;
    TreeItem *rootItem=model->getItem(QModelIndex());
    if(rootItem)
    {
        for(int i=0; i<rootItem->childCount(); ++i)
            stack.append(rootItem->child(i));
    }

    while(!stack.isEmpty() && hits.size()<limit)
    {
        TreeItem *item=stack.takeLast();

        if(item->getField("name").contains(query, Qt::CaseInsensitive))
        {
            hits.append(QStringLiteral("[branch] %1 : %2")
                        .arg(item->getField("id")).arg(item->getField("name")));
        }

        RecordTableData *table=item->recordtableGetTableData();
        if(table)
        {
            for(unsigned int i=0; i<table->size() && hits.size()<limit; ++i)
            {
                Record *record=table->getRecord(static_cast<int>(i));
                if(!record)
                    continue;

                const QString name=record->getField("name");
                const QString author=record->getField("author");
                const QString tags=record->getField("tags");
                const QString url=record->getField("url");

                QString snippet;
                if(name.contains(query, Qt::CaseInsensitive))
                    snippet=QStringLiteral("name: ")+name;
                else if(author.contains(query, Qt::CaseInsensitive))
                    snippet=QStringLiteral("author: ")+author;
                else if(tags.contains(query, Qt::CaseInsensitive))
                    snippet=QStringLiteral("tags: ")+tags;
                else if(url.contains(query, Qt::CaseInsensitive))
                    snippet=QStringLiteral("url: ")+url;
                else
                {
                    // Тяжёлая проверка — текст с диска (шифрованные пропускаем)
                    if(record->getField("crypt")=="1")
                        continue;
                    const QString text=readRecordTextFile(record->getField("dir"),
                                                          record->getField("file"));
                    const int pos=text.indexOf(query, 0, Qt::CaseInsensitive);
                    if(pos<0)
                        continue;
                    QString context=text.mid(qMax(0, pos-100), 200);
                    context=context.simplified();
                    snippet=QStringLiteral("text: ...%1...").arg(context);
                }

                hits.append(QStringLiteral("[note] %1 : %2 (%3)")
                            .arg(record->getField("id")).arg(name).arg(snippet));
            }
        }

        for(int i=0; i<item->childCount() && hits.size()<limit; ++i)
            stack.append(item->child(i));
    }

    if(hits.isEmpty())
        return textContent(QStringLiteral("No matches for '%1'.").arg(query));

    QStringList output;
    output << QStringLiteral("Matches for '%1' (%2):").arg(query).arg(hits.size());
    output << hits;
    return textContent(output.join("\n"));
}


QJsonObject McpServer::toolCreateNote(const QJsonObject &args, bool *isError)
{
    const QString branchId=args.value("branch_id").toString();
    QString name=args.value("name").toString();
    if(name.isEmpty())
        name=QStringLiteral("New note");

    TreeItem *branchItem=model->getItemById(branchId);
    if(!branchItem)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Branch not found: ")+branchId);
    }

    if(isBranchChainCrypt(branchItem))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Branch is encrypted; MCP mode has no password. Unlock it in the GUI."));
    }

    Record record;
    record.switchToFat();
    record.setText(args.value("text").toString());
    record.setField("name", name);
    record.setField("author", args.value("author").toString());
    record.setField("url", args.value("url").toString());
    record.setField("tags", args.value("tags").toString());

    RecordTableData *table=branchItem->recordtableGetTableData();
    if(!table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Branch has no record table."));
    }

    const int pos=table->insertNewRecord(
        GlobalParameters::AddNewRecordBehavior::ADD_TO_END, 0, record);

    model->save();

    Record *created=table->getRecord(pos);
    const QString newId=created ? created->getField("id") : QString();
    const QString newDir=created ? created->getField("dir") : QString();

    return textContent(QStringLiteral("Created note %1 (dir %2) in branch %3.")
                       .arg(newId).arg(newDir).arg(branchId));
}


QJsonObject McpServer::toolUpdateNoteText(const QJsonObject &args, bool *isError)
{
    const QString recordId=args.value("record_id").toString();
    const QString newText=args.value("text").toString();

    const RecordLocation location=locateRecord(recordId);
    if(!location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    if(isBranchChainCrypt(location.branchItem))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record is encrypted; MCP mode has no password. Unlock it in the GUI."));
    }

    Record fatRecord=location.table->getRecordFat(location.pos);
    if(fatRecord.getField("crypt")=="1")
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record is encrypted; MCP mode has no password. Unlock it in the GUI."));
    }

    fatRecord.setText(newText);
    fatRecord.pushFatAttributes();

    // Формат БД не хранит mtime: сохранения mytetra.xml не требуется,
    // мусор в корзину не кладём
    return textContent(QStringLiteral("Updated text of note %1 (%2 bytes).")
                       .arg(recordId).arg(newText.toUtf8().size()));
}


QJsonObject McpServer::toolSetTags(const QJsonObject &args, bool *isError)
{
    const QString recordId=args.value("record_id").toString();
    const QString tags=args.value("tags").toString();

    const RecordLocation location=locateRecord(recordId);
    if(!location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    Record *record=location.table->getRecord(location.pos);
    if(!record)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    record->setField("tags", tags);
    model->save();

    return textContent(QStringLiteral("Tags of note %1 set to '%2'.").arg(recordId).arg(tags));
}


void McpServer::collectAllBranches(QList<TreeItem*> &branches)
{
    branches.clear();

    QList<TreeItem*> stack;
    TreeItem *rootItem=model->getItem(QModelIndex());
    if(rootItem)
    {
        for(int i=0; i<rootItem->childCount(); ++i)
            stack.append(rootItem->child(i));
    }

    while(!stack.isEmpty())
    {
        TreeItem *item=stack.takeLast();
        branches.append(item);
        for(int i=0; i<item->childCount(); ++i)
            stack.append(item->child(i));
    }
}


void McpServer::purgeLocalRecordFromSubscriptions(const QString &localRecordId)
{
    if(localRecordId.isEmpty())
        return;

    const QStringList keys=subscriptionRegistry.listSubscriptionKeys();
    for(const QString &key : keys)
    {
        SubscriptionRecord record=subscriptionRegistry.getSubscription(key);
        bool changed=false;

        QList<QString> recordKeys=record.recordsMap.keys();
        for(const QString &sharedId : recordKeys)
        {
            if(record.recordsMap.value(sharedId).first==localRecordId)
            {
                record.recordsMap.remove(sharedId);
                changed=true;
            }
        }

        if(changed)
            subscriptionRegistry.addOrUpdateSubscription(record);
    }
}


void McpServer::purgeLocalBranchFromSubscriptions(TreeItem *localSubtreeRoot)
{
    if(!localSubtreeRoot)
        return;

    QStringList localBranchIds;
    QStringList localRecordIds;

    QList<TreeItem*> stack;
    stack.append(localSubtreeRoot);
    while(!stack.isEmpty())
    {
        TreeItem *item=stack.takeLast();
        localBranchIds.append(item->getField("id"));

        RecordTableData *branchTable=item->recordtableGetTableData();
        if(branchTable)
        {
            for(unsigned int i=0; i<branchTable->size(); ++i)
            {
                Record *record=branchTable->getRecord(static_cast<int>(i));
                if(record)
                    localRecordIds.append(record->getField("id"));
            }
        }

        for(int i=0; i<item->childCount(); ++i)
            stack.append(item->child(i));
    }

    const QStringList keys=subscriptionRegistry.listSubscriptionKeys();
    for(const QString &key : keys)
    {
        SubscriptionRecord record=subscriptionRegistry.getSubscription(key);
        bool changed=false;

        QList<QString> branchKeys=record.branchesMap.keys();
        for(const QString &sharedId : branchKeys)
        {
            if(localBranchIds.contains(record.branchesMap.value(sharedId)))
            {
                record.branchesMap.remove(sharedId);
                changed=true;
            }
        }

        QList<QString> recordKeys=record.recordsMap.keys();
        for(const QString &sharedId : recordKeys)
        {
            if(localRecordIds.contains(record.recordsMap.value(sharedId).first))
            {
                record.recordsMap.remove(sharedId);
                changed=true;
            }
        }

        if(changed)
            subscriptionRegistry.addOrUpdateSubscription(record);
    }
}


QJsonObject McpServer::toolSuggestTags(const QJsonObject &args, bool *isError)
{
    const QString recordId=args.value("record_id").toString();
    int topN=5;
    if(args.contains("top_n"))
    {
        bool ok=false;
        const int parsed=args.value("top_n").toString().toInt(&ok);
        if(ok && parsed>0 && parsed<=20)
            topN=parsed;
    }

    // Профили по всем размеченным заметкам базы
    TagSuggester suggester;
    QList<TreeItem*> branches;
    collectAllBranches(branches);
    for(TreeItem *branch : branches)
    {
        if(isBranchChainCrypt(branch))
            continue;
        RecordTableData *table=branch->recordtableGetTableData();
        if(!table)
            continue;
        for(unsigned int i=0; i<table->size(); ++i)
        {
            Record *record=table->getRecord(static_cast<int>(i));
            if(!record)
                continue;
            const QString tags=record->getField("tags");
            if(tags.trimmed().isEmpty())
                continue;
            if(record->getField("crypt")=="1")
                continue;
            const QString body=readRecordTextFile(record->getField("dir"),
                                                  record->getField("file"));
            suggester.addDocument(tags.split(',', Qt::SkipEmptyParts),
                                  record->getField("name")+" "
                                  +TextDiff::htmlToTextLines(body).join(" "));
        }
    }

    if(suggester.documentCount()==0)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("No tagged notes to learn from."));
    }

    // Текст целевой заметки
    const RecordLocation location=locateRecord(recordId);
    if(!location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }
    Record *target=location.table->getRecord(location.pos);
    if(!target)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }
    const QString targetBody=readRecordTextFile(target->getField("dir"),
                                                target->getField("file"));
    const QList<TagSuggestion> suggestions=suggester.suggest(
        target->getField("name")+" "+TextDiff::htmlToTextLines(targetBody).join(" "), topN);

    if(suggestions.isEmpty())
        return textContent(QStringLiteral("No suitable tags found for note %1 "
                                           "(taught on %2 tagged notes). Ask the user.")
                           .arg(recordId).arg(suggester.documentCount()));

    QStringList lines;
    lines.append(QStringLiteral("Suggested tags for note %1 '%2' (taught on %3 tagged notes):")
                 .arg(recordId).arg(target->getField("name")).arg(suggester.documentCount()));
    for(const TagSuggestion &suggestion : suggestions)
    {
        lines.append(QStringLiteral("- %1 (score %2, matched: %3)")
                     .arg(suggestion.tag)
                     .arg(QString::number(suggestion.score, 'f', 3))
                     .arg(suggestion.matchedWords.join(", ")));
    }
    lines.append(QStringLiteral("Current tags: '%1'. Apply with set_tags (confirm with the user first).")
                 .arg(target->getField("tags")));
    return textContent(lines.join("\n"));
}


QJsonObject McpServer::toolMoveNote(const QJsonObject &args, bool *isError)
{
    const QString recordId=args.value("record_id").toString();
    const QString targetBranchId=args.value("target_branch_id").toString();

    const RecordLocation location=locateRecord(recordId);
    if(!location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    TreeItem *targetBranch=model->getItemById(targetBranchId);
    if(!targetBranch)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Target branch not found: ")+targetBranchId);
    }

    RecordTableData *targetTable=targetBranch->recordtableGetTableData();
    if(!targetTable)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Target branch has no record table."));
    }

    if(targetTable==location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Note is already in that branch."));
    }

    if(isBranchChainCrypt(location.branchItem) || isBranchChainCrypt(targetBranch))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Encrypted branches are not supported in MCP mode."));
    }

    Record *liteRecord=location.table->getRecord(location.pos);
    if(!liteRecord || liteRecord->getField("crypt")=="1")
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found or encrypted: ")+recordId);
    }

    // Полная копия (текст и картинки подтянутся с диска), новый каталог,
    // id сохраняется (карты подписок остаются валидны)
    Record fatRecord=location.table->getRecordFat(location.pos);
    const QString newDir=getUniqueId();
    fatRecord.setField("dir", newDir);
    if(fatRecord.getField("file").isEmpty())
        fatRecord.setField("file", QStringLiteral("text.html"));

    targetTable->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                 0, fatRecord);
    location.table->deleteRecordById(recordId);
    model->save();

    return textContent(QStringLiteral("Moved note %1 to branch %2 (%3). Old files went to trash.")
                       .arg(recordId).arg(targetBranchId).arg(targetBranch->getField("name")));
}


QJsonObject McpServer::toolRenameBranch(const QJsonObject &args, bool *isError)
{
    const QString branchId=args.value("branch_id").toString();
    const QString name=args.value("name").toString();
    if(name.isEmpty())
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Empty name."));
    }

    TreeItem *branchItem=model->getItemById(branchId);
    if(!branchItem)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Branch not found: ")+branchId);
    }

    const QString oldName=branchItem->getField("name");
    branchItem->setField("name", name);
    model->save();

    return textContent(QStringLiteral("Renamed branch %1 from '%2' to '%3'.")
                       .arg(branchId).arg(oldName).arg(name));
}


QJsonObject McpServer::toolRenameNote(const QJsonObject &args, bool *isError)
{
    const QString recordId=args.value("record_id").toString();
    const QString name=args.value("name").toString();
    if(name.isEmpty())
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Empty name."));
    }

    const RecordLocation location=locateRecord(recordId);
    if(!location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    Record *record=location.table->getRecord(location.pos);
    if(!record)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    const QString oldName=record->getField("name");
    record->setField("name", name);
    model->save();

    return textContent(QStringLiteral("Renamed note %1 from '%2' to '%3'.")
                       .arg(recordId).arg(oldName).arg(name));
}


QJsonObject McpServer::toolDeleteNote(const QJsonObject &args, bool *isError)
{
    const QString recordId=args.value("record_id").toString();

    const RecordLocation location=locateRecord(recordId);
    if(!location.table)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Record not found: ")+recordId);
    }

    const QString name=location.table->getRecord(location.pos)
                       ? location.table->getRecord(location.pos)->getField("name")
                       : QString();
    location.table->deleteRecordById(recordId);
    purgeLocalRecordFromSubscriptions(recordId);
    model->save();

    return textContent(QStringLiteral("Deleted note %1 ('%2'). Files went to trash.")
                       .arg(recordId).arg(name));
}


QJsonObject McpServer::toolMoveBranch(const QJsonObject &args, bool *isError)
{
    const QString branchId=args.value("branch_id").toString();
    const QString newParentId=args.value("new_parent_branch_id").toString();

    if(branchId.isEmpty() || branchId==QStringLiteral("0"))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Refusing to move the root."));
    }

    TreeItem *branchItem=model->getItemById(branchId);
    if(!branchItem)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Branch not found: ")+branchId);
    }

    TreeItem *newParent=model->getItemById(newParentId);
    if(!newParent)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("New parent branch not found: ")+newParentId);
    }

    if(isBranchChainCrypt(branchItem) || isBranchChainCrypt(newParent))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Encrypted branches are not supported in MCP mode."));
    }

    const QString oldParentName=branchItem->parent()
                                ? branchItem->parent()->getField("name")
                                : QStringLiteral("?");
    if(!model->moveBranchToParent(branchItem, newParent))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Cannot move branch %1 under %2 (cycle or same parent).")
                         .arg(branchId).arg(newParentId));
    }
    model->save();

    return textContent(QStringLiteral("Moved branch %1 ('%2') from '%3' under '%4'.")
                       .arg(branchId).arg(branchItem->getField("name"))
                       .arg(oldParentName).arg(newParent->getField("name")));
}


QJsonObject McpServer::toolDeleteBranch(const QJsonObject &args, bool *isError)
{
    const QString branchId=args.value("branch_id").toString();

    if(branchId.isEmpty() || branchId==QStringLiteral("0"))
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Refusing to delete the root."));
    }

    TreeItem *branchItem=model->getItemById(branchId);
    if(!branchItem)
    {
        if(isError)
            *isError=true;
        return textError(QStringLiteral("Branch not found: ")+branchId);
    }

    const QString name=branchItem->getField("name");
    const int noteCount=model->getRecordCountForItem(branchItem);

    purgeLocalBranchFromSubscriptions(branchItem);

    QModelIndexList indexList;
    indexList << model->getIndexByItem(branchItem);
    model->deleteItemsByModelIndexList(indexList);
    model->save();

    return textContent(QStringLiteral("Deleted branch %1 ('%2') with %3 notes. Files went to trash.")
                       .arg(branchId).arg(name).arg(noteCount));
}


QJsonObject McpServer::toolFindDuplicates(const QJsonObject &args, bool *isError)
{
    Q_UNUSED(args)
    Q_UNUSED(isError)

    // Группировка: одинаковые имена (нормализованные) и одинаковые тексты (sha256)
    QMap<QString, QStringList> byName;
    QMap<QString, QStringList> byTextHash;

    QList<TreeItem*> branches;
    collectAllBranches(branches);
    for(TreeItem *branch : branches)
    {
        RecordTableData *table=branch->recordtableGetTableData();
        if(!table)
            continue;
        for(unsigned int i=0; i<table->size(); ++i)
        {
            Record *record=table->getRecord(static_cast<int>(i));
            if(!record)
                continue;
            const QString id=record->getField("id");
            const QString label=QStringLiteral("%1 (%2, branch %3)").arg(id)
                                .arg(record->getField("name"))
                                .arg(branch->getField("id"));

            const QString normalizedName=record->getField("name").trimmed().toLower();
            if(!normalizedName.isEmpty())
                byName[normalizedName].append(label);

            // Тексты: только незашифрованные с доступными файлами
            if(record->getField("crypt")=="1" || isBranchChainCrypt(branch))
                continue;
            QFile textFile(tetradir+"/base/"+record->getField("dir")+"/"
                           +(record->getField("file").isEmpty()
                             ? QStringLiteral("text.html") : record->getField("file")));
            if(!textFile.open(QIODevice::ReadOnly))
                continue;
            const QString hash=QString::fromLatin1(
                QCryptographicHash::hash(textFile.readAll(), QCryptographicHash::Sha256).toHex());
            textFile.close();
            byTextHash[hash].append(label);
        }
    }

    QStringList lines;
    int groupNo=0;
    for(auto it=byName.constBegin(); it!=byName.constEnd(); ++it)
    {
        if(it.value().size()<2)
            continue;
        groupNo++;
        lines.append(QStringLiteral("Group %1 (same name '%2'):").arg(groupNo).arg(it.key()));
        for(const QString &entry : it.value())
            lines.append(QStringLiteral("  - %1").arg(entry));
    }
    for(auto it=byTextHash.constBegin(); it!=byTextHash.constEnd(); ++it)
    {
        if(it.value().size()<2)
            continue;
        groupNo++;
        lines.append(QStringLiteral("Group %1 (identical text, sha %2):").arg(groupNo).arg(it.key().left(12)));
        for(const QString &entry : it.value())
            lines.append(QStringLiteral("  - %1").arg(entry));
    }

    if(lines.isEmpty())
        return textContent(QStringLiteral("No duplicates found."));
    return textContent(QStringLiteral("Duplicate groups:\n%1").arg(lines.join("\n")));
}


QJsonObject McpServer::toolFindEmptyBranches(const QJsonObject &args, bool *isError)
{
    Q_UNUSED(args)
    Q_UNUSED(isError)

    QStringList lines;
    QList<TreeItem*> branches;
    collectAllBranches(branches);
    for(TreeItem *branch : branches)
    {
        const int noteCount=branch->recordtableGetRowCount();
        if(noteCount==0)
        {
            lines.append(QStringLiteral("- %1 %2 (sub-branches: %3)")
                         .arg(branch->getField("id"))
                         .arg(branch->getField("name"))
                         .arg(branch->childCount()));
        }
    }

    if(lines.isEmpty())
        return textContent(QStringLiteral("No empty branches."));
    return textContent(QStringLiteral("Empty branches (no notes):\n%1").arg(lines.join("\n")));
}


QJsonObject McpServer::toolFindUntaggedNotes(const QJsonObject &args, bool *isError)
{
    Q_UNUSED(isError)

    int limit=200;
    if(args.contains("limit"))
    {
        bool ok=false;
        const int parsed=args.value("limit").toString().toInt(&ok);
        if(ok && parsed>0 && parsed<=1000)
            limit=parsed;
    }

    QStringList lines;
    int total=0;
    QList<TreeItem*> branches;
    collectAllBranches(branches);
    for(TreeItem *branch : branches)
    {
        RecordTableData *table=branch->recordtableGetTableData();
        if(!table)
            continue;
        for(unsigned int i=0; i<table->size(); ++i)
        {
            Record *record=table->getRecord(static_cast<int>(i));
            if(!record)
                continue;
            if(!record->getField("tags").trimmed().isEmpty())
                continue;
            total++;
            if(lines.size()<limit)
            {
                lines.append(QStringLiteral("- %1 | %2 (branch %3 %4)")
                             .arg(record->getField("id"))
                             .arg(record->getField("name"))
                             .arg(branch->getField("id"))
                             .arg(branch->getField("name")));
            }
        }
    }

    if(total==0)
        return textContent(QStringLiteral("No untagged notes. All notes have tags."));
    QStringList output;
    output.append(QStringLiteral("Untagged notes: %1 (showing %2). Pair with suggest_tags to tidy them.")
                  .arg(total).arg(lines.size()));
    output << lines;
    return textContent(output.join("\n"));
}

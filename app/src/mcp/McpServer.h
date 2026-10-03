#ifndef _MCPSERVER_H_
#define _MCPSERVER_H_

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>

class KnowTreeModel;
class RecordTableData;
class TreeItem;
class QtSingleApplication;

// MCP-сервер MyTetra поверх stdio (JSON-RPC 2.0, NDJSON).
//
// Запуск: `mytetra --mcp [--db-path <каталог>]`.
// Совместим с любым MCP-клиентом (Claude Code / OpenCode / Claude Desktop /
// Cursor / Goose и др.): протокол стандартный, транспорт — stdin/stdout
// запущенного процесса. HTTP-режим — отдельная работа на Qt6.
//
// Правила режима:
//   - stdout — только протокол; весь диагностический вывод — в stderr;
//   - один писатель: при запущенном GUI-экземпляре старт запрещён;
//   - модель перезагружается с диска перед каждым вызовом инструмента;
//   - зашифрованные ветки/записи недоступны (пароля в MCP-режиме нет);
//   - БД по умолчанию — tetradir из conf.ini, переопределение --db-path
//     действует только в памяти (в конфиг не пишется).

// Точка входа режима --mcp. Возвращает код выхода процесса.
// Отказывает в старте, если уже запущен другой экземпляр (GUI):
// параллельная запись whole-file mytetra.xml несовместима.
int runMcpMode(QtSingleApplication &app);

class McpServer
{
public:
    // readOnly (--mcp-ro): только чтение и поиск. Пишущие инструменты
    // скрыты из tools/list и отклоняются при прямом вызове
    explicit McpServer(const QString &tetradir, bool readOnly=false);

    // Загрузка модели с диска. false + текст ошибки — нельзя стартовать
    bool init(QString *errorText=nullptr);

    // Цикл обработки stdin до EOF. Возвращает код выхода
    int run(void);

private:
    // --- JSON-RPC ---
    void handleMessage(const QJsonObject &message);
    void handleRequest(const QJsonObject &message);
    void sendResult(const QJsonValue &id, const QJsonObject &result);
    void sendError(const QJsonValue &id, int code, const QString &messageText);
    static QString versionString(void);

    // --- Инструменты ---
    QJsonArray toolSchemas(void);
    QJsonObject callTool(const QString &name, const QJsonObject &args, bool *isError);

    QJsonObject toolListBranches(const QJsonObject &args, bool *isError);
    QJsonObject toolListRecords(const QJsonObject &args, bool *isError);
    QJsonObject toolReadNote(const QJsonObject &args, bool *isError);
    QJsonObject toolSearch(const QJsonObject &args, bool *isError);
    QJsonObject toolCreateNote(const QJsonObject &args, bool *isError);
    QJsonObject toolUpdateNoteText(const QJsonObject &args, bool *isError);
    QJsonObject toolSetTags(const QJsonObject &args, bool *isError);

    // Наведение порядка (housekeeping)
    QJsonObject toolSuggestTags(const QJsonObject &args, bool *isError);
    QJsonObject toolMoveNote(const QJsonObject &args, bool *isError);
    QJsonObject toolRenameBranch(const QJsonObject &args, bool *isError);
    QJsonObject toolRenameNote(const QJsonObject &args, bool *isError);
    QJsonObject toolDeleteNote(const QJsonObject &args, bool *isError);
    QJsonObject toolMoveBranch(const QJsonObject &args, bool *isError);
    QJsonObject toolDeleteBranch(const QJsonObject &args, bool *isError);
    QJsonObject toolFindDuplicates(const QJsonObject &args, bool *isError);
    QJsonObject toolFindEmptyBranches(const QJsonObject &args, bool *isError);
    QJsonObject toolFindUntaggedNotes(const QJsonObject &args, bool *isError);

    // --- Доступ к данным ---
    struct RecordLocation
    {
        TreeItem *branchItem=nullptr;
        RecordTableData *table=nullptr;
        int pos=-1;
    };

    // Свежая модель с диска перед вызовом (дешёво, спасает от stale)
    bool reloadModel(QString *errorText);

    // Поиск записи по id во всех ветках
    RecordLocation locateRecord(const QString &recordId);

    // Ветка (или её предки) зашифрована — в MCP-режиме отказ
    static bool isBranchChainCrypt(TreeItem *branchItem);

    // Обход всего дерева (все ветки, сверху вниз)
    void collectAllBranches(QList<TreeItem*> &branches);

    // Чистка карт подписок от удалённой локальной записи
    // (recordsMap shared->localId во всех подписках)
    void purgeLocalRecordFromSubscriptions(const QString &localRecordId);

    // Чистка карт подписок от удалённого локального поддерева
    void purgeLocalBranchFromSubscriptions(TreeItem *localSubtreeRoot);

    // Текст записи с диска (обрезка лимитом, без диалогов)
    QString readRecordTextFile(const QString &dir, const QString &file,
                               bool *truncated=nullptr);

    static QJsonObject textContent(const QString &text);
    static QJsonObject textError(const QString &text);

    QString tetradir;
    KnowTreeModel *model=nullptr;
    bool initialized=false;
    bool readOnly=false;
};

#endif // _MCPSERVER_H_

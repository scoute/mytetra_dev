// MCP catalog стенд: mcpToolCatalog() после P3.3. Без модели и сервера:
// каталог строится из buildToolsList(ReadWriteDelete) и toolAllowed,
// чистые данные. Падение = регрессия распила McpServer/McpTools.
#include <QCoreApplication>
#include <QSet>
#include <QDebug>

#include "libraries/mcp/McpServer.h"

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QList<McpToolInfo> catalog=mcpToolCatalog();
    check(catalog.size()==27, "CATALOG-COUNT");

    QSet<QString> names;
    bool allFull=true;
    bool catsOk=true;
    QMap<QString,QString> byName;
    for(const McpToolInfo &t : catalog)
    {
        if(t.name.isEmpty() || t.description.isEmpty())
            allFull=false;
        if(t.category!=QStringLiteral("ro") &&
           t.category!=QStringLiteral("rw") &&
           t.category!=QStringLiteral("delete"))
            catsOk=false;
        names.insert(t.name);
        byName[t.name]=t.category;
    }
    check(allFull, "CATALOG-FULL");
    check(catsOk, "CATALOG-CATS");
    check(names.size()==catalog.size(), "CATALOG-UNIQUE");

    // Точечно: чтения ro, записи rw, удаления delete
    check(byName.value("list_branches")==QStringLiteral("ro"), "CAT-RO-READ");
    check(byName.value("get_note")==QStringLiteral("ro"), "CAT-RO-GET");
    check(byName.value("search")==QStringLiteral("ro"), "CAT-RO-SEARCH");
    check(byName.value("create_note")==QStringLiteral("rw"), "CAT-RW-CREATE");
    check(byName.value("update_note")==QStringLiteral("rw"), "CAT-RW-UPDATE");
    check(byName.value("move_note")==QStringLiteral("rw"), "CAT-RW-MOVE");
    check(byName.value("delete_note")==QStringLiteral("delete"), "CAT-DEL-NOTE");
    check(byName.value("delete_branch")==QStringLiteral("delete"), "CAT-DEL-BRANCH");
    check(byName.value("apply_batch")==QStringLiteral("rw"), "CAT-RW-BATCH");
    check(byName.value("stats")==QStringLiteral("ro"), "CAT-RO-STATS");

    qDebug() << (fails==0 ? "MCPCATPROBE-OK" : "MCPCATPROBE-BAD");
    return fails;
}

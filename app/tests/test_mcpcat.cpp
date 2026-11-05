// MCP catalog на QTest (см. 522).
// Порт mcpcatprobe: mcpToolCatalog() после P3.3: каталог строится
// из buildToolsList(ReadWriteDelete) и toolAllowed, чистые данные.
// Падение = регрессия распила McpServer/McpTools.
#include <QtTest>
#include <QSet>
#include <QMap>

#include "libraries/mcp/McpServer.h"

class TestMcpCat : public QObject
{
    Q_OBJECT

private slots:

    void catalogShape(void);
    void catalogReads(void);
    void catalogWrites(void);
};

void TestMcpCat::catalogShape(void)
{
    QList<McpToolInfo> catalog=mcpToolCatalog();
    QCOMPARE(catalog.size(), 27);

    QSet<QString> names;
    bool allFull=true;
    bool catsOk=true;
    for(const McpToolInfo &t : catalog)
    {
        if(t.name.isEmpty() || t.description.isEmpty())
            allFull=false;
        if(t.category!=QStringLiteral("ro") &&
           t.category!=QStringLiteral("rw") &&
           t.category!=QStringLiteral("delete"))
            catsOk=false;
        names.insert(t.name);
    }
    QVERIFY2(allFull, "every tool must have name and description");
    QVERIFY2(catsOk, "every tool must be ro/rw/delete");
    QCOMPARE(names.size(), catalog.size());
}

void TestMcpCat::catalogReads(void)
{
    QMap<QString,QString> byName;
    for(const McpToolInfo &t : mcpToolCatalog())
        byName[t.name]=t.category;

    QCOMPARE(byName.value("list_branches"), QStringLiteral("ro"));
    QCOMPARE(byName.value("get_note"), QStringLiteral("ro"));
    QCOMPARE(byName.value("search"), QStringLiteral("ro"));
    QCOMPARE(byName.value("stats"), QStringLiteral("ro"));
}

void TestMcpCat::catalogWrites(void)
{
    QMap<QString,QString> byName;
    for(const McpToolInfo &t : mcpToolCatalog())
        byName[t.name]=t.category;

    QCOMPARE(byName.value("create_note"), QStringLiteral("rw"));
    QCOMPARE(byName.value("update_note"), QStringLiteral("rw"));
    QCOMPARE(byName.value("move_note"), QStringLiteral("rw"));
    QCOMPARE(byName.value("apply_batch"), QStringLiteral("rw"));
    QCOMPARE(byName.value("delete_note"), QStringLiteral("delete"));
    QCOMPARE(byName.value("delete_branch"), QStringLiteral("delete"));
}

QTEST_APPLESS_MAIN(TestMcpCat)

#include "test_mcpcat.moc"

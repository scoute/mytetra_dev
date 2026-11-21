#include <QMap>
#include <QString>

#include <cstdio>

#include "SyncCounts.h"

// «Сколько исчезнет вместе с веткой»: ветка + подветки + заметки.
// Индексы строятся один раз (было O(n²) на больших удалениях).
//
//   make -C app/tests run

static int failures=0;

#define CHECK(cond) do { \
    if(!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while(0)


static SyncBranchInfo branch(const QString &id, const QString &parent)
{
    return SyncBranchInfo{id, parent, 0, QString(), {}};
}

static SyncNoteInfo note(const QString &id, const QString &branchId)
{
    return SyncNoteInfo{id, branchId, 0, QString(), QString(), false, {}, {}};
}


int main(void)
{
    QMap<QString, SyncBranchInfo> branches;
    branches.insert(QStringLiteral("b1"), branch(QStringLiteral("b1"), QString()));
    branches.insert(QStringLiteral("b2"), branch(QStringLiteral("b2"), QStringLiteral("b1")));
    branches.insert(QStringLiteral("b3"), branch(QStringLiteral("b3"), QStringLiteral("b2")));

    QMap<QString, SyncNoteInfo> notes;
    notes.insert(QStringLiteral("n1"), note(QStringLiteral("n1"), QStringLiteral("b1")));
    notes.insert(QStringLiteral("n2"), note(QStringLiteral("n2"), QStringLiteral("b1")));
    notes.insert(QStringLiteral("n3"), note(QStringLiteral("n3"), QStringLiteral("b2")));
    notes.insert(QStringLiteral("n4"), note(QStringLiteral("n4"), QStringLiteral("b3")));

    const SyncSubtreeCounter counter(branches, notes);

    CHECK(counter.count(QStringLiteral("b1"))==7); // 3 ветки + 4 заметки
    CHECK(counter.count(QStringLiteral("b2"))==4); // b2,b3 + n3,n4
    CHECK(counter.count(QStringLiteral("b3"))==2);
    CHECK(counter.count(QStringLiteral("n1"))==1);
    CHECK(counter.count(QStringLiteral("none"))==0);
    CHECK(counter.count(QString())==0);

    // Цикл в parentId не вешает
    QMap<QString, SyncBranchInfo> loop;
    loop.insert(QStringLiteral("x"), branch(QStringLiteral("x"), QStringLiteral("y")));
    loop.insert(QStringLiteral("y"), branch(QStringLiteral("y"), QStringLiteral("x")));

    const SyncSubtreeCounter loopCounter(loop, {});
    CHECK(loopCounter.count(QStringLiteral("x"))==2);
    CHECK(loopCounter.count(QStringLiteral("y"))==2);

    if(failures==0)
        std::printf("sync_counts_test OK\n");

    return failures==0 ? 0 : 1;
}

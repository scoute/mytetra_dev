#include <QMap>
#include <QString>
#include <QStringList>

#include <cstdio>

#include "SyncScope.h"

// Срез плана по ветке: действия — только внутри поддерева; предки
// добавляются лишь как зависимость размещения (и только отсутствующие).
//
//   make -C app/tests run

static int failures=0;

#define CHECK(cond) do { \
    if(!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while(0)


static SyncBranchInfo branch(const QString &id, const QString &parent,
                             const QString &hash=QStringLiteral("h"))
{
    return SyncBranchInfo{id, parent, 0, hash, {}};
}

static SyncNoteInfo note(const QString &id, const QString &branchId)
{
    return SyncNoteInfo{id, branchId, 0, QString(), QString(), false, {}, {}};
}


int main(void)
{
    // a -> s -> t; заметки n1 (s), n2 (a, вне scope), n3 (t)
    QMap<QString, SyncBranchInfo> branches;
    branches.insert(QStringLiteral("a"), branch(QStringLiteral("a"), QString()));
    branches.insert(QStringLiteral("s"), branch(QStringLiteral("s"), QStringLiteral("a")));
    branches.insert(QStringLiteral("t"), branch(QStringLiteral("t"), QStringLiteral("s")));

    QMap<QString, SyncNoteInfo> notes;
    notes.insert(QStringLiteral("n1"), note(QStringLiteral("n1"), QStringLiteral("s")));
    notes.insert(QStringLiteral("n2"), note(QStringLiteral("n2"), QStringLiteral("a")));
    notes.insert(QStringLiteral("n3"), note(QStringLiteral("n3"), QStringLiteral("t")));

    SyncManifest local;
    local.branches=branches;
    local.notes=notes;

    SyncManifest remote=local;

    SyncPlan plan;
    plan.uploadBranches << QStringLiteral("a");      // переименование предка
    plan.downloadBranches << QStringLiteral("a");    // и его чужая версия
    plan.uploadNotes << QStringLiteral("n1") << QStringLiteral("n2");
    plan.downloadNotes << QStringLiteral("n3");
    plan.deleteLocal << QStringLiteral("a") << QStringLiteral("n1");
    plan.pushTombs << QStringLiteral("a") << QStringLiteral("n3");
    plan.conflicts << QStringLiteral("a") << QStringLiteral("n1");

    const SyncPlan sub=SyncScope::slice(plan, local, remote, QStringLiteral("s"));

    // Предок a существует локально: его поля не трогаем в обе стороны
    CHECK(sub.uploadBranches.isEmpty());
    CHECK(sub.downloadBranches.isEmpty());

    // Заметки — только s/t, n2 (под a) вне scope
    CHECK(sub.uploadNotes==QStringList{QStringLiteral("n1")});
    CHECK(sub.downloadNotes==QStringList{QStringLiteral("n3")});

    // Tombstone/конфликт на предке не применяем; внутри — да
    CHECK(sub.deleteLocal==QStringList{QStringLiteral("n1")});
    CHECK(sub.pushTombs==QStringList{QStringLiteral("n3")});
    CHECK(sub.conflicts==QStringList{QStringLiteral("n1")});

    // --- отсутствующий предок нужен для размещения ---
    {
        SyncManifest localNoA;
        localNoA.branches.insert(QStringLiteral("s"),
            branch(QStringLiteral("s"), QStringLiteral("a")));
        localNoA.branches.insert(QStringLiteral("t"),
            branch(QStringLiteral("t"), QStringLiteral("s")));

        SyncPlan p;
        p.downloadBranches << QStringLiteral("a") << QStringLiteral("s");

        const SyncPlan s2=SyncScope::slice(p, localNoA, remote, QStringLiteral("s"));
        // Предок идёт ПЕРЕД поддеревом (иначе create не найдёт родителя)
        const QStringList expected=QStringList()
            << QStringLiteral("a") << QStringLiteral("s");
        CHECK(s2.downloadBranches==expected);
    }

    // --- существующий предок с расхождением не скачивается ---
    {
        SyncPlan p;
        p.downloadBranches << QStringLiteral("a") << QStringLiteral("s");
        const SyncPlan s3=SyncScope::slice(p, local, remote, QStringLiteral("s"));
        CHECK(!s3.downloadBranches.contains(QStringLiteral("a")));
        CHECK(s3.downloadBranches.contains(QStringLiteral("s")));
    }

    // --- пустой scope id = полный синк (план без изменений) ---
    {
        const SyncPlan s4=SyncScope::slice(plan, local, remote, QString());
        CHECK(s4.uploadBranches==plan.uploadBranches);
        CHECK(s4.uploadNotes==plan.uploadNotes);
    }

    if(failures==0)
        std::printf("sync_scope_test OK\n");

    return failures==0 ? 0 : 1;
}

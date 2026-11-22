#include <QMap>
#include <QString>
#include <QStringList>

#include <cstdio>

#include "SyncPreview.h"

// Регрессия: "восстановить с сервера" / "настоять на моём" для удалённой
// ВЕТКИ должно тянуть всё поддерево (подветки и заметки), а не только
// строку самой ветки. Заметки под удалённой веткой своих tombstone'ов
// не имеют — их покрывает tombstone ветки, поэтому в план они не попадают.
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
    SyncBranchInfo b;
    b.id=id;
    b.parentId=parent;
    b.mtime=1;
    b.hash=id;
    return b;
}


static SyncNoteInfo note(const QString &id, const QString &branchId)
{
    SyncNoteInfo n;
    n.id=id;
    n.branchId=branchId;
    n.mtime=1;
    n.hash=id;
    n.fhash=id;
    return n;
}


int main(void)
{
    using namespace SyncPreview;

    // --- Скачать с сервера удалённую ветку вместе с поддеревом ---
    {
        SyncManifest local;
        local.tombstones.insert(QStringLiteral("B1"), 10);
        local.tombstones.insert(QStringLiteral("B2"), 10);

        SyncManifest remote;
        remote.branches.insert(QStringLiteral("B1"), branch("B1", ""));
        remote.branches.insert(QStringLiteral("B2"), branch("B2", "B1"));
        remote.notes.insert(QStringLiteral("N1"), note("N1", "B1"));
        remote.notes.insert(QStringLiteral("N2"), note("N2", "B2"));

        SyncPlan plan;
        plan.pushTombs << QStringLiteral("B1") << QStringLiteral("B2");

        const Preview preview=classify(plan, local, remote);
        CHECK(preview.items.size()==2);

        Decisions decisions=Decisions::defaults();
        // Решаем только за корень; потомок остаётся Confirm, но
        // воскрешение корня обязано отменить и его удаление
        decisions.byId[QStringLiteral("B1")]=Action::TakeServer;

        const SyncPlan out=applyDecisions(plan, preview, decisions,
                                          local, remote);

        CHECK(out.pushTombs.isEmpty());
        CHECK(out.downloadBranches.contains(QStringLiteral("B1")));
        CHECK(out.downloadBranches.contains(QStringLiteral("B2")));
        CHECK(out.downloadNotes.contains(QStringLiteral("N1")));
        CHECK(out.downloadNotes.contains(QStringLiteral("N2")));
    }

    // --- Настоять на своём: залить удалённое на сервере поддерево ---
    {
        SyncManifest local;
        local.branches.insert(QStringLiteral("B1"), branch("B1", ""));
        local.branches.insert(QStringLiteral("B2"), branch("B2", "B1"));
        local.notes.insert(QStringLiteral("N1"), note("N1", "B1"));
        local.notes.insert(QStringLiteral("N2"), note("N2", "B2"));

        SyncManifest remote;
        remote.tombstones.insert(QStringLiteral("B1"), 10);
        remote.tombstones.insert(QStringLiteral("B2"), 10);

        SyncPlan plan;
        plan.deleteLocal << QStringLiteral("B1") << QStringLiteral("B2");

        const Preview preview=classify(plan, local, remote);
        CHECK(preview.items.size()==2);

        Decisions decisions=Decisions::defaults();
        decisions.byId[QStringLiteral("B1")]=Action::ForceMine;

        const SyncPlan out=applyDecisions(plan, preview, decisions,
                                          local, remote);

        CHECK(out.deleteLocal.isEmpty());
        CHECK(out.uploadBranches.contains(QStringLiteral("B1")));
        CHECK(out.uploadBranches.contains(QStringLiteral("B2")));
        CHECK(out.uploadNotes.contains(QStringLiteral("N1")));
        CHECK(out.uploadNotes.contains(QStringLiteral("N2")));
    }

    // --- Обычная правка ветки не должна тянуть чужие заметки ---
    {
        SyncManifest local;
        local.branches.insert(QStringLiteral("B1"), branch("B1", ""));
        local.notes.insert(QStringLiteral("N1"), note("N1", "B1"));

        SyncManifest remote;
        remote.branches.insert(QStringLiteral("B1"), branch("B1", ""));
        remote.notes.insert(QStringLiteral("N1"), note("N1", "B1"));

        SyncPlan plan;
        plan.downloadBranches << QStringLiteral("B1");

        const Preview preview=classify(plan, local, remote);
        Decisions decisions=Decisions::defaults();
        decisions.byId[QStringLiteral("B1")]=Action::TakeServer;

        const SyncPlan out=applyDecisions(plan, preview, decisions,
                                          local, remote);

        CHECK(out.downloadBranches.size()==1);
        CHECK(out.downloadNotes.isEmpty());
    }

    if(failures==0)
        std::printf("sync_preview_test OK\n");

    return failures==0 ? 0 : 1;
}

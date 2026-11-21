#include <QMap>
#include <QSet>
#include <QStringList>

#include "SyncScope.h"


namespace SyncScope
{

SyncPlan slice(const SyncPlan &plan,
               const SyncManifest &local,
               const SyncManifest &remote,
               const QString &scopeRootId)
{
    // Пустой scope — полный синк (вызывающий и так не срезает)
    if(scopeRootId.isEmpty())
        return plan;

    // UNION локального и удалённого: созданное с любой стороны входит
    QMap<QString, QString> branchParent; // id -> parentId
    QMap<QString, QStringList> children; // parentId -> id веток

    auto addBranches=[&](const QMap<QString, SyncBranchInfo> &branches) {
        for(auto it=branches.constBegin(); it!=branches.constEnd(); ++it)
        {
            if(!branchParent.contains(it.key()))
                branchParent.insert(it.key(), it.value().parentId);

            if(!children[it.value().parentId].contains(it.key()))
                children[it.value().parentId] << it.key();
        }
    };

    addBranches(local.branches);
    addBranches(remote.branches);

    // Ветка + все потомки вниз — это и есть scope
    QSet<QString> descendantScope;
    descendantScope.insert(scopeRootId);
    QStringList queue;
    queue << scopeRootId;

    while(!queue.isEmpty())
    {
        const QString cur=queue.takeFirst();

        for(const QString &child : children.value(cur))
        {
            if(descendantScope.contains(child))
                continue;

            descendantScope.insert(child);
            queue << child;
        }
    }

    // Заметки по branchId: только корень scope и потомки.
    // Заметки предков сюда не входят.
    QSet<QString> noteScope;

    auto addNotes=[&](const QMap<QString, SyncNoteInfo> &notes) {
        for(auto it=notes.constBegin(); it!=notes.constEnd(); ++it)
            if(descendantScope.contains(it.value().branchId))
                noteScope.insert(it.key());
    };

    addNotes(local.notes);
    addNotes(remote.notes);

    // Предки — только как зависимость для размещения, не как действия
    QSet<QString> ancestorIds;
    QSet<QString> seen;
    QString cur=scopeRootId;

    while(!seen.contains(cur))
    {
        seen.insert(cur);

        const QString pid=branchParent.value(cur);

        if(pid.isEmpty() || pid==QStringLiteral("0"))
            break;

        ancestorIds.insert(pid);

        if(!branchParent.contains(pid))
            break;

        cur=pid;
    }

    auto keep=[&](const QStringList &list, const QSet<QString> &scope) {
        QStringList out;

        for(const QString &id : list)
            if(scope.contains(id))
                out << id;

        return out;
    };

    SyncPlan sub;
    sub.uploadNotes=keep(plan.uploadNotes, noteScope);
    sub.downloadNotes=keep(plan.downloadNotes, noteScope);

    // Действия по веткам — только сам scope. Изменения полей предков
    // (переименование/иконка/родитель) в срез не попадают.
    sub.uploadBranches=keep(plan.uploadBranches, descendantScope);

    // Исключение — отсутствующий у нас предок: его надо скачать, чтобы
    // было куда встать выбранной ветке. Порядок — как в плане
    // (родители раньше детей), поэтому предки идут ПЕРЕД поддеревом.
    QStringList missingAncestors;

    for(const QString &id : plan.downloadBranches)
        if(ancestorIds.contains(id) && !local.branches.contains(id))
            missingAncestors << id;

    sub.downloadBranches=missingAncestors
        +keep(plan.downloadBranches, descendantScope);

    // Tombstone'ы и конфликты — только внутри scope (предки не в счёте,
    // иначе синк ветки удалял бы данные вне выбранного поддерева)
    const QSet<QString> allScope=descendantScope+noteScope;
    sub.deleteLocal=keep(plan.deleteLocal, allScope);
    sub.pushTombs=keep(plan.pushTombs, allScope);
    sub.conflicts=keep(plan.conflicts, allScope);

    return sub;
}

} // namespace SyncScope

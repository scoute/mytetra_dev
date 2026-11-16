#include <QSet>
#include <algorithm>

#include "SyncPreview.h"

namespace SyncPreview
{

namespace
{

QString noteName(const SyncManifest &manifest, const QString &id)
{
    return manifest.notes.value(id).fields.value(QStringLiteral("name"));
}


QString branchName(const SyncManifest &manifest, const QString &id)
{
    return manifest.branches.value(id).fields.value(QStringLiteral("name"));
}


Item makeItem(const QString &id, Category category, bool isBranch,
              bool fromLocal, const QString &name,
              const QString &parentId=QString(),
              const QString &parentName=QString())
{
    Item item;
    item.id=id;
    item.category=category;
    item.isBranch=isBranch;
    item.fromLocal=fromLocal;
    item.name=name;
    item.parentId=parentId;
    item.parentName=parentName;

    if(!isBranch)
        item.branchId=parentId; // у записей родитель и есть ветка

    return item;
}

// Имя ветки-родителя для дерева: для записей — содержащая ветка,
// для веток — родительская (пусто = верхний уровень)
QString parentBranchName(const SyncManifest &manifest, const QString &branchId)
{
    if(branchId.isEmpty())
        return QString();

    return branchName(manifest, branchId);
}


// Глубина ветки в манифесте (для порядка родители-вперед)
int branchDepth(const SyncManifest &manifest, const QString &id)
{
    int depth=0;
    QString cur=id;
    QSet<QString> seen;

    while(true)
    {
        const QString parent=manifest.branches.value(cur).parentId;

        if(parent.isEmpty() || seen.contains(parent))
            break;

        seen.insert(parent);
        cur=parent;
        depth++;

        if(depth>10000)
            break;
    }

    return depth;
}


void orderBranchesParentsFirst(const SyncManifest &manifest,
                               QStringList &ids)
{
    std::stable_sort(ids.begin(), ids.end(),
                     [&](const QString &a, const QString &b) {
        return branchDepth(manifest, a) < branchDepth(manifest, b);
    });
}

} // namespace


int Preview::count(Category category) const
{
    int count=0;

    for(const Item &item : items)
        if(item.category==category)
            count++;

    return count;
}


bool Preview::isEmpty(void) const
{
    return items.isEmpty();
}


bool Preview::hasDestructive(void) const
{
    return count(Category::MyDelete)>0 || count(Category::TheirDelete)>0;
}


bool Preview::needsAttention(void) const
{
    return count(Category::Rename)>0 || count(Category::Edit)>0
        || hasDestructive();
}


Preview classify(const SyncPlan &plan,
                 const SyncManifest &local,
                 const SyncManifest &remote)
{
    Preview preview;

    // Наши изменения, едущие наружу
    for(const QString &id : plan.uploadNotes)
    {
        Category category;

        if(!remote.notes.contains(id))
            category=Category::Create;       // воскрешение тоже Create:
                                             // для пира это новая запись
        else if(local.notes.value(id).hash!=remote.notes.value(id).hash)
            category=Category::Edit;
        else
            category=Category::Rename;       // контент тот же, поля/ветка другие

        const QString branchId=local.notes.value(id).branchId;
        preview.items.append(
            makeItem(id, category, false, true, noteName(local, id),
                     branchId, parentBranchName(local, branchId)));
    }

    for(const QString &id : plan.uploadBranches)
    {
        // Ветка не имеет контента: любые расхождения — метаданные
        const Category category=remote.branches.contains(id)
            ? Category::Rename : Category::Create;

        const QString parentId=local.branches.value(id).parentId;
        preview.items.append(
            makeItem(id, category, true, true, branchName(local, id),
                     parentId, parentBranchName(local, parentId)));
    }

    // Ихние изменения, приходящие к нам
    for(const QString &id : plan.downloadNotes)
    {
        Category category;

        if(!local.notes.contains(id))
            category=Category::Create;
        else if(local.notes.value(id).hash!=remote.notes.value(id).hash)
            category=Category::Edit;
        else
            category=Category::Rename;

        const QString branchId=remote.notes.value(id).branchId;
        preview.items.append(
            makeItem(id, category, false, false, noteName(remote, id),
                     branchId, parentBranchName(remote, branchId)));
    }

    for(const QString &id : plan.downloadBranches)
    {
        const Category category=local.branches.contains(id)
            ? Category::Rename : Category::Create;

        const QString parentId=remote.branches.value(id).parentId;
        preview.items.append(
            makeItem(id, category, true, false, branchName(remote, id),
                     parentId, parentBranchName(remote, parentId)));
    }

    // Удаления по направлениям
    for(const QString &id : plan.pushTombs)
    {
        const bool isBranch=!local.branches.contains(id)
            && !local.notes.contains(id) ? false
            : local.branches.contains(id);

        preview.items.append(
            makeItem(id, Category::MyDelete, isBranch, true,
                     isBranch ? branchName(local, id) : noteName(local, id)));
    }

    for(const QString &id : plan.deleteLocal)
    {
        const bool isBranch=local.branches.contains(id)
            || remote.branches.contains(id);
        const QString parentId=isBranch
            ? local.branches.value(id).parentId
            : local.notes.value(id).branchId;

        preview.items.append(
            makeItem(id, Category::TheirDelete, isBranch, false,
                     isBranch ? branchName(remote, id) : noteName(remote, id),
                     parentId, parentBranchName(local, parentId)));
    }

    return preview;
}


Action Decisions::forItem(const Item &item) const
{
    if(byId.contains(item.id))
        return byId.value(item.id);

    return category.value(item.category, Action::Confirm);
}


Decisions Decisions::defaults(void)
{
    Decisions decisions;
    decisions.category[Category::Create]=Action::Apply;
    decisions.category[Category::Rename]=Action::Confirm;
    decisions.category[Category::Edit]=Action::Confirm;
    decisions.category[Category::MyDelete]=Action::TakeServer;
    decisions.category[Category::TheirDelete]=Action::Confirm;
    return decisions;
}


SyncPlan applyDecisions(const SyncPlan &plan,
                        const Preview &preview,
                        const Decisions &decisions,
                        const SyncManifest &local,
                        const SyncManifest &remote)
{
    QStringList uploadNotes=plan.uploadNotes;
    QStringList downloadNotes=plan.downloadNotes;
    QStringList uploadBranches=plan.uploadBranches;
    QStringList downloadBranches=plan.downloadBranches;
    QStringList deleteLocal=plan.deleteLocal;
    QStringList pushTombs=plan.pushTombs;

    auto removeId=[&](QStringList &list, const QString &id) {
        list.removeAll(id);
    };

    for(const Item &item : preview.items)
    {
        const Action action=decisions.forItem(item);

        if(action==Action::Apply || action==Action::Confirm)
            continue; // уже в плане

        if(action==Action::Skip)
        {
            // Расхождение остается: сайдкар не трогаем, следующий синк
            // снова покажет этот элемент
            if(item.category==Category::MyDelete)
                removeId(pushTombs, item.id);
            else if(item.category==Category::TheirDelete)
                removeId(deleteLocal, item.id);
            else if(item.fromLocal)
            {
                removeId(uploadNotes, item.id);
                removeId(uploadBranches, item.id);
            }
            else
            {
                removeId(downloadNotes, item.id);
                removeId(downloadBranches, item.id);
            }

            continue;
        }

        if(action==Action::TakeServer)
        {
            if(!item.fromLocal)
                continue; // это и есть серверное: применяем как есть

            // Наше не едет, вместо этого приезжает ихнее (воскрешение
            // для удалений). Для новой записи ихнего нет — просто не едет
            removeId(uploadNotes, item.id);
            removeId(uploadBranches, item.id);
            removeId(pushTombs, item.id);

            if(remote.notes.contains(item.id))
                downloadNotes.append(item.id);
            else if(remote.branches.contains(item.id))
                downloadBranches.append(item.id);

            continue;
        }

        if(action==Action::ForceMine)
        {
            if(item.fromLocal)
                continue; // наше и так едет

            // Ихнее не приезжает, наше побеждает (для ихнего удаления —
            // воскрешение на сервере нашей копией)
            removeId(downloadNotes, item.id);
            removeId(downloadBranches, item.id);
            removeId(deleteLocal, item.id);

            if(local.notes.contains(item.id))
                uploadNotes.append(item.id);
            else if(local.branches.contains(item.id))
                uploadBranches.append(item.id);

            continue;
        }
    }

    uploadNotes.removeDuplicates();
    downloadNotes.removeDuplicates();
    uploadBranches.removeDuplicates();
    downloadBranches.removeDuplicates();
    deleteLocal.removeDuplicates();
    pushTombs.removeDuplicates();

    // Порядок веток: родители раньше детей (переписывание могло добавить
    // ветки в конец списка)
    orderBranchesParentsFirst(local, uploadBranches);
    orderBranchesParentsFirst(remote, downloadBranches);

    SyncPlan result;
    result.uploadNotes=uploadNotes;
    result.downloadNotes=downloadNotes;
    result.uploadBranches=uploadBranches;
    result.downloadBranches=downloadBranches;
    result.deleteLocal=deleteLocal;
    result.pushTombs=pushTombs;
    return result;
}


QString serializeDefaults(const Decisions &decisions)
{
    QStringList parts;

    for(int i=0; i<5; ++i)
        parts << QString::number(static_cast<int>(
            decisions.category.value(static_cast<Category>(i),
                                     Action::Confirm)));

    return parts.join(QLatin1Char(','));
}


Decisions defaultsFromString(const QString &data)
{
    Decisions decisions=Decisions::defaults();
    const QStringList parts=data.split(QLatin1Char(','));

    if(parts.size()==5)
        for(int i=0; i<5; ++i)
        {
            bool ok=false;
            const int value=parts.at(i).toInt(&ok);

            if(ok && value>=0 && value<=4)
                decisions.category[static_cast<Category>(i)]=
                    static_cast<Action>(value);
        }

    return decisions;
}


QString actionName(Action action)
{
    switch(action)
    {
    case Action::Apply:      return QObject::tr("Apply silently");
    case Action::Confirm:    return QObject::tr("Show and confirm");
    case Action::TakeServer: return QObject::tr("Take from the server");
    case Action::ForceMine:  return QObject::tr("Keep mine");
    case Action::Skip:       return QObject::tr("Skip this time");
    }

    return QString();
}


QString categoryName(Category category)
{
    switch(category)
    {
    case Category::Create:      return QObject::tr("Added");
    case Category::Rename:      return QObject::tr("Renamed");
    case Category::Edit:        return QObject::tr("Modified");
    case Category::MyDelete:    return QObject::tr("My deletions");
    case Category::TheirDelete: return QObject::tr("Their deletions");
    }

    return QString();
}

} // namespace SyncPreview

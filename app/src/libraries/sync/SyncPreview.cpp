#include <QRegularExpression>
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


// Имя ищем на ОБЕИХ сторонах: у удаленной записи своя сторона уже
// ничего не знает (иначе в окне "(без имени)" вместо реального имени).
// Удаление едет наружу — имя берет с той стороны, где элемент еще жив
QString anyNoteName(const SyncManifest &local, const SyncManifest &remote,
                    const QString &id)
{
    QString name=noteName(local, id);

    if(name.isEmpty())
        name=noteName(remote, id);

    return name;
}


// Имя/родитель ищутся на ОБЕИХ сторонах: у удаленного элемента
// его сторона уже ничего не знает
QString parentOf(const SyncManifest &local, const SyncManifest &remote,
                 const QString &id)
{
    if(local.notes.contains(id) && !local.notes.value(id).branchId.isEmpty())
        return local.notes.value(id).branchId;

    if(local.branches.contains(id) && !local.branches.value(id).parentId.isEmpty())
        return local.branches.value(id).parentId;

    if(remote.notes.contains(id) && !remote.notes.value(id).branchId.isEmpty())
        return remote.notes.value(id).branchId;

    if(remote.branches.contains(id) && !remote.branches.value(id).parentId.isEmpty())
        return remote.branches.value(id).parentId;

    return QString();
}

QString anyBranchName(const SyncManifest &local, const SyncManifest &remote,
                      const QString &id)
{
    if(id.isEmpty())
        return QString();

    QString name=local.branches.value(id).fields.value(QStringLiteral("name"));

    if(name.isEmpty())
        name=remote.branches.value(id).fields.value(QStringLiteral("name"));

    return name;
}

/** Цепочка предков: ids и имена от корня до родителя. */
void pathOf(const SyncManifest &local, const SyncManifest &remote,
            const QString &id, QStringList &outIds, QStringList &outNames)
{
    outIds.clear();
    outNames.clear();
    QSet<QString> seen;
    QString cur=parentOf(local, remote, id);

    while(!cur.isEmpty() && !seen.contains(cur) && outIds.size()<10000)
    {
        seen.insert(cur);
        outIds.prepend(cur);
        outNames.prepend(anyBranchName(local, remote, cur));
        cur=parentOf(local, remote, cur);
    }
}

Item makeItem(const SyncManifest &local, const SyncManifest &remote,
              const QString &id, Category category, bool isBranch,
              bool fromLocal, const QString &name)
{
    Item item;
    item.id=id;
    item.category=category;
    item.isBranch=isBranch;
    item.fromLocal=fromLocal;
    item.name=name;

    if(isBranch)
    {
        item.ownName=local.branches.value(id).fields.value(QStringLiteral("name"));
        item.otherName=remote.branches.value(id).fields.value(QStringLiteral("name"));
    }
    else
    {
        item.ownName=noteName(local, id);
        item.otherName=noteName(remote, id);
    }

    pathOf(local, remote, id, item.pathIds, item.path);
    item.parentId=item.pathIds.isEmpty() ? QString() : item.pathIds.last();
    item.parentName=item.path.isEmpty() ? QString() : item.path.last();

    if(!isBranch)
        item.branchId=item.parentId; // у записей родитель и есть ветка

    item.ownAttaches=local.notes.contains(id)
        ? local.notes.value(id).attaches.size() : 0;
    item.otherAttaches=remote.notes.contains(id)
        ? remote.notes.value(id).attaches.size() : 0;

    return item;
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

        preview.items.append(
            makeItem(local, remote, id, category, false, true,
                     anyNoteName(local, remote, id)));
    }

    for(const QString &id : plan.uploadBranches)
    {
        // Ветка не имеет контента: любые расхождения — метаданные
        const Category category=remote.branches.contains(id)
            ? Category::Rename : Category::Create;

        preview.items.append(
            makeItem(local, remote, id, category, true, true,
                     anyBranchName(local, remote, id)));
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

        preview.items.append(
            makeItem(local, remote, id, category, false, false,
                     anyNoteName(local, remote, id)));
    }

    for(const QString &id : plan.downloadBranches)
    {
        const Category category=local.branches.contains(id)
            ? Category::Rename : Category::Create;

        preview.items.append(
            makeItem(local, remote, id, category, true, false,
                     anyBranchName(local, remote, id)));
    }

    // Удаления по направлениям
    for(const QString &id : plan.pushTombs)
    {
        // Тип — по ОБОИМ манифестам: своего уже нет (удалён),
        // живой лежит у пира. Только локальный давал вечный false
        // для веток — и имя искалось среди заметок ("без имени")
        const bool isBranch=local.branches.contains(id)
            || remote.branches.contains(id);

        preview.items.append(
            makeItem(local, remote, id, Category::MyDelete, isBranch, true,
                     isBranch ? anyBranchName(local, remote, id)
                              : anyNoteName(local, remote, id)));
    }

    for(const QString &id : plan.deleteLocal)
    {
        const bool isBranch=local.branches.contains(id)
            || remote.branches.contains(id);
        preview.items.append(
            makeItem(local, remote, id, Category::TheirDelete, isBranch, false,
                     isBranch ? anyBranchName(local, remote, id)
                              : anyNoteName(local, remote, id)));
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
    decisions.category[Category::MyDelete]=Action::Confirm;
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


int visibleTextLength(const QString &text)
{
    QString noTags=text;
    noTags.replace(QRegularExpression(QStringLiteral("<[^>]*>")),
                   QStringLiteral(" "));
    QString noEntities=noTags;
    noEntities.replace(
        QRegularExpression(QStringLiteral("&[a-zA-Z#0-9]{1,8};")),
        QStringLiteral(" "));
    return noEntities.trimmed().length();
}


bool isNearEmpty(const QString &text, int attaches)
{
    return visibleTextLength(text)<NEAR_EMPTY_CHARS && attaches==0;
}


Danger dangerFor(const Item &item, bool ownEmpty, bool otherEmpty)
{
    if(item.category!=Category::Edit)
        return Danger::None;

    if(ownEmpty && !otherEmpty)
        return Danger::MyEmpty;

    if(otherEmpty && !ownEmpty)
        return Danger::TheirEmpty;

    return Danger::None;
}


QString actionName(Action action)
{
    switch(action)
    {
    case Action::Apply:      return QObject::tr("Применить молча");
    case Action::Confirm:    return QObject::tr("Показать и подтвердить");
    case Action::TakeServer: return QObject::tr("Восстановить с сервера");
    case Action::ForceMine:  return QObject::tr("Настоять на моём");
    case Action::Skip:       return QObject::tr("Пропустить в этот раз");
    }

    return QString();
}


QString categoryName(Category category)
{
    switch(category)
    {
    case Category::Create:      return QObject::tr("Добавлено");
    case Category::Rename:      return QObject::tr("Переименовано");
    case Category::Edit:        return QObject::tr("Изменено");
    case Category::MyDelete:    return QObject::tr("Мои удаления");
    case Category::TheirDelete: return QObject::tr("Ихние удаления");
    }

    return QString();
}

} // namespace SyncPreview

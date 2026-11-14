#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>

#include "SyncManifest.h"
#include "SyncSidecar.h"
#include "SyncDebug.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/attachTable/AttachTableData.h"
#include "models/attachTable/Attach.h"


QString SyncManifestBuilder::sha256File(const QString &path)
{
    QFile file(path);

    if(!file.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}


QString SyncManifestBuilder::sha256Bytes(const QByteArray &data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data,
                                 QCryptographicHash::Sha256).toHex());
}


static void collectBranches(const TreeItem *item, const QString &parentId,
                            SyncManifest &out, SyncSidecar &sidecar)
{
    if(item==nullptr)
        return;

    const QString id=item->getField(QStringLiteral("id"));

    // Корневой псевдо-элемент не синхронизируется, только его дети
    if(!id.isEmpty() && id!=QStringLiteral("0"))
    {
        SyncBranchInfo info;
        info.id=id;
        info.parentId=parentId;
        info.fields=item->getAllFieldsDirect();

        if(sidecar.hasEntry(id))
        {
            const SyncSidecar::Entry e=sidecar.entry(id);
            info.mtime=e.mtime;
            info.hash=e.hash;
        }
        else
        {
            // Истории нет — хеш живьем, mtime ноль (досчитается при первом сейве)
            info.hash=SyncSidecar::hashBranchFields(info.fields);
            info.mtime=0;
        }

        out.branches.insert(id, info);
    }

    for(int i=0; i<item->childCount(); ++i)
        collectBranches(item->child(i), id, out, sidecar);
}


SyncManifest SyncManifestBuilder::build(KnowTreeModel *model,
                                        SyncSidecar &sidecar,
                                        const QString &tetradir)
{
    SyncManifest out;

    if(model==nullptr)
        return out;

    QElapsedTimer timer;
    timer.start();

    // Ветки обходом от корня
    collectBranches(model->getRootItemPublic(), QString(), out, sidecar);

    // Заметки по реестру id. Ветка — последний элемент пути
    const QSharedPointer< QSet<QString> > allIds=model->getAllRecordsIdList();

    if(!allIds.isNull())
    {
        for(const QString &id : allIds->values())
        {
            Record *record=model->getRecord(id);

            if(record==nullptr)
                continue;

            SyncNoteInfo info;
            info.id=id;

            const QStringList path=model->getRecordPath(id);
            info.branchId=path.isEmpty() ? QString() : path.last();

            info.crypt=(record->getNaturalFieldSource(QStringLiteral("crypt"))==QStringLiteral("1"));

            // Синхронизируются только натуральные поля. hasAttach/attachCount —
            // вычислимые (Record::getCalculableField), через getNaturalFieldSource
            // роняют программу в criticalError-модалку и вешают сервер.
            // dir/file нужны приему для пути текста (base/dir/file),
            // crypt — чтобы новая запись создавалась с тем же флагом
            static const char *fieldNames[] = {
                "name", "author", "url", "tags", "ctime",
                "block", "dir", "file", "crypt"
            };

            for(const char *field : fieldNames)
            {
                const QString name=QString::fromLatin1(field);
                info.fields.insert(name, record->getNaturalFieldSource(name));
            }

            const QString dir=record->getNaturalFieldSource(QStringLiteral("dir"));
            const QString file=record->getNaturalFieldSource(QStringLiteral("file"));
            const QString textPath=tetradir+QStringLiteral("/base/")
                                   +dir+QStringLiteral("/")+file;

            // Аттачи записи: id, имя, размер, хеш
            AttachTableData attachTable=record->getAttachTable();

            for(int i=0; i<attachTable.size(); ++i)
            {
                SyncAttachInfo att;
                att.id=attachTable.getIdByRow(i);
                att.fileName=attachTable.getAbsoluteInnerFileName(i);
                att.size=attachTable.getFileSize(i);
                att.hash=sha256File(att.fileName);

                // Тип и цель нужны чтобы восстановить запись таблицы как
                // была (файл vs ссылка). Хеш всегда по содержимому.
                // link читается только у link-аттачей: у файловых getField
                // роняет criticalError-модалку и вешает сервер
                const Attach rowAttach=attachTable.getAttach(att.id);
                att.type=rowAttach.getField(QStringLiteral("type"));
                att.link=(att.type==QStringLiteral("link"))
                    ? rowAttach.getField(QStringLiteral("link"))
                    : QString();

                info.attaches << att;
            }

            if(sidecar.hasEntry(id))
            {
                const SyncSidecar::Entry e=sidecar.entry(id);
                info.mtime=e.mtime;

                // Хеш из сайдкара, но если его нет (старая запись) — живьем
                info.hash=e.hash.isEmpty() ? sha256File(textPath) : e.hash;
            }
            else
            {
                info.hash=sha256File(textPath);
                info.mtime=QFileInfo(textPath).lastModified()
                             .toMSecsSinceEpoch();
            }

            out.notes.insert(id, info);
        }
    }

    out.tombstones=sidecar.tombstones();

    // Сколько чего собрали и за сколько: вис манифеста раньше ловили
    // только гаданием (был criticalError на вычислимых полях)
    if(syncDebugEnabled())
        qDebug() << "SyncManifest: built branches=" << out.branches.size()
                 << "notes=" << out.notes.size()
                 << "tombs=" << out.tombstones.size()
                 << "in" << timer.elapsed() << "ms";

    return out;
}


QJsonObject SyncManifest::toJson(void) const
{
    QJsonObject root;
    QJsonObject branchesObj;

    for(auto it=branches.constBegin(); it!=branches.constEnd(); ++it)
    {
        QJsonObject b;
        b[QStringLiteral("parent")]=it.value().parentId;
        b[QStringLiteral("mtime")]=it.value().mtime;
        b[QStringLiteral("hash")]=it.value().hash;

        QJsonObject fields;

        for(auto fit=it.value().fields.constBegin();
            fit!=it.value().fields.constEnd(); ++fit)
            fields.insert(fit.key(), fit.value());

        b[QStringLiteral("fields")]=fields;
        branchesObj.insert(it.key(), b);
    }

    QJsonObject notesObj;

    for(auto it=notes.constBegin(); it!=notes.constEnd(); ++it)
    {
        QJsonObject n;
        n[QStringLiteral("branch")]=it.value().branchId;
        n[QStringLiteral("mtime")]=it.value().mtime;
        n[QStringLiteral("hash")]=it.value().hash;
        n[QStringLiteral("crypt")]=it.value().crypt;

        QJsonObject fields;

        for(auto fit=it.value().fields.constBegin();
            fit!=it.value().fields.constEnd(); ++fit)
            fields.insert(fit.key(), fit.value());

        n[QStringLiteral("fields")]=fields;

        QJsonArray attaches;

        for(const SyncAttachInfo &att : it.value().attaches)
        {
            QJsonObject a;
            a[QStringLiteral("id")]=att.id;
            a[QStringLiteral("file")]=QFileInfo(att.fileName).fileName();
            a[QStringLiteral("type")]=att.type;
            a[QStringLiteral("link")]=att.link;
            a[QStringLiteral("size")]=att.size;
            a[QStringLiteral("hash")]=att.hash;
            attaches.append(a);
        }

        n[QStringLiteral("attaches")]=attaches;
        notesObj.insert(it.key(), n);
    }

    QJsonObject tombs;

    for(auto it=tombstones.constBegin(); it!=tombstones.constEnd(); ++it)
        tombs.insert(it.key(), it.value());

    root[QStringLiteral("branches")]=branchesObj;
    root[QStringLiteral("notes")]=notesObj;
    root[QStringLiteral("tombstones")]=tombs;
    return root;
}


SyncManifest SyncManifest::fromJson(const QJsonObject &obj)
{
    SyncManifest out;

    const QJsonObject branches=obj.value(QStringLiteral("branches")).toObject();

    for(auto it=branches.constBegin(); it!=branches.constEnd(); ++it)
    {
        const QJsonObject b=it.value().toObject();
        SyncBranchInfo info;
        info.id=it.key();
        info.parentId=b.value(QStringLiteral("parent")).toString();
        info.mtime=b.value(QStringLiteral("mtime")).toVariant().toLongLong();
        info.hash=b.value(QStringLiteral("hash")).toString();

        const QJsonObject fields=b.value(QStringLiteral("fields")).toObject();

        for(auto fit=fields.constBegin(); fit!=fields.constEnd(); ++fit)
            info.fields.insert(fit.key(), fit.value().toString());

        out.branches.insert(it.key(), info);
    }

    const QJsonObject notes=obj.value(QStringLiteral("notes")).toObject();

    for(auto it=notes.constBegin(); it!=notes.constEnd(); ++it)
    {
        const QJsonObject n=it.value().toObject();
        SyncNoteInfo info;
        info.id=it.key();
        info.branchId=n.value(QStringLiteral("branch")).toString();
        info.mtime=n.value(QStringLiteral("mtime")).toVariant().toLongLong();
        info.hash=n.value(QStringLiteral("hash")).toString();
        info.crypt=n.value(QStringLiteral("crypt")).toBool();

        const QJsonObject fields=n.value(QStringLiteral("fields")).toObject();

        for(auto fit=fields.constBegin(); fit!=fields.constEnd(); ++fit)
            info.fields.insert(fit.key(), fit.value().toString());

        const QJsonArray attaches=n.value(QStringLiteral("attaches")).toArray();

        for(const QJsonValue &value : attaches)
        {
            const QJsonObject a=value.toObject();
            SyncAttachInfo att;
            att.id=a.value(QStringLiteral("id")).toString();
            att.fileName=a.value(QStringLiteral("file")).toString();
            att.type=a.value(QStringLiteral("type")).toString();
            att.link=a.value(QStringLiteral("link")).toString();
            att.size=a.value(QStringLiteral("size")).toVariant().toLongLong();
            att.hash=a.value(QStringLiteral("hash")).toString();
            info.attaches << att;
        }

        out.notes.insert(it.key(), info);
    }

    const QJsonObject tombs=obj.value(QStringLiteral("tombstones")).toObject();

    for(auto it=tombs.constBegin(); it!=tombs.constEnd(); ++it)
        out.tombstones.insert(it.key(), it.value().toVariant().toLongLong());

    return out;
}


bool SyncPlan::empty(void) const
{
    return uploadNotes.isEmpty() && downloadNotes.isEmpty() &&
           uploadBranches.isEmpty() && downloadBranches.isEmpty() &&
           deleteLocal.isEmpty() && conflicts.isEmpty();
}


// Наборы аттачей равны: совпадают id с хешами. Имена файлов и размеры
// не сравниваем: хеш контента решает, остальное косметика
static bool attachSetsEqual(const QList<SyncAttachInfo> &a,
                            const QList<SyncAttachInfo> &b)
{
    if(a.size()!=b.size())
        return false;

    QMap<QString, QString> aHashes;

    for(const SyncAttachInfo &att : a)
        aHashes.insert(att.id, att.hash);

    for(const SyncAttachInfo &att : b)
    {
        if(!aHashes.contains(att.id))
            return false;

        if(aHashes.value(att.id)!=att.hash)
            return false;
    }

    return true;
}


SyncPlan SyncManifestBuilder::compare(const SyncManifest &local,
                                      const SyncManifest &remote,
                                      bool isHub)
{
    Q_UNUSED(isHub);
    SyncPlan plan;

    // Чужие tombstone'ы: у нас такое есть — удалить локально.
    // Нет — нечего делать (уже удалено или никогда не было)
    for(auto it=remote.tombstones.constBegin();
        it!=remote.tombstones.constEnd(); ++it)
    {
        if(local.notes.contains(it.key()) ||
           local.branches.contains(it.key()))
            plan.deleteLocal << it.key();
    }

    // Свои tombstone'ы вторая сторона отработает сама по нашему манифесту

    // Ветки: нет там — загрузить туда; нет здесь — скачать оттуда;
    // хеши разные — побеждает больший mtime, равные — конфликт
    for(auto it=remote.branches.constBegin();
        it!=remote.branches.constEnd(); ++it)
    {
        if(local.tombstones.contains(it.key()))
            continue; // мы удалили — наше удаление уедет к ним tombstone'ом

        if(!local.branches.contains(it.key()))
        {
            plan.downloadBranches << it.key();
            continue;
        }

        const SyncBranchInfo &ours=local.branches.value(it.key());
        const SyncBranchInfo &theirs=it.value();

        if(ours.hash==theirs.hash)
            continue;

        if(ours.mtime>theirs.mtime)
            plan.uploadBranches << it.key();
        else if(theirs.mtime>ours.mtime)
            plan.downloadBranches << it.key();
        else
            plan.conflicts << it.key();
    }

    for(auto it=local.branches.constBegin();
        it!=local.branches.constEnd(); ++it)
    {
        if(remote.tombstones.contains(it.key()))
        {
            plan.deleteLocal << it.key();
            continue;
        }

        if(!remote.branches.contains(it.key()))
            plan.uploadBranches << it.key();
    }

    // Заметки — та же логика
    for(auto it=remote.notes.constBegin();
        it!=remote.notes.constEnd(); ++it)
    {
        if(local.tombstones.contains(it.key()))
            continue;

        if(!local.notes.contains(it.key()))
        {
            plan.downloadNotes << it.key();
            continue;
        }

        const SyncNoteInfo &ours=local.notes.value(it.key());
        const SyncNoteInfo &theirs=it.value();

        if(ours.hash==theirs.hash && attachSetsEqual(ours.attaches,
                                                    theirs.attaches))
            continue;

        if(ours.mtime>theirs.mtime)
            plan.uploadNotes << it.key();
        else if(theirs.mtime>ours.mtime)
            plan.downloadNotes << it.key();
        else
            plan.conflicts << it.key();
    }

    for(auto it=local.notes.constBegin();
        it!=local.notes.constEnd(); ++it)
    {
        if(remote.tombstones.contains(it.key()))
        {
            plan.deleteLocal << it.key();
            continue;
        }

        if(!remote.notes.contains(it.key()))
            plan.uploadNotes << it.key();
    }

    plan.deleteLocal.removeDuplicates();
    plan.conflicts.removeDuplicates();
    return plan;
}

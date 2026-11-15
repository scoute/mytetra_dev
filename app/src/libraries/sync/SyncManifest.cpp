#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>
#include <QSet>
#include <algorithm>

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


// Хеш полей заметки + ее ветки: переименования и переносы меняют хеш
// и потому синхронизируются (хеш контента их не видит).
// Канонизация как у веток: crypt/block "0" == "" (старые базы),
// пустые значения не хешируются (round-trip их роняет/возвращает)
static QString hashNoteFields(const QMap<QString, QString> &fields,
                              const QString &branchId)
{
    QByteArray canonical;

    for(auto it=fields.constBegin(); it!=fields.constEnd(); ++it)
    {
        QString value=it.value();

        if((it.key()==QStringLiteral("crypt")
            || it.key()==QStringLiteral("block"))
           && value==QStringLiteral("0"))
            value.clear();

        if(value.isEmpty())
            continue;

        canonical+=it.key().toUtf8();
        canonical+='=';
        canonical+=value.toUtf8();
        canonical+='\n';
    }

    canonical+="branch=";
    canonical+=branchId.toUtf8();
    canonical+='\n';

    return QString::fromLatin1(
        QCryptographicHash::hash(canonical,
                                 QCryptographicHash::Sha256).toHex());
}


// Покрыт ли id tombstone'ом: сам или любой предок по цепочке parents.
// parents: id -> parentId того же манифеста, чья цепочка проверяется
static bool coveredByTombs(const QString &id,
                            const QMap<QString, QString> &parents,
                            const QMap<QString, qint64> &tombs)
{
    QString cur=id;
    QSet<QString> seen;

    while(!cur.isEmpty() && !seen.contains(cur))
    {
        seen.insert(cur);

        if(tombs.contains(cur))
            return true;

        cur=parents.value(cur);
    }

    return false;
}


// Глубина ветки по цепочке parentId (корень — 0)
static int branchDepth(const QMap<QString, SyncBranchInfo> &branches,
                       const QString &id)
{
    int depth=0;
    QString cur=id;
    QSet<QString> seen;

    while(true)
    {
        auto it=branches.constFind(cur);

        if(it==branches.constEnd())
            break;

        const QString parent=it.value().parentId;

        if(parent.isEmpty() || seen.contains(parent))
            break;

        seen.insert(parent);
        cur=parent;
        depth++;

        if(depth>10000)
            break; // паранойя от циклов
    }

    return depth;
}


// Сортировка ид веток: сначала родители, потом дети (stable —
// детерминированность плана при равной глубине сохраняется)
static void orderBranchesParentsFirst(
    const QMap<QString, SyncBranchInfo> &branches, QStringList &ids)
{
    std::stable_sort(ids.begin(), ids.end(),
                     [&](const QString &a, const QString &b) {
        return branchDepth(branches, a) < branchDepth(branches, b);
    });
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

        // Хеш ветки — всегда живьем по полям+родителю: поля все в памяти,
        // это дешево, зато переезды веток детектятся даже по протухшему
        // сайдкару (а он при перемещениях и протухает).
        // mtime — из сайдкара (там метки правок), без записи — 0
        info.hash=SyncSidecar::hashBranchFields(info.fields, info.parentId);

        if(sidecar.hasEntry(id))
            info.mtime=sidecar.entry(id).mtime;
        else
            info.mtime=0;

        out.branches.insert(id, info);
    }

    // parentId в формате провода: корень "0" наружу не отдаем,
    // иначе разъедется с serializeBranch (там "0" -> "")
    const QString childParent=(id==QStringLiteral("0")) ? QString() : id;

    for(int i=0; i<item->childCount(); ++i)
        collectBranches(item->child(i), childParent, out, sidecar);
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
        int skippedEmpty=0;

        for(const QString &id : allIds->values())
        {
            // Запись без id (битая/импортная, поля нет в XML): она невидима
            // поиску модели и уронила бы сессию через "bad id" в serialize.
            // Пропускаем честно с одной строкой в лог, а не молча
            if(id.isEmpty())
            {
                skippedEmpty++;
                continue;
            }

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

            // Хеш полей+ветки — всегда живьем (поля в памяти, дешево):
            // переименования и переносы детектятся без сайдкара
            info.fhash=hashNoteFields(info.fields, info.branchId);

            // Аттачи записи: id, имя, размер, хеш
            AttachTableData attachTable=record->getAttachTable();

            for(int i=0; i<attachTable.size(); ++i)
            {
                SyncAttachInfo att;
                att.id=attachTable.getIdByRow(i);

                const Attach rowAttach=attachTable.getAttach(att.id);
                att.type=rowAttach.getField(QStringLiteral("type"));

                // Ссылка: контента нет, хеш — по строке цели.
                // Иначе sha256File по отсутствующей цели давал бы пустоту
                // с обеих сторон (ложное равенство) или рвало бы сессию
                if(att.type==QStringLiteral("link"))
                {
                    att.link=rowAttach.getField(QStringLiteral("link"));
                    att.fileName=rowAttach.getField(QStringLiteral("fileName"));
                    att.size=attachTable.getFileSize(i);
                    att.hash=sha256Bytes(att.link.toUtf8());
                }
                else
                {
                    att.fileName=attachTable.getAbsoluteInnerFileName(i);
                    att.size=attachTable.getFileSize(i);
                    att.hash=sha256File(att.fileName);
                    att.link=QString();
                }

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

        if(skippedEmpty>0)
            qDebug() << "SyncManifest: skipped records without id:"
                     << skippedEmpty << "(such notes stay local-only, "
                     "recreate them to sync)";
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
        n[QStringLiteral("fhash")]=it.value().fhash;
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
        info.fhash=n.value(QStringLiteral("fhash")).toString();
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

    // Родительские цепочки для проверки покрытия tombstone'ами:
    // ветка/заметка под удаленной веткой — часть удаленного поддерева
    QMap<QString, QString> localParents;
    for(auto it=local.branches.constBegin();
        it!=local.branches.constEnd(); ++it)
        localParents.insert(it.key(), it.value().parentId);

    QMap<QString, QString> localNoteParents;
    for(auto it=local.notes.constBegin();
        it!=local.notes.constEnd(); ++it)
        localNoteParents.insert(it.key(), it.value().branchId);

    QMap<QString, QString> remoteParents;
    for(auto it=remote.branches.constBegin();
        it!=remote.branches.constEnd(); ++it)
        remoteParents.insert(it.key(), it.value().parentId);

    QMap<QString, QString> remoteNoteParents;
    for(auto it=remote.notes.constBegin();
        it!=remote.notes.constEnd(); ++it)
        remoteNoteParents.insert(it.key(), it.value().branchId);

    // Чужие tombstone'ы с правилом edit-wins: наша правка новее чужого
    // удаления — побеждает правка (зальем к ним, воскресят).
    // Равный mtime — удаление (осознанное действие бьет ничью).
    // Иначе удалить локально
    for(auto it=remote.tombstones.constBegin();
        it!=remote.tombstones.constEnd(); ++it)
    {
        const QString id=it.key();
        const qint64 tombTime=it.value();

        if(local.notes.contains(id))
        {
            if(local.notes.value(id).mtime>tombTime)
                plan.uploadNotes << id;
            else
                plan.deleteLocal << id;
        }
        else if(local.branches.contains(id))
        {
            if(local.branches.value(id).mtime>tombTime)
                plan.uploadBranches << id;
            else
                plan.deleteLocal << id;
        }
    }

    // Наши tombstone'ы против их живых с тем же edit-wins: их правка новее
    // нашего удаления — качаем к себе (воскрешаем), tomb им не пушим
    // (pushOwnTombstones ту же проверку делает сам перед отправкой).
    // Tombstone ветки покрывает поддерево: их заметки/дети под нашей
    // удаленной веткой не качаются (наше удаление поддерева в силе)
    for(auto it=local.tombstones.constBegin();
        it!=local.tombstones.constEnd(); ++it)
    {
        const QString id=it.key();
        const qint64 tombTime=it.value();

        if(remote.notes.contains(id)
           && remote.notes.value(id).mtime>tombTime)
            plan.downloadNotes << id;
        else if(remote.branches.contains(id)
                && remote.branches.value(id).mtime>tombTime)
            plan.downloadBranches << id;
    }

    // Ветки: нет там — загрузить туда; нет здесь — скачать оттуда;
    // хеши разные — побеждает больший mtime, равные — конфликт
    for(auto it=remote.branches.constBegin();
        it!=remote.branches.constEnd(); ++it)
    {
        if(local.tombstones.contains(it.key()))
            continue; // наше удаление в силе (edit-wins решен выше)

        if(coveredByTombs(it.value().parentId, remoteParents,
                           local.tombstones))
            continue; // чужое под нашим удаленным поддеревом — не качаем

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
            continue; // решено в tomb-цикле выше (edit-wins/deleteLocal)

        if(coveredByTombs(it.value().parentId, localParents,
                           remote.tombstones))
            continue; // наше под их удаленным поддеревом — не заливаем

        if(!remote.branches.contains(it.key()))
            plan.uploadBranches << it.key();
    }

    // Заметки — та же логика; равенство — по контенту, полям+ветке и аттачам
    for(auto it=remote.notes.constBegin();
        it!=remote.notes.constEnd(); ++it)
    {
        if(local.tombstones.contains(it.key()))
            continue; // наше удаление в силе (edit-wins решен выше)

        if(local.tombstones.contains(it.value().branchId))
            continue; // чужое под нашим удаленным поддеревом — не качаем

        if(!local.notes.contains(it.key()))
        {
            plan.downloadNotes << it.key();
            continue;
        }

        const SyncNoteInfo &ours=local.notes.value(it.key());
        const SyncNoteInfo &theirs=it.value();

        if(ours.hash==theirs.hash && ours.fhash==theirs.fhash
           && attachSetsEqual(ours.attaches, theirs.attaches))
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
        if(remote.tombstones.contains(it.key())
           || remote.tombstones.contains(it.value().branchId))
            continue; // решено в tomb-цикле / под их удаленным поддеревом

        if(!remote.notes.contains(it.key()))
            plan.uploadNotes << it.key();
    }

    plan.deleteLocal.removeDuplicates();
    plan.conflicts.removeDuplicates();

    // Порядок веток: родители раньше детей. Иначе принимающая сторона
    // не найдет родителя в createSyncedBranch и пропустит ветку —
    // внешне это "синк только вглубь на 1". Глубина считается по цепочке
    // parentId в том же манифесте; битые родители/циклы дают 0
    orderBranchesParentsFirst(local.branches, plan.uploadBranches);
    orderBranchesParentsFirst(remote.branches, plan.downloadBranches);

    return plan;
}

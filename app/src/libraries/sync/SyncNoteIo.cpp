#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonArray>
#include <QModelIndex>
#include <QModelIndexList>

#include "SyncNoteIo.h"
#include "SyncSidecar.h"
#include "SyncManifest.h"
#include "libraries/FixedParameters.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/attachTable/AttachTableData.h"
#include "models/attachTable/Attach.h"


namespace SyncNoteIo
{
namespace
{

QByteArray readBytes(const QString &path, bool &ok)
{
    QFile file(path);
    ok=file.open(QIODevice::ReadOnly);

    if(!ok)
        return QByteArray();

    return file.readAll();
}


bool writeBytesAtomic(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);

    if(!file.open(QIODevice::WriteOnly))
        return false;

    if(file.write(data)!=data.size())
        return false;

    return file.commit();
}

} // namespace


QJsonObject serializeNote(KnowTreeModel *model, const QString &tetradir,
                          const QString &id)
{
    QJsonObject answer;

    if(model==nullptr || id.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad id");
        return answer;
    }

    Record *record=model->getRecord(id);

    if(record==nullptr)
    {
        answer[QStringLiteral("error")]=QStringLiteral("not found");
        return answer;
    }

    answer[QStringLiteral("id")]=id;
    answer[QStringLiteral("crypt")]=
        (record->getNaturalFieldSource(QStringLiteral("crypt"))==QStringLiteral("1"));

    // Синхронизируются только натуральные поля (см. SyncManifestBuilder:
    // вычислимые hasAttach/attachCount роняют criticalError).
    // dir/file нужны приему для пути текста (base/dir/file),
    // crypt — чтобы новая запись создавалась с тем же флагом
    static const char *fieldNames[] = {
        "name", "author", "url", "tags", "ctime",
        "block", "dir", "file", "crypt"
    };

    QJsonObject fields;

    for(const char *field : fieldNames)
    {
        const QString name=QString::fromLatin1(field);
        fields.insert(name, record->getNaturalFieldSource(name));
    }

    answer[QStringLiteral("fields")]=fields;

    const QString dir=record->getNaturalFieldSource(QStringLiteral("dir"));
    const QString file=record->getNaturalFieldSource(QStringLiteral("file"));
    const QString textPath=tetradir+QStringLiteral("/base/")
                           +dir+QStringLiteral("/")+file;

    // mtime/hash для adopt на принимающей стороне
    {
        QString entryHash;
        qint64 entryMtime=0;

        if(SyncSidecar::instance().isLoaded() &&
           SyncSidecar::instance().hasEntry(id))
        {
            const SyncSidecar::Entry e=SyncSidecar::instance().entry(id);
            entryHash=e.hash;
            entryMtime=e.mtime;
        }

        if(entryHash.isEmpty())
        {
            entryHash=SyncManifestBuilder::sha256File(textPath);

            if(entryMtime==0)
                entryMtime=QFileInfo(textPath).lastModified()
                             .toMSecsSinceEpoch();
        }

        answer[QStringLiteral("mtime")]=entryMtime;
        answer[QStringLiteral("hash")]=entryHash;
    }

    bool ok=false;
    const QByteArray textBytes=readBytes(textPath, ok);

    if(!ok)
    {
        answer[QStringLiteral("error")]=QStringLiteral("no text");
        return answer;
    }

    answer[QStringLiteral("text")]=QString::fromLatin1(textBytes.toBase64());

    QJsonArray attaches;
    AttachTableData attachTable=record->getAttachTable();

    for(int i=0; i<attachTable.size(); ++i)
    {
        const QString absPath=attachTable.getAbsoluteInnerFileName(i);
        const QByteArray data=readBytes(absPath, ok);

        if(!ok)
        {
            answer[QStringLiteral("error")]=QStringLiteral("no attach file");
            return answer;
        }

        const Attach rowAttach=attachTable.getAttach(attachTable.getIdByRow(i));
        const QString attachType=rowAttach.getField(QStringLiteral("type"));

        QJsonObject a;
        a[QStringLiteral("id")]=attachTable.getIdByRow(i);
        a[QStringLiteral("file")]=QFileInfo(absPath).fileName();
        a[QStringLiteral("type")]=attachType;
        a[QStringLiteral("link")]=(attachType==QStringLiteral("link"))
            ? rowAttach.getField(QStringLiteral("link"))
            : QString();
        a[QStringLiteral("size")]=attachTable.getFileSize(i);
        a[QStringLiteral("data")]=QString::fromLatin1(data.toBase64());
        attaches.append(a);
    }

    answer[QStringLiteral("attaches")]=attaches;
    return answer;
}


QJsonObject serializeBranch(KnowTreeModel *model, const QString &id)
{
    QJsonObject answer;

    if(model==nullptr || id.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad id");
        return answer;
    }

    TreeItem *item=model->getItemById(id);

    if(item==nullptr)
    {
        answer[QStringLiteral("error")]=QStringLiteral("not found");
        return answer;
    }

    QString parentId;
    TreeItem *parentItem=item->parent();

    if(parentItem!=nullptr)
    {
        const QString parentFieldId=
            parentItem->getField(QStringLiteral("id"));

        if(parentFieldId!=QStringLiteral("0"))
            parentId=parentFieldId;
    }

    answer[QStringLiteral("id")]=id;
    answer[QStringLiteral("parent")]=parentId;

    QJsonObject fields;
    const QMap<QString, QString> allFields=item->getAllFieldsDirect();

    for(auto it=allFields.constBegin(); it!=allFields.constEnd(); ++it)
        fields.insert(it.key(), it.value());

    answer[QStringLiteral("fields")]=fields;

    if(SyncSidecar::instance().isLoaded() &&
       SyncSidecar::instance().hasEntry(id))
    {
        const SyncSidecar::Entry e=SyncSidecar::instance().entry(id);
        answer[QStringLiteral("mtime")]=e.mtime;
        answer[QStringLiteral("hash")]=e.hash;
    }
    else
    {
        // Живьем: mtime неоткуда взять, хеш по полям. Досчитается при сейве
        answer[QStringLiteral("mtime")]=0;
        answer[QStringLiteral("hash")]=
            SyncSidecar::hashBranchFields(allFields);
    }

    return answer;
}


QString applyNote(KnowTreeModel *model, const QString &tetradir,
                  const QJsonObject &request)
{
    const QString id=request.value(QStringLiteral("id")).toString();
    const QString branchId=request.value(QStringLiteral("branch")).toString();

    if(model==nullptr || id.isEmpty() || branchId.isEmpty())
        return QStringLiteral("bad request");

    TreeItem *branchItem=model->getItemById(branchId);

    if(branchItem==nullptr)
        return QStringLiteral("no branch");

    RecordTableData *table=branchItem->recordtableGetTableData();

    if(table==nullptr)
        return QStringLiteral("no table");

    const QByteArray textBytes=
        QByteArray::fromBase64(request.value(QStringLiteral("text"))
                               .toString().toLatin1());

    const QJsonObject fields=request.value(QStringLiteral("fields")).toObject();
    const QString dir=fields.value(QStringLiteral("dir")).toString();
    const QString file=fields.value(QStringLiteral("file")).toString();

    if(dir.isEmpty() || file.isEmpty())
        return QStringLiteral("bad record dir");

    const QString basePath=tetradir+QStringLiteral("/base/")
                           +dir+QStringLiteral("/")+file;

    if(!writeBytesAtomic(basePath, textBytes))
        return QStringLiteral("cannot write text");

    const QJsonArray attaches=request.value(QStringLiteral("attaches")).toArray();

    for(const QJsonValue &value : attaches)
    {
        const QJsonObject a=value.toObject();
        const QByteArray data=
            QByteArray::fromBase64(a.value(QStringLiteral("data"))
                                   .toString().toLatin1());

        if(!writeBytesAtomic(basePath.section(QLatin1Char('/'), 0, -2)
                             +QStringLiteral("/")
                             +a.value(QStringLiteral("file")).toString(), data))
            return QStringLiteral("cannot write attach");
    }

    const qint64 remoteMtime=request.value(QStringLiteral("mtime")).toVariant().toLongLong();
    const QString remoteHash=request.value(QStringLiteral("hash")).toString();
    const int pos=table->getPosById(id);

    if(pos>=0)
    {
        // Смена флага шифрования через синк запрещена (как у веток):
        // расшифровать без пароля нечем, молча портить нельзя
        {
            Record *existing=model->getRecord(id);

            if(existing==nullptr)
                return QStringLiteral("record lost");

            const QString oldCrypt=
                existing->getNaturalFieldSource(QStringLiteral("crypt"));
            const QString newCrypt=fields.value(QStringLiteral("crypt")).toString();

            if(oldCrypt!=newCrypt)
                return QStringLiteral("crypt change unsupported");
        }

        // Слепая запись сырого источника (шифротекст как есть, без пароля).
        // table->setField здесь нельзя: для crypt=1 он требует ключ
        // и роняет criticalError-модалку, вешая сервер
        {
            Record *record=model->getRecord(id);

            if(record==nullptr)
                return QStringLiteral("record lost");

            // Чужие/старые пиры могли прислать вычислимые поля (hasAttach и т.п.):
            // setField на них роняет criticalError, пропускаем молча
            for(auto it=fields.constBegin(); it!=fields.constEnd(); ++it)
                if(FixedParameters::isRecordFieldNatural(it.key()))
                    record->setNaturalFieldSource(it.key(), it.value().toString());
        }

        {
            Record *record=model->getRecord(id);

            if(record==nullptr)
                return QStringLiteral("record lost");

            AttachTableData *attachTable=record->getAttachTablePointer();
            attachTable->clear();

            for(const QJsonValue &value : attaches)
            {
                const QJsonObject a=value.toObject();
                Attach attach(attachTable);
                attach.setField(QStringLiteral("id"), a.value(QStringLiteral("id")).toString());
                attach.setField(QStringLiteral("type"), a.value(QStringLiteral("type")).toString());
                attach.setField(QStringLiteral("fileName"), a.value(QStringLiteral("file")).toString());

                if(!a.value(QStringLiteral("link")).toString().isEmpty())
                    attach.setField(QStringLiteral("link"), a.value(QStringLiteral("link")).toString());

                attachTable->addAttach(attach);
            }

            SyncSidecar::adopt(id, remoteMtime, remoteHash,
                               QStringLiteral("note"));
        }
    }
    else
    {
        Record record;

        for(auto it=fields.constBegin(); it!=fields.constEnd(); ++it)
            if(FixedParameters::isRecordFieldNatural(it.key()))
                record.setNaturalFieldSource(it.key(), it.value().toString());

        AttachTableData *attachTable=record.getAttachTablePointer();

        for(const QJsonValue &value : attaches)
        {
            const QJsonObject a=value.toObject();
            Attach attach(attachTable);
            attach.setField(QStringLiteral("id"), a.value(QStringLiteral("id")).toString());
            attach.setField(QStringLiteral("type"), a.value(QStringLiteral("type")).toString());
            attach.setField(QStringLiteral("fileName"), a.value(QStringLiteral("file")).toString());

            if(!a.value(QStringLiteral("link")).toString().isEmpty())
                attach.setField(QStringLiteral("link"), a.value(QStringLiteral("link")).toString());

            attachTable->addAttach(attach);
        }

        table->appendSyncedRecord(record);

        SyncSidecar::adopt(id, remoteMtime, remoteHash,
                           QStringLiteral("note"));
    }

    return QString();
}


QString applyBranch(KnowTreeModel *model, const QJsonObject &request)
{
    const QString id=request.value(QStringLiteral("id")).toString();

    if(model==nullptr || id.isEmpty())
        return QStringLiteral("bad request");

    QMap<QString, QString> fields;
    const QJsonObject fieldsObj=request.value(QStringLiteral("fields")).toObject();

    for(auto it=fieldsObj.constBegin(); it!=fieldsObj.constEnd(); ++it)
        fields.insert(it.key(), it.value().toString());

    const QString parentId=request.value(QStringLiteral("parent")).toString();
    const qint64 remoteMtime=request.value(QStringLiteral("mtime")).toVariant().toLongLong();
    const QString remoteHash=request.value(QStringLiteral("hash")).toString();

    TreeItem *existing=model->getItemById(id);

    if(existing!=nullptr)
    {
        const QString oldCrypt=existing->getField(QStringLiteral("crypt"));
        const QString newCrypt=fields.value(QStringLiteral("crypt"));

        if(oldCrypt!=newCrypt)
            return QStringLiteral("crypt change unsupported");

        existing->setField(QStringLiteral("name"), fields.value(QStringLiteral("name")));
        existing->setField(QStringLiteral("icon"), fields.value(QStringLiteral("icon")));
        existing->setField(QStringLiteral("ctime"), fields.value(QStringLiteral("ctime")));

        SyncSidecar::adopt(id, remoteMtime, remoteHash, QStringLiteral("branch"));
        return QString();
    }

    const QString created=model->createSyncedBranch(parentId, fields);

    if(created.isEmpty())
        return QStringLiteral("cannot create branch");

    SyncSidecar::adopt(id, remoteMtime, remoteHash, QStringLiteral("branch"));
    return QString();
}


QString applyDelete(KnowTreeModel *model, const QString &id, qint64 deletedAt)
{
    if(model==nullptr || id.isEmpty())
        return QStringLiteral("bad request");

    TreeItem *branchItem=model->getItemById(id);

    if(branchItem!=nullptr)
    {
        if(id==QStringLiteral("0"))
            return QStringLiteral("cannot delete root");

        QModelIndex index=model->getIndexByItem(branchItem);

        if(!index.isValid())
            return QStringLiteral("no index");

        {
            const QSharedPointer< QSet<QString> > subRecords=
                model->getRecordsIdList(branchItem);

            if(!subRecords.isNull())
                for(const QString &recordId : subRecords->values())
                    SyncSidecar::dropEntry(recordId);
        }

        QModelIndexList list;
        list << index;
        model->deleteItemsByModelIndexList(list);
        SyncSidecar::adoptTombstone(id, deletedAt);
        return QString();
    }

    const QStringList path=model->getRecordPath(id);

    if(path.isEmpty())
        return QString();

    TreeItem *branch=model->getItemById(path.last());

    if(branch==nullptr)
        return QStringLiteral("no branch");

    RecordTableData *table=branch->recordtableGetTableData();

    if(table==nullptr)
        return QStringLiteral("no table");

    const int pos=table->getPosById(id);

    if(pos>=0)
        table->deleteRecord(pos);

    SyncSidecar::adoptTombstone(id, deletedAt);
    return QString();
}

} // namespace SyncNoteIo

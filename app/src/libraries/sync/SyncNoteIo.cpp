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

// Канонический crypt: приложение пишет "" (или вовсе без атрибута),
// старые базы — "0". Семантика одна (не "1" = не шифровано), и хеш
// обязан считать их равными, иначе вечные фантомные расхождения:
// хеш с "0" никогда не сойдется с хешем с "". Нормализация — в одной
// точке, внутри hashBranchFields ниже; здесь только для сравнений
static QString normCrypt(const QString &crypt)
{
    return (crypt==QStringLiteral("0")) ? QString() : crypt;
}
namespace
{

// Собрать id поддерева: сама ветка, все подветки и все их записи.
// Нужно ДО удаления (после элементы недоступны). Записи — через модель,
// ветки — рекурсией по детям
void collectSubtreeIds(KnowTreeModel *model, TreeItem *item,
                       QStringList &branches, QStringList &records)
{
    if(model==nullptr || item==nullptr)
        return;

    branches << item->getField(QStringLiteral("id"));

    const QSharedPointer< QSet<QString> > ownRecords=
        model->getRecordsIdList(item);

    if(!ownRecords.isNull())
        for(const QString &recordId : ownRecords->values())
            records << recordId;

    for(int i=0; i<item->childCount(); ++i)
        collectSubtreeIds(model, item->child(i), branches, records);
}

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
    // crypt — чтобы новая запись создавалась с тем же флагом,
    // id — чтобы втянутая запись не осталась без ид (иначе следующая
    // сессия видит фантом "" и падает в serialize через "bad id")
    static const char *fieldNames[] = {
        "id", "name", "author", "url", "tags", "ctime",
        "block", "dir", "file", "crypt"
    };

    QJsonObject fields;

    for(const char *field : fieldNames)
    {
        const QString name=QString::fromLatin1(field);
        const QString value=record->getNaturalFieldSource(name);

        // Канонические пустые crypt/block не передаем: само приложение
        // такие атрибуты не пишет, а прием вставил бы их в XML пустыми —
        // побайтовое расхождение баз без разницы в данных
        if((name==QStringLiteral("crypt") || name==QStringLiteral("block"))
           && value.isEmpty())
            continue;

        fields.insert(name, value);
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
        const Attach rowAttach=attachTable.getAttach(attachTable.getIdByRow(i));
        const QString attachType=rowAttach.getField(QStringLiteral("type"));

        QJsonObject a;
        a[QStringLiteral("id")]=attachTable.getIdByRow(i);
        a[QStringLiteral("type")]=attachType;

        // Ссылка — не файл: контента нет, цель может вообще не существовать
        // (внешний путь, URL). Хеш — по строке цели, иначе любая ссылка
        // роняла бы всю сессию через "no attach file"
        if(attachType==QStringLiteral("link"))
        {
            const QString link=rowAttach.getField(QStringLiteral("link"));
            a[QStringLiteral("file")]=rowAttach.getField(QStringLiteral("fileName"));
            a[QStringLiteral("link")]=link;
            a[QStringLiteral("size")]=attachTable.getFileSize(i);
            a[QStringLiteral("data")]=QString();
            attaches.append(a);
            continue;
        }

        const QString absPath=attachTable.getAbsoluteInnerFileName(i);
        const QByteArray data=readBytes(absPath, ok);

        if(!ok)
        {
            answer[QStringLiteral("error")]=QStringLiteral("no attach file");
            return answer;
        }

        a[QStringLiteral("file")]=QFileInfo(absPath).fileName();
        a[QStringLiteral("link")]=QString();
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
        // Живьем: mtime неоткуда взять, хеш по полям+родителю.
        // Досчитается при сейве
        answer[QStringLiteral("mtime")]=0;
        answer[QStringLiteral("hash")]=
            SyncSidecar::hashBranchFields(allFields, parentId);
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

    const QJsonObject fields=request.value(QStringLiteral("fields")).toObject();
    const QString dir=fields.value(QStringLiteral("dir")).toString();
    const QString file=fields.value(QStringLiteral("file")).toString();

    if(dir.isEmpty() || file.isEmpty())
        return QStringLiteral("bad record dir");

    const int pos=table->getPosById(id);
    const QStringList curPath=model->getRecordPath(id);
    const bool needMove=(pos<0 && !curPath.isEmpty()
                         && curPath.last()!=branchId);

    // Проверки ДО записи на диск: иначе при отказе (смена шифрования)
    // файлы уже перезаписаны, а модель/сайдкар старые — молчаливое
    // расхождение навсегда (сайдкар приоритетнее живого хеша)
    if(pos>=0 && !needMove)
    {
        // Смена флага шифрования через синк запрещена (как у веток):
        // расшифровать без пароля нечем, молча портить нельзя
        Record *existing=model->getRecord(id);

        if(existing==nullptr)
            return QStringLiteral("record lost");

        const QString oldCrypt=
            existing->getNaturalFieldSource(QStringLiteral("crypt"));
        const QString newCrypt=fields.value(QStringLiteral("crypt")).toString();

        // "" и "0" — одно и то же (не шифровано), см. normCrypt
        if(normCrypt(oldCrypt)!=normCrypt(newCrypt))
            return QStringLiteral("crypt change unsupported");
    }

    if(needMove)
    {
        // Переезд записи между ветками: изъять из старой таблицы
        // (без диска/корзины/сайдкара — это не удаление) и добавить
        // в новую. Файлы лежат в dir записи и не трогаются
        TreeItem *oldBranch=model->getItemById(curPath.last());

        if(oldBranch==nullptr)
            return QStringLiteral("no old branch");

        RecordTableData *oldTable=oldBranch->recordtableGetTableData();

        if(oldTable==nullptr)
            return QStringLiteral("no old table");

        const int oldPos=oldTable->getPosById(id);

        if(oldPos<0)
            return QStringLiteral("record lost");

        Record moving;

        if(!oldTable->takeRecordForMove(oldPos, moving))
            return QStringLiteral("record lost");

        // Смена шифрования при переезде — тот же запрет. Запись уже
        // изъята (старой позиции нет): кладем в новую как есть, диск
        // не тронут, данные целы; сессия повторит и сойдется
        const QString oldCrypt=
            moving.getNaturalFieldSource(QStringLiteral("crypt"));
        const QString newCrypt=fields.value(QStringLiteral("crypt")).toString();

        if(normCrypt(oldCrypt)!=normCrypt(newCrypt))
        {
            table->appendSyncedRecord(moving);
            return QStringLiteral("crypt change unsupported");
        }

        table->appendSyncedRecord(moving);
    }

    const QByteArray textBytes=
        QByteArray::fromBase64(request.value(QStringLiteral("text"))
                               .toString().toLatin1());

    const QString basePath=tetradir+QStringLiteral("/base/")
                           +dir+QStringLiteral("/")+file;

    if(!writeBytesAtomic(basePath, textBytes))
        return QStringLiteral("cannot write text");

    const QJsonArray attaches=request.value(QStringLiteral("attaches")).toArray();

    for(const QJsonValue &value : attaches)
    {
        const QJsonObject a=value.toObject();

        // Ссылка — файла нет, писать нечего: только строка таблицы.
        // Писать пустой data в файл цели было бы порчей чужого пути
        if(a.value(QStringLiteral("type")).toString()==QStringLiteral("link"))
            continue;

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

    // После переезда запись уже в новой таблице: позиция пересчитывается
    const int finalPos=needMove ? table->getPosById(id) : pos;

    if(finalPos>=0)
    {
        // Слепая запись сырого источника (шифротекст как есть, без пароля).
        // table->setField здесь нельзя: для crypt=1 он требует ключ
        // и роняет criticalError-модалку, вешая сервер.
        // Смена шифрования проверена до записи на диск (см. выше)
        {
            Record *record=model->getRecord(id);

            if(record==nullptr)
                return QStringLiteral("record lost");

            // Чужие/старые пиры могли прислать вычислимые поля (hasAttach и т.п.):
            // setField на них роняет criticalError, пропускаем молча.
            // Пустые crypt/block тоже не ставим (канонический XML без них)
            for(auto it=fields.constBegin(); it!=fields.constEnd(); ++it)
            {
                if(!FixedParameters::isRecordFieldNatural(it.key()))
                    continue;

                if((it.key()==QStringLiteral("crypt")
                    || it.key()==QStringLiteral("block"))
                   && it.value().toString().isEmpty())
                    continue;

                record->setNaturalFieldSource(it.key(), it.value().toString());
            }
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
        {
            if(!FixedParameters::isRecordFieldNatural(it.key()))
                continue;

            if((it.key()==QStringLiteral("crypt")
                || it.key()==QStringLiteral("block"))
               && it.value().toString().isEmpty())
                continue;

            record.setNaturalFieldSource(it.key(), it.value().toString());
        }

        // id дублируем явно из верха запроса: старые пиры его в fields
        // не клали, а без ид запись-невидимка (фантом "" в манифесте).
        // appendSyncedRecord ждет запись "с готовым id" — даем его
        record.setNaturalFieldSource(QStringLiteral("id"), id);

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

    // id всегда из верха запроса: старые/чужие пиры могли не класть его
    // в fields, а без id создание ветки невозможно
    fields.insert(QStringLiteral("id"), id);

    const QString parentId=request.value(QStringLiteral("parent")).toString();
    const qint64 remoteMtime=request.value(QStringLiteral("mtime")).toVariant().toLongLong();
    const QString remoteHash=request.value(QStringLiteral("hash")).toString();

    TreeItem *existing=model->getItemById(id);

    if(existing!=nullptr)
    {
        const QString oldCrypt=existing->getField(QStringLiteral("crypt"));
        const QString newCrypt=fields.value(QStringLiteral("crypt"));

        // "" и "0" — одно и то же (не шифровано), см. normCrypt
        if(normCrypt(oldCrypt)!=normCrypt(newCrypt))
            return QStringLiteral("crypt change unsupported");

        // Слепая запись полей целиком (как у записей через setNaturalFieldSource):
        // setField для crypt=1 шифрует значение ПОВТОРНО (двойной шифртекст,
        // нечитаемо даже с верным паролем) или роняет criticalError без пароля
        // (headless: весь процесс хаба падает от одного пуша).
        // Принимаем все чужие поля (кроме id/crypt — сверены выше): хеш ветки
        // считается по всем полям, иначе расхождение не сойдется никогда
        QMap<QString, QString> rawFields=fields;
        rawFields.insert(QStringLiteral("id"), id);
        rawFields.insert(QStringLiteral("crypt"), oldCrypt);
        existing->setAllFieldDirect(rawFields);

        // Переезд: родитель в запросе отличается — перелинковываем
        // с сохранением id и поддерева. Без этого переезды веток
        // детектятся (хеш с parentId), но молча не применяются
        const QString newParent=request.value(QStringLiteral("parent")).toString();
        const QString curParent=existing->getParentId();
        const QString curParentWire=(curParent==QStringLiteral("0"))
            ? QString() : curParent;

        if(newParent!=curParentWire)
        {
            if(!model->moveSyncedBranch(id, newParent))
                return QStringLiteral("cannot move branch");
        }

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

        // Id поддерева — заранее: после удаления элементы недоступны.
        // Ветки — каскадом в tombstones (иначе дети воскресают у пира,
        // а их записи дают вечный "no branch" при каждом синке).
        // Записи — молча из entry (покрыты tombstone'ами веток, как в GUI).
        // Путь модели сам проставит свежие метки — ниже перезапишем
        // удаленным временем, иначе пинг-понг tombstone'ов
        QStringList subBranches;
        QStringList subRecords;
        collectSubtreeIds(model, branchItem, subBranches, subRecords);

        QModelIndexList list;
        list << index;
        model->deleteItemsByModelIndexList(list);

        for(const QString &branchId : subBranches)
            SyncSidecar::adoptTombstone(branchId, deletedAt);

        for(const QString &recordId : subRecords)
            SyncSidecar::dropEntry(recordId);

        return QString();
    }

    const QStringList path=model->getRecordPath(id);

    if(path.isEmpty())
    {
        // Уже нет (удалено раньше или никогда не было): tombstone все равно
        // запоминаем — иначе удаление неидемпотентно и при смене ролей
        // данные воскреснут
        SyncSidecar::adoptTombstone(id, deletedAt);
        return QString();
    }

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


namespace
{

// Среди детей parent найти одноименную ветку, чей id НЕ из clientIds
// ("ихняя" ветка: ее id и побеждает при слиянии)
TreeItem *findForeignChildByName(TreeItem *parent, const QString &name,
                                 const QSet<QString> &clientIds)
{
    for(int i=0; i<parent->childCount(); ++i)
    {
        TreeItem *child=parent->child(i);

        if(clientIds.contains(child->getField(QStringLiteral("id"))))
            continue;

        if(child->getField(QStringLiteral("name"))==name)
            return child;
    }

    return nullptr;
}


// Влить содержимое from в to (id to побеждает): наши подветки —
// рекурсивно по именам или переносом целиком, чужие/общие — переносом
// как есть. Записи переносятся все, одноименные остаются дублями.
// Оболочка from удаляется с tombstone'ом
void mergeBranchInto(KnowTreeModel *model, TreeItem *from, TreeItem *to,
                     const QSet<QString> &clientIds)
{
    // подветки — с конца: переносы и удаления меняют список детей
    for(int i=from->childCount()-1; i>=0; --i)
    {
        TreeItem *child=from->child(i);
        const QString childId=child->getField(QStringLiteral("id"));
        const QString childName=child->getField(QStringLiteral("name"));

        // сливаем только НАШИ ветки; общие по истории просто переезжают
        TreeItem *target=nullptr;

        if(clientIds.contains(childId))
            target=findForeignChildByName(to, childName, clientIds);

        if(target!=nullptr)
            mergeBranchInto(model, child, target, clientIds);
        else
            model->moveSyncedBranch(childId, to->getField(QStringLiteral("id")));
    }

    // записи from — все в to (заметки не сливаем: одноименные — дубли)
    RecordTableData *fromTable=from->recordtableGetTableData();
    RecordTableData *toTable=to->recordtableGetTableData();

    while(fromTable->size()>0)
    {
        Record record;

        if(!fromTable->takeRecordForMove(0, record))
            break;

        toTable->appendSyncedRecord(record);
    }

    // оболочка from пуста и не нужна (tombstone: ветка удалена)
    applyDelete(model, from->getField(QStringLiteral("id")),
                QDateTime::currentMSecsSinceEpoch());
}

} // namespace


QString mergeByName(KnowTreeModel *model, const QStringList &clientBranchIds)
{
    if(model==nullptr)
        return QStringLiteral("bad request");

    TreeItem *root=const_cast<TreeItem *>(model->getRootItemPublic());

    if(root==nullptr)
        return QStringLiteral("bad request");

    const QSet<QString> clientIds=clientBranchIds.toSet();

    // Верхний уровень: наши ветки вливаются в одноименные ихние;
    // без пары остаются как есть (просто новые ветки на хабе)
    for(int i=root->childCount()-1; i>=0; --i)
    {
        TreeItem *child=root->child(i);
        const QString childId=child->getField(QStringLiteral("id"));

        if(!clientIds.contains(childId))
            continue;

        TreeItem *target=findForeignChildByName(
            root, child->getField(QStringLiteral("name")), clientIds);

        if(target!=nullptr)
            mergeBranchInto(model, child, target, clientIds);
    }

    return QString();
}


QString wipeAll(KnowTreeModel *model)
{
    if(model==nullptr)
        return QStringLiteral("bad request");

    const TreeItem *root=model->getRootItemPublic();

    if(root==nullptr)
        return QStringLiteral("bad request");

    // Все ветки верхнего уровня уходят в корзину штатным удалением
    // (записи и поддеревья — с ними). Индексы держат указатели на
    // элементы, порядок не важен
    QModelIndexList list;

    for(int i=0; i<root->childCount(); ++i)
    {
        QModelIndex index=model->getIndexByItem(
            const_cast<TreeItem *>(root->child(i)));

        if(index.isValid())
            list << index;
    }

    if(!list.isEmpty())
        model->deleteItemsByModelIndexList(list);

    // Журнал — целиком: стертые локальные id не должны поехать пиру
    // tombstone'ами (это была бы порча чужих данных), а его содержимое
    // должно приехать свежим без edit-wins от нашей истории
    SyncSidecar::reset();

    return QString();
}

} // namespace SyncNoteIo

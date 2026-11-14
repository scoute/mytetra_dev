#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonArray>

#include "SyncHub.h"
#include "SyncServer.h"
#include "SyncHandshake.h"
#include "SyncManifest.h"
#include "SyncSidecar.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/attachTable/AttachTableData.h"
#include "models/attachTable/Attach.h"


SyncHub::SyncHub(KnowTreeModel *model, AppConfig *config,
                 SyncSidecar &sidecar, const QString &tetradir,
                 QObject *parent)
    : QObject(parent),
      model(model),
      config(config),
      sidecar(&sidecar),
      tetradir(tetradir)
{
    server=new SyncServer(this);
    SyncHandshake::registerRoutes(server, this->config);

    // Манифест локальной базы для спока
    server->addRoute(QStringLiteral("GET /manifest"),
                     [this](const QJsonObject &) -> QJsonObject {
        const SyncManifest manifest=
            SyncManifestBuilder::build(this->model, *this->sidecar,
                                       this->tetradir);
        return manifest.toJson();
    });

    // Заметка целиком: поля, блоб текста как лежит, аттачи байтами.
    // Шифрованные отдаются шифртекстом, ключ не нужен и не отдается
    server->addRoute(QStringLiteral("POST /pull-note"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return pullNote(request.value(QStringLiteral("id")).toString());
    });

    // Ветка целиком: поля + parentId
    server->addRoute(QStringLiteral("POST /pull-branch"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return pullBranch(request.value(QStringLiteral("id")).toString());
    });

    // Прием заметки целиком (создание или обновление, id сохраняются).
    // Ветка должна уже существовать: клиент шлет ветки раньше заметок
    server->addRoute(QStringLiteral("POST /push-note"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return applyNote(request);
    });
    // Прием ветки целиком (создание или обновление полей, id сохраняется)
    server->addRoute(QStringLiteral("POST /push-branch"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return applyBranch(request);
    });

    // Применить удаление по tombstone: ветка с поддеревом или заметка.
    // Время tombstone'а — удаленное, не локальное (иначе пинг-понг)
    server->addRoute(QStringLiteral("POST /push-delete"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return applyDelete(request.value(QStringLiteral("id")).toString(),
                           request.value(QStringLiteral("deletedAt")).toVariant().toLongLong());
    });

    // Фиксация сессии: сейв базы и перезагрузка моделей,
    // чтобы виды показали примененное. Вызывает клиент в конце плана
    server->addRoute(QStringLiteral("POST /commit"),
                     [this](const QJsonObject &) -> QJsonObject {
        this->model->save();
        this->model->reload();

        QJsonObject answer;
        answer[QStringLiteral("ok")]=true;
        return answer;
    });
}


SyncHub::~SyncHub(void)
{
    stop();
}


bool SyncHub::start(quint16 port)
{
    return server->start(port);
}


void SyncHub::stop(void)
{
    server->stop();
}


bool SyncHub::isRunning(void) const
{
    return server->isRunning();
}


quint16 SyncHub::port(void) const
{
    return server->port();
}


void SyncHub::setHub(bool hub)
{
    this->hub=hub;
}


bool SyncHub::isHub(void) const
{
    return hub;
}


static QByteArray readFileBytes(const QString &path, bool &ok)
{
    QFile file(path);
    ok=file.open(QIODevice::ReadOnly);

    if(!ok)
        return QByteArray();

    return file.readAll();
}


QJsonObject SyncHub::pullNote(const QString &id)
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

    // mtime/hash для adopt на принимающей стороне: из сайдкара,
    // нет записи — досчитать живьем (старые данные до первого сейва)
    {
        QString entryHash;
        qint64 entryMtime=0;

        if(sidecar->hasEntry(id))
        {
            const SyncSidecar::Entry e=sidecar->entry(id);
            entryHash=e.hash;
            entryMtime=e.mtime;
        }

        if(entryHash.isEmpty())
        {
            const QString dir0=record->getNaturalFieldSource(QStringLiteral("dir"));
            const QString file0=record->getNaturalFieldSource(QStringLiteral("file"));
            const QString textPath0=tetradir+QStringLiteral("/base/")
                                     +dir0+QStringLiteral("/")+file0;
            entryHash=SyncManifestBuilder::sha256File(textPath0);

            if(entryMtime==0)
                entryMtime=QFileInfo(textPath0).lastModified()
                             .toMSecsSinceEpoch();
        }

        answer[QStringLiteral("mtime")]=entryMtime;
        answer[QStringLiteral("hash")]=entryHash;
    }

    static const char *fieldNames[] = {
        "name", "author", "url", "tags", "ctime",
        "block", "hasAttach", "attachCount"
    };

    QJsonObject fields;

    for(const char *field : fieldNames)
    {
        const QString name=QString::fromLatin1(field);
        fields.insert(name, record->getNaturalFieldSource(name));
    }

    answer[QStringLiteral("fields")]=fields;

    // Блоб текста как лежит на диске: открытый текст или шифртекст.
    // Шифртекст отдается вслепую, ключ не нужен
    const QString dir=record->getNaturalFieldSource(QStringLiteral("dir"));
    const QString file=record->getNaturalFieldSource(QStringLiteral("file"));
    const QString textPath=tetradir+QStringLiteral("/base/")
                           +dir+QStringLiteral("/")+file;

    bool ok=false;
    const QByteArray textBytes=readFileBytes(textPath, ok);

    if(!ok)
    {
        answer[QStringLiteral("error")]=QStringLiteral("no text");
        return answer;
    }

    answer[QStringLiteral("text")]=
        QString::fromLatin1(textBytes.toBase64());

    // Аттачи байтами. Имена файлов на принимающей стороне не используются
    // для путей (кладем в свой каталог записи), только для восстановления имен
    QJsonArray attaches;
    AttachTableData attachTable=record->getAttachTable();

    for(int i=0; i<attachTable.size(); ++i)
    {
        const QString absPath=attachTable.getAbsoluteInnerFileName(i);
        const QByteArray data=readFileBytes(absPath, ok);

        if(!ok)
        {
            answer[QStringLiteral("error")]=QStringLiteral("no attach file");
            return answer;
        }

        QJsonObject a;
        a[QStringLiteral("id")]=attachTable.getIdByRow(i);
        a[QStringLiteral("fileName")]=QFileInfo(absPath).fileName();
        a[QStringLiteral("data")]=QString::fromLatin1(data.toBase64());
        attaches.append(a);
    }

    answer[QStringLiteral("attaches")]=attaches;
    return answer;
}


QJsonObject SyncHub::pullBranch(const QString &id)
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

    // Родитель корневого псевдо-элемента не синхронизируется:
    // верхнеуровневые ветки едут с пустым parent
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
    return answer;
}


static bool writeBytesAtomic(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);

    if(!file.open(QIODevice::WriteOnly))
        return false;

    if(file.write(data)!=data.size())
        return false;

    return file.commit();
}


QJsonObject SyncHub::applyNote(const QJsonObject &request)
{
    QJsonObject answer;

    const QString id=request.value(QStringLiteral("id")).toString();
    const QString branchId=request.value(QStringLiteral("branch")).toString();

    if(model==nullptr || id.isEmpty() || branchId.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad request");
        return answer;
    }

    TreeItem *branchItem=model->getItemById(branchId);

    if(branchItem==nullptr)
    {
        // Ветки нет — клиент нарушил порядок (ветки шлются раньше заметок)
        answer[QStringLiteral("error")]=QStringLiteral("no branch");
        return answer;
    }

    RecordTableData *table=branchItem->recordtableGetTableData();

    if(table==nullptr)
    {
        answer[QStringLiteral("error")]=QStringLiteral("no table");
        return answer;
    }

    // Текст как лежит (открытый или шифртекст — вслепую, ключ не нужен)
    const QByteArray textBytes=
        QByteArray::fromBase64(request.value(QStringLiteral("text"))
                               .toString().toLatin1());

    const QJsonObject fields=request.value(QStringLiteral("fields")).toObject();
    const QString dir=fields.value(QStringLiteral("dir")).toString();
    const QString file=fields.value(QStringLiteral("file")).toString();

    if(dir.isEmpty() || file.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad record dir");
        return answer;
    }

    // Файлы пишутся до обновления модели: не записалось — модель не трогаем
    const QString basePath=tetradir+QStringLiteral("/base/")+dir+QStringLiteral("/");

    if(!writeBytesAtomic(basePath+file, textBytes))
    {
        answer[QStringLiteral("error")]=QStringLiteral("cannot write text");
        return answer;
    }

    const QJsonArray attaches=request.value(QStringLiteral("attaches")).toArray();

    for(const QJsonValue &value : attaches)
    {
        const QJsonObject a=value.toObject();
        const QByteArray data=
            QByteArray::fromBase64(a.value(QStringLiteral("data"))
                                   .toString().toLatin1());

        if(!writeBytesAtomic(basePath+a.value(QStringLiteral("file")).toString(), data))
        {
            answer[QStringLiteral("error")]=QStringLiteral("cannot write attach");
            return answer;
        }
    }

        // Принимаем удаленную mtime/hash как свои: иначе следующий синк
    // погнал бы все обратно как "более новое". Локальное "сейчас"
    // здесь не пишется никогда
    const qint64 remoteMtime=request.value(QStringLiteral("mtime")).toVariant().toLongLong();
    const QString remoteHash=request.value(QStringLiteral("hash")).toString();

    const int pos=table->getPosById(id);

    if(pos>=0)
    {
        // Обновление существующей: поля как есть (для шифрованных —
        // шифртекст полей, пароль не нужен), текст и аттачи уже на диске
        for(auto it=fields.constBegin(); it!=fields.constEnd(); ++it)
            table->setField(it.key(), it.value().toString(), pos);

        Record *record=model->getRecord(id);

        if(record==nullptr)
        {
            answer[QStringLiteral("error")]=QStringLiteral("record lost");
            return answer;
        }

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

        SyncSidecar::adopt(id, remoteMtime, remoteHash, QStringLiteral("note"));
    }
    else
    {
        Record record;

        for(auto it=fields.constBegin(); it!=fields.constEnd(); ++it)
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

        SyncSidecar::adopt(id, remoteMtime, remoteHash, QStringLiteral("note"));
    }


    answer[QStringLiteral("ok")]=true;
    return answer;
}


QJsonObject SyncHub::applyBranch(const QJsonObject &request)
{
    QJsonObject answer;

    const QString id=request.value(QStringLiteral("id")).toString();

    if(model==nullptr || id.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad request");
        return answer;
    }

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
        // Обновление: имя, иконка, ctime. crypt смена состава —
        // вне MVP (нужен перешифр всего поддерева), отдаем ошибку.
        // Перемещение между родителями — тоже вне MVP, только поля
        const QString oldCrypt=existing->getField(QStringLiteral("crypt"));
        const QString newCrypt=fields.value(QStringLiteral("crypt"));

        if(oldCrypt!=newCrypt)
        {
            answer[QStringLiteral("error")]=QStringLiteral("crypt change unsupported");
            return answer;
        }

        existing->setField(QStringLiteral("name"), fields.value(QStringLiteral("name")));
        existing->setField(QStringLiteral("icon"), fields.value(QStringLiteral("icon")));
        existing->setField(QStringLiteral("ctime"), fields.value(QStringLiteral("ctime")));

        SyncSidecar::adopt(id, remoteMtime, remoteHash, QStringLiteral("branch"));
    }
    else
    {
        const QString created=model->createSyncedBranch(parentId, fields);

        if(created.isEmpty())
        {
            answer[QStringLiteral("error")]=QStringLiteral("cannot create branch");
            return answer;
        }

        SyncSidecar::adopt(id, remoteMtime, remoteHash, QStringLiteral("branch"));
    }

    answer[QStringLiteral("ok")]=true;
    return answer;
}


QJsonObject SyncHub::applyDelete(const QString &id, qint64 deletedAt)
{
    QJsonObject answer;

    if(model==nullptr || id.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad request");
        return answer;
    }

    // Ветка? Удаляем с поддеревом через существующий механизм,
    // tombstone уже есть (пришел вместе с планом), дублируем локально
    TreeItem *branchItem=model->getItemById(id);

    if(branchItem!=nullptr)
    {
        // Корневую псевдоветку не трогаем никогда
        if(id==QStringLiteral("0"))
        {
            answer[QStringLiteral("error")]=QStringLiteral("cannot delete root");
            return answer;
        }

        QModelIndex index=model->getIndexByItem(branchItem);

        if(!index.isValid())
        {
            answer[QStringLiteral("error")]=QStringLiteral("no index");
            return answer;
        }

        QModelIndexList list;
        list << index;

        // Записи поддерева: их покрывает tombstone ветки, локальные
        // entry выкидываем молча чтобы не висели мусором
        {
            const QSharedPointer< QSet<QString> > subRecords=
                model->getRecordsIdList(branchItem);

            if(!subRecords.isNull())
                for(const QString &recordId : subRecords->values())
                    SyncSidecar::dropEntry(recordId);
        }

        model->deleteItemsByModelIndexList(list);
        SyncSidecar::adoptTombstone(id, deletedAt);

        answer[QStringLiteral("ok")]=true;
        return answer;
    }

    // Иначе заметка: ищем таблицу через путь
    const QStringList path=model->getRecordPath(id);

    if(path.isEmpty())
    {
        // Нет ни ветки, ни заметки — уже удалено, ок
        answer[QStringLiteral("ok")]=true;
        return answer;
    }

    TreeItem *branch=model->getItemById(path.last());

    if(branch==nullptr)
    {
        answer[QStringLiteral("error")]=QStringLiteral("no branch");
        return answer;
    }

    RecordTableData *table=branch->recordtableGetTableData();

    if(table==nullptr)
    {
        answer[QStringLiteral("error")]=QStringLiteral("no table");
        return answer;
    }

    const int pos=table->getPosById(id);

    if(pos<0)
    {
        answer[QStringLiteral("ok")]=true;
        return answer;
    }

    table->deleteRecord(pos);
    SyncSidecar::adoptTombstone(id, deletedAt);

    answer[QStringLiteral("ok")]=true;
    return answer;
}

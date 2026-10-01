#include "SyncCore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/TreeScreen.h"
#include "libraries/helpers/ObjectHelper.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


SyncCore::SyncCore(void)
{
    m_model=nullptr;
}


SyncCore::~SyncCore(void)
{
}


void SyncCore::setModel(KnowTreeModel *model)
{
    m_model=model;
}


SyncStore &SyncCore::store(void)
{
    return m_store;
}


int SyncCore::protoVersion(void)
{
    return 1;
}


void SyncCore::collectSnapshot(QMap<QString, QMap<QString, QString> > &branches,
                               QMap<QString, QMap<QString, QString> > &records)
{
    branches.clear();
    records.clear();

    if(m_model==nullptr)
        return;

    TreeItem *root=m_model->getItem(QModelIndex());
    if(root==nullptr)
        return;

    QList<TreeItem *> stack;
    for(int i=0; i<root->childCount(); ++i)
        stack.append(root->child(i));

    while(!stack.isEmpty())
    {
        TreeItem *item=stack.takeLast();
        if(item==nullptr)
            continue;

        QString branchId=item->getField("id");

        QString parentId="0";
        if(item->parent()!=nullptr && item->parent()!=root)
            parentId=item->parent()->getField("id");

        QMap<QString, QString> branchInfo;
        branchInfo["parent"]=parentId;
        branchInfo["name"]=item->getField("name");
        branchInfo["hash"]=SyncStore::branchHash(item);
        branches[branchId]=branchInfo;

        RecordTableData *table=item->recordtableGetTableData();
        if(table!=nullptr)
        {
            for(unsigned int pos=0; pos<table->size(); ++pos)
            {
                QString recordId=table->getField("id", pos);

                QMap<QString, QString> recordInfo;
                recordInfo["branch"]=branchId;
                recordInfo["name"]=table->getField("name", pos);

                QMap<QString, QString> fields;
                fields["id"]=recordId;
                fields["name"]=table->getField("name", pos);
                fields["author"]=table->getField("author", pos);
                fields["url"]=table->getField("url", pos);
                fields["tags"]=table->getField("tags", pos);
                fields["ctime"]=table->getField("ctime", pos);
                fields["block"]=table->getField("block", pos);
                fields["crypt"]=table->getField("crypt", pos);

                recordInfo["hash"]=SyncStore::recordHash(fields, table->getField("dir", pos));
                records[recordId]=recordInfo;
            }
        }

        for(int i=0; i<item->childCount(); ++i)
            stack.append(item->child(i));
    }
}


bool SyncCore::findRecord(KnowTreeModel *model,
                          const QString &recordId,
                          TreeItem *&branchItem,
                          int &pos)
{
    branchItem=nullptr;
    pos=-1;

    if(model==nullptr)
        return false;

    TreeItem *root=model->getItem(QModelIndex());
    if(root==nullptr)
        return false;

    QList<TreeItem *> stack;
    for(int i=0; i<root->childCount(); ++i)
        stack.append(root->child(i));

    while(!stack.isEmpty())
    {
        TreeItem *item=stack.takeLast();
        if(item==nullptr)
            continue;

        RecordTableData *table=item->recordtableGetTableData();
        if(table!=nullptr)
        {
            int foundPos=table->getPosById(recordId);
            if(foundPos>=0)
            {
                branchItem=item;
                pos=foundPos;
                return true;
            }
        }

        for(int i=0; i<item->childCount(); ++i)
            stack.append(item->child(i));
    }

    return false;
}


QByteArray SyncCore::snapshot(void)
{
    if(m_model==nullptr)
        return "{\"error\":\"no model\"}";

    // Локальные правки подтверждаются и попадают в sidecar
    QMap<QString, QString> changedBranches;
    QMap<QString, QString> changedRecords;
    m_store.scan(m_model, changedBranches, changedRecords);

    QMapIterator<QString, QString> branchIt(changedBranches);
    while(branchIt.hasNext())
    {
        branchIt.next();
        m_store.touchBranch(branchIt.key(), branchIt.value());
    }

    QMapIterator<QString, QString> recordIt(changedRecords);
    while(recordIt.hasNext())
    {
        recordIt.next();
        m_store.touchRecord(recordIt.key(), recordIt.value());
    }
    m_store.save();

    QMap<QString, QMap<QString, QString> > branches;
    QMap<QString, QMap<QString, QString> > records;
    collectSnapshot(branches, records);

    QJsonObject snapshotObject;
    snapshotObject["rev"]=static_cast<int>(m_store.rev());

    QJsonArray branchList;
    QMapIterator<QString, QMap<QString, QString> > snapshotBranchIt(branches);
    while(snapshotBranchIt.hasNext())
    {
        snapshotBranchIt.next();

        QJsonObject branch;
        branch["id"]=snapshotBranchIt.key();
        branch["parent"]=snapshotBranchIt.value().value("parent");
        branch["name"]=snapshotBranchIt.value().value("name");
        branch["hash"]=snapshotBranchIt.value().value("hash");
        branchList.append(branch);
    }
    snapshotObject["branches"]=branchList;

    QJsonArray recordList;
    QMapIterator<QString, QMap<QString, QString> > snapshotRecordIt(records);
    while(snapshotRecordIt.hasNext())
    {
        snapshotRecordIt.next();

        QJsonObject record;
        record["id"]=snapshotRecordIt.key();
        record["branch"]=snapshotRecordIt.value().value("branch");
        record["name"]=snapshotRecordIt.value().value("name");
        record["hash"]=snapshotRecordIt.value().value("hash");
        recordList.append(record);
    }
    snapshotObject["records"]=recordList;

    return QJsonDocument(snapshotObject).toJson(QJsonDocument::Compact);
}


QByteArray SyncCore::pull(void)
{
    if(m_model==nullptr)
        return "{\"error\":\"no model\"}";

    // Прототип держит одно последнее подтвержденное состояние,
    // diff всегда против него. Для одного клиента этого достаточно
    QMap<QString, QString> changedBranches;
    QMap<QString, QString> changedRecords;
    m_store.scan(m_model, changedBranches, changedRecords);

    QMap<QString, QMap<QString, QString> > branches;
    QMap<QString, QMap<QString, QString> > records;
    collectSnapshot(branches, records);

    QJsonObject pullObject;
    pullObject["rev"]=static_cast<int>(m_store.rev());

    QJsonArray branchList;
    QMapIterator<QString, QString> branchIt(changedBranches);
    while(branchIt.hasNext())
    {
        branchIt.next();

        // Ветка могла быть удалена локально после scan, проверка
        if(!branches.contains(branchIt.key()))
            continue;

        QJsonObject branch;
        branch["id"]=branchIt.key();
        branch["parent"]=branches[branchIt.key()].value("parent");
        branch["name"]=branches[branchIt.key()].value("name");
        branch["hash"]=branchIt.value();
        branchList.append(branch);

        m_store.touchBranch(branchIt.key(), branchIt.value());
    }
    pullObject["branches"]=branchList;

    QJsonArray recordList;
    QMapIterator<QString, QString> recordIt(changedRecords);
    while(recordIt.hasNext())
    {
        recordIt.next();

        if(!records.contains(recordIt.key()))
            continue;

        QJsonObject record;
        record["id"]=recordIt.key();
        record["branch"]=records[recordIt.key()].value("branch");
        record["name"]=records[recordIt.key()].value("name");
        record["hash"]=recordIt.value();
        recordList.append(record);

        m_store.touchRecord(recordIt.key(), recordIt.value());
    }
    pullObject["records"]=recordList;

    m_store.save();

    return QJsonDocument(pullObject).toJson(QJsonDocument::Compact);
}


QByteArray SyncCore::record(const QString &recordId)
{
    if(m_model==nullptr)
        return QByteArray();

    TreeItem *branchItem=nullptr;
    int pos=-1;
    if(!findRecord(m_model, recordId, branchItem, pos))
        return QByteArray();

    RecordTableData *table=branchItem->recordtableGetTableData();

    QJsonObject recordObject;
    recordObject["id"]=recordId;
    recordObject["branch"]=branchItem->getField("id");

    QJsonObject fields;
    fields["name"]=table->getField("name", pos);
    fields["author"]=table->getField("author", pos);
    fields["url"]=table->getField("url", pos);
    fields["tags"]=table->getField("tags", pos);
    fields["ctime"]=table->getField("ctime", pos);
    fields["block"]=table->getField("block", pos);
    fields["crypt"]=table->getField("crypt", pos);
    recordObject["fields"]=fields;

    // Тело записи. Шифрованная запись едет base64 шифртекста,
    // ядро его не расшифровывает и расшифровать не может
    QString dirName=table->getField("dir", pos);
    QString textFileName=mytetraConfig.get_tetradir()+"/base/"+dirName+"/text.html";

    QFile textFile(textFileName);
    if(textFile.open(QIODevice::ReadOnly))
    {
        QByteArray textBytes=textFile.readAll();
        textFile.close();

        if(table->getField("crypt", pos)=="1")
        {
            recordObject["textEncoding"]="base64";
            recordObject["text"]=QString::fromLatin1(textBytes.toBase64());
        }
        else
        {
            recordObject["textEncoding"]="utf8";
            recordObject["text"]=QString::fromUtf8(textBytes);
        }
    }

    // Блобы: все файлы каталога кроме текста
    QJsonArray blobList;
    QDir recordDir(mytetraConfig.get_tetradir()+"/base/"+dirName);
    QStringList fileNames=recordDir.entryList(QDir::Files, QDir::Name);
    foreach(QString fileName, fileNames)
    {
        if(fileName=="text.html")
            continue;

        QJsonObject blob;
        blob["name"]=fileName;
        blob["size"]=static_cast<int>(QFileInfo(recordDir.absoluteFilePath(fileName)).size());
        blobList.append(blob);
    }
    recordObject["blobs"]=blobList;

    return QJsonDocument(recordObject).toJson(QJsonDocument::Compact);
}


QByteArray SyncCore::blob(const QString &recordId,
                          const QString &fileName,
                          int &responseCode)
{
    responseCode=404;

    // Санитизация имени: только имя файла без путей,
    // текст записи отдается только через record
    if(fileName.isEmpty() || fileName.contains("/") || fileName.contains("\\") || fileName=="text.html")
        return QByteArray();

    if(m_model==nullptr)
        return QByteArray();

    TreeItem *branchItem=nullptr;
    int pos=-1;
    if(!findRecord(m_model, recordId, branchItem, pos))
        return QByteArray();

    QString dirName=branchItem->recordtableGetTableData()->getField("dir", pos);
    QDir recordDir(mytetraConfig.get_tetradir()+"/base/"+dirName);

    // Файл обязан лежать строго внутри каталога записи
    QString canonicalDir=recordDir.canonicalPath();
    QString canonicalFile=QFileInfo(recordDir.absoluteFilePath(fileName)).canonicalFilePath();
    if(canonicalDir.isEmpty() || canonicalFile.isEmpty() || !canonicalFile.startsWith(canonicalDir+"/"))
        return QByteArray();

    QFile file(canonicalFile);
    if(!file.open(QIODevice::ReadOnly))
        return QByteArray();

    responseCode=200;
    return file.readAll();
}


// Запись тела записи напрямую в файл. Для шифрованной записи байты
// уже шифртекст от клиента, ядро его не трогает
static bool writeSyncTextFile(const QString &dirName,
                              bool isCrypt,
                              const QString &text,
                              const QString &textEncoding)
{
    QString fileName=mytetraConfig.get_tetradir()+"/base/"+dirName+"/text.html";

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly))
        return false;

    if(isCrypt || textEncoding=="base64")
        file.write(QByteArray::fromBase64(text.toLatin1()));
    else
        file.write(text.toUtf8());

    file.close();

    return true;
}


// Запись блобов в каталог записи. Имена санитизируются,
// пути и text.html отклоняются
static QStringList writeSyncBlobs(const QString &dirName,
                                  const QJsonObject &blobs)
{
    QStringList written;

    QDir recordDir(mytetraConfig.get_tetradir()+"/base/"+dirName);
    QString canonicalDir=recordDir.canonicalPath();
    if(canonicalDir.isEmpty())
        return written;

    QStringList names=blobs.keys();
    foreach(QString name, names)
    {
        if(name.isEmpty() || name.contains("/") || name.contains("\\") || name=="text.html")
            continue;

        QString canonicalFile=QFileInfo(recordDir.absoluteFilePath(name)).canonicalFilePath();
        QString targetFile=recordDir.absoluteFilePath(name);

        // Новый файл еще не существует, проверяется его будущий путь
        QString targetDir=QFileInfo(targetFile).absolutePath();
        if(QDir(targetDir).canonicalPath()!=canonicalDir && targetDir!=canonicalDir)
            continue;

        QFile file(targetFile);
        if(!file.open(QIODevice::WriteOnly))
            continue;

        file.write(QByteArray::fromBase64(blobs.value(name).toString().toLatin1()));
        file.close();

        written << name;
    }

    return written;
}


void SyncCore::applyPush(unsigned int baseRev,
                         const QJsonArray &branches,
                         const QJsonArray &records,
                         QJsonArray &appliedBranches,
                         QJsonArray &appliedRecords,
                         QJsonArray &conflicts)
{
    if(m_model==nullptr)
        return;

    bool hasCryptKey=globalParameters.getCryptKey().length()>0;

    // Сначала ветки, записи ссылаются на них
    foreach(QJsonValue branchValue, branches)
    {
        QJsonObject branchObject=branchValue.toObject();

        QString branchId=branchObject.value("id").toString();
        QString parentId=branchObject.value("parent").toString("0");
        QString branchName=branchObject.value("name").toString();
        QString baseHash=branchObject.value("baseHash").toString();

        if(branchId.isEmpty() || branchName.isEmpty())
            continue;

        TreeItem *item=m_model->getItemById(branchId);
        if(item==nullptr)
        {
            // Создание ветки
            TreeItem *parentItem=nullptr;
            if(!parentId.isEmpty() && parentId!="0")
                parentItem=m_model->getItemById(parentId);

            if(parentItem!=nullptr &&
               parentItem->getField("crypt")=="1" &&
               !hasCryptKey)
            {
                QJsonObject conflict;
                conflict["id"]=branchId;
                conflict["reason"]="crypt-locked";
                conflicts.append(conflict);
                continue;
            }

            QModelIndex parentIndex;
            if(parentItem!=nullptr)
                parentIndex=m_model->getIndexByItem(parentItem);

            QMap<QString, QString> branchFields;
            branchFields["id"]=branchId;
            branchFields["name"]=branchName;

            m_model->addNewChildBranch(parentIndex, branchFields);

            TreeItem *newItem=m_model->getItemById(branchId);
            if(newItem!=nullptr)
            {
                m_store.touchBranch(branchId, SyncStore::branchHash(newItem));
                appliedBranches.append(branchId);
            }
        }
        else
        {
            // Переименование. Ветки в прототипе last-writer-wins,
            // конфликт только фиксируется в ответе
            if(item->getField("name")!=branchName)
            {
                if(m_store.storedBranchRev(branchId)>baseRev &&
                   m_store.storedBranchHash(branchId)!=baseHash)
                {
                    QJsonObject conflict;
                    conflict["id"]=branchId;
                    conflict["reason"]="branch-overwritten";
                    conflicts.append(conflict);
                }

                item->setField("name", branchName);
                m_store.touchBranch(branchId, SyncStore::branchHash(item));
                appliedBranches.append(branchId);
            }
        }
    }

    // Затем записи
    foreach(QJsonValue recordValue, records)
    {
        QJsonObject recordObject=recordValue.toObject();

        QString recordId=recordObject.value("id").toString();
        QString branchId=recordObject.value("branch").toString();
        QString baseHash=recordObject.value("baseHash").toString();
        QJsonObject fields=recordObject.value("fields").toObject();
        QString text=recordObject.value("text").toString();
        QString textEncoding=recordObject.value("textEncoding").toString("utf8");
        QJsonObject blobs=recordObject.value("blobs").toObject();

        bool isCrypt=fields.value("crypt").toString()=="1";

        if(recordId.isEmpty() || branchId.isEmpty())
            continue;

        // Шифрованные сущности без введенного пароля не трогаем
        if(isCrypt && !hasCryptKey)
        {
            QJsonObject conflict;
            conflict["id"]=recordId;
            conflict["reason"]="crypt-locked";
            conflicts.append(conflict);
            continue;
        }

        TreeItem *branchItem=m_model->getItemById(branchId);
        if(branchItem==nullptr)
        {
            QJsonObject conflict;
            conflict["id"]=recordId;
            conflict["reason"]="no-branch";
            conflicts.append(conflict);
            continue;
        }

        if(branchItem->getField("crypt")=="1" && !hasCryptKey)
        {
            QJsonObject conflict;
            conflict["id"]=recordId;
            conflict["reason"]="crypt-locked";
            conflicts.append(conflict);
            continue;
        }

        RecordTableData *table=branchItem->recordtableGetTableData();
        if(table==nullptr)
            continue;

        int pos=table->getPosById(recordId);
        if(pos<0)
        {
            // Создание записи с идентификатором клиента,
            // чтобы id не разъехались между сторонами.
            // Раз id задан явно, каталог и файл задаются тоже,
            // иначе вставка считает запись вырезанной из буфера
            // и падает на отсутствующем поле dir
            Record record;
            record.switchToFat();
            record.setField("id", recordId);
            record.setField("dir", getUniqueId());
            record.setField("file", "text.html");
            record.setField("name", fields.value("name").toString());
            record.setField("author", fields.value("author").toString());
            record.setField("url", fields.value("url").toString());
            record.setField("tags", fields.value("tags").toString());
            record.setField("crypt", isCrypt ? "1" : "0");

            QString ctime=fields.value("ctime").toString();
            if(ctime.isEmpty())
                ctime=QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
            record.setField("ctime", ctime);

            if(!isCrypt)
                record.setText(text);

            int insertPos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                                0,
                                                record);
            if(insertPos<0)
                continue;

            QString dirName=table->getField("dir", insertPos);

            if(isCrypt)
                writeSyncTextFile(dirName, true, text, textEncoding);

            writeSyncBlobs(dirName, blobs);

            QMap<QString, QString> currentFields;
            currentFields["id"]=recordId;
            currentFields["name"]=table->getField("name", insertPos);
            currentFields["author"]=table->getField("author", insertPos);
            currentFields["url"]=table->getField("url", insertPos);
            currentFields["tags"]=table->getField("tags", insertPos);
            currentFields["ctime"]=table->getField("ctime", insertPos);
            currentFields["block"]=table->getField("block", insertPos);
            currentFields["crypt"]=table->getField("crypt", insertPos);

            m_store.touchRecord(recordId, SyncStore::recordHash(currentFields, dirName));
            appliedRecords.append(recordId);
        }
        else
        {
            // Конфликт: сервер менялся после baseRev клиента,
            // а клиент правил не серверную версию
            if(m_store.storedRecordRev(recordId)>baseRev &&
               m_store.storedRecordHash(recordId)!=baseHash)
            {
                // Обе копии сохраняются, входящая как новая запись с тегом
                Record copyRecord;
                copyRecord.switchToFat();
                copyRecord.setField("id", getUniqueId());
                copyRecord.setField("dir", getUniqueId());
                copyRecord.setField("file", "text.html");
                copyRecord.setField("name", table->getField("name", pos)+" (conflict)");

                QString copyTags=table->getField("tags", pos);
                if(!copyTags.isEmpty())
                    copyTags+=",";
                copyTags+="conflict";
                copyRecord.setField("tags", copyTags);

                copyRecord.setField("author", fields.value("author").toString());
                copyRecord.setField("url", fields.value("url").toString());
                copyRecord.setField("crypt", isCrypt ? "1" : "0");
                copyRecord.setField("ctime", QDateTime::currentDateTime().toString("yyyyMMddhhmmss"));

                if(!isCrypt)
                    copyRecord.setText(text);

                int copyPos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                                   0,
                                                   copyRecord);
                if(copyPos>=0)
                {
                    QString copyId=table->getField("id", copyPos);
                    QString copyDir=table->getField("dir", copyPos);

                    if(isCrypt)
                        writeSyncTextFile(copyDir, true, text, textEncoding);

                    writeSyncBlobs(copyDir, blobs);

                    QMap<QString, QString> copyFields;
                    copyFields["id"]=copyId;
                    copyFields["name"]=table->getField("name", copyPos);
                    copyFields["author"]=table->getField("author", copyPos);
                    copyFields["url"]=table->getField("url", copyPos);
                    copyFields["tags"]=table->getField("tags", copyPos);
                    copyFields["ctime"]=table->getField("ctime", copyPos);
                    copyFields["block"]=table->getField("block", copyPos);
                    copyFields["crypt"]=table->getField("crypt", copyPos);

                    m_store.touchRecord(copyId, SyncStore::recordHash(copyFields, copyDir));

                    QJsonObject conflict;
                    conflict["id"]=recordId;
                    conflict["reason"]="conflict";
                    conflict["copyId"]=copyId;
                    conflicts.append(conflict);

                    appliedRecords.append(copyId);
                }
                continue;
            }

            // Тихое обновление
            table->setField("name", fields.value("name").toString(), pos);
            table->setField("author", fields.value("author").toString(), pos);
            table->setField("url", fields.value("url").toString(), pos);
            table->setField("tags", fields.value("tags").toString(), pos);

            QString dirName=table->getField("dir", pos);
            writeSyncTextFile(dirName, isCrypt, text, textEncoding);
            writeSyncBlobs(dirName, blobs);

            QMap<QString, QString> currentFields;
            currentFields["id"]=recordId;
            currentFields["name"]=table->getField("name", pos);
            currentFields["author"]=table->getField("author", pos);
            currentFields["url"]=table->getField("url", pos);
            currentFields["tags"]=table->getField("tags", pos);
            currentFields["ctime"]=table->getField("ctime", pos);
            currentFields["block"]=table->getField("block", pos);
            currentFields["crypt"]=table->getField("crypt", pos);

            m_store.touchRecord(recordId, SyncStore::recordHash(currentFields, dirName));
            appliedRecords.append(recordId);
        }
    }

    // Сохранение дерева на диск. Вид не дергается, фоновая задача
    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen!=nullptr)
        treeScreen->saveKnowTree();

    m_store.save();
}


QByteArray SyncCore::push(const QByteArray &body)
{
    if(m_model==nullptr)
        return "{\"error\":\"no model\"}";

    QJsonParseError parseError;
    QJsonDocument document=QJsonDocument::fromJson(body, &parseError);
    if(parseError.error!=QJsonParseError::NoError || !document.isObject())
        return "{\"error\":\"bad push\"}";

    QJsonObject pushObject=document.object();

    unsigned int baseRev=static_cast<unsigned int>(pushObject.value("baseRev").toInt(0));

    // Sidecar освежается перед conflict-check, чтобы локальные
    // правки десктопа участвовали в детекте конфликтов
    QMap<QString, QString> changedBranches;
    QMap<QString, QString> changedRecords;
    m_store.scan(m_model, changedBranches, changedRecords);

    QMapIterator<QString, QString> branchIt(changedBranches);
    while(branchIt.hasNext())
    {
        branchIt.next();
        m_store.touchBranch(branchIt.key(), branchIt.value());
    }

    QMapIterator<QString, QString> recordIt(changedRecords);
    while(recordIt.hasNext())
    {
        recordIt.next();
        m_store.touchRecord(recordIt.key(), recordIt.value());
    }

    QJsonArray appliedBranches;
    QJsonArray appliedRecords;
    QJsonArray conflicts;

    applyPush(baseRev,
              pushObject.value("branches").toArray(),
              pushObject.value("records").toArray(),
              appliedBranches,
              appliedRecords,
              conflicts);

    QJsonObject response;
    response["rev"]=static_cast<int>(m_store.rev());

    QJsonObject applied;
    applied["branches"]=appliedBranches;
    applied["records"]=appliedRecords;
    response["applied"]=applied;

    response["conflicts"]=conflicts;

    return QJsonDocument(response).toJson(QJsonDocument::Compact);
}

#include "SyncStore.h"

#include <QSettings>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>

#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"

extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


SyncStore::SyncStore(void)
{
    m_rev=0;
}


SyncStore::~SyncStore(void)
{
}


QString SyncStore::stateFileName(void)
{
    return globalParameters.getWorkDirectory()+"/syncstate.ini";
}


void SyncStore::load(void)
{
    m_rev=0;
    m_branchHash.clear();
    m_branchRev.clear();
    m_recordHash.clear();
    m_recordRev.clear();

    QFile stateFile(stateFileName());
    if(!stateFile.exists())
        return; // Первый запуск, состояние пустое

    QSettings state(stateFileName(), QSettings::IniFormat);

    m_rev=state.value("Rev/rev", 0).toUInt();

    state.beginGroup("BranchHash");
    QStringList branchIds=state.childKeys();
    foreach(QString id, branchIds)
        m_branchHash[id]=state.value(id).toString();
    state.endGroup();

    state.beginGroup("BranchRev");
    branchIds=state.childKeys();
    foreach(QString id, branchIds)
        m_branchRev[id]=state.value(id).toUInt();
    state.endGroup();

    state.beginGroup("RecordHash");
    QStringList recordIds=state.childKeys();
    foreach(QString id, recordIds)
        m_recordHash[id]=state.value(id).toString();
    state.endGroup();

    state.beginGroup("RecordRev");
    recordIds=state.childKeys();
    foreach(QString id, recordIds)
        m_recordRev[id]=state.value(id).toUInt();
    state.endGroup();
}


void SyncStore::save(void) const
{
    QSettings state(stateFileName(), QSettings::IniFormat);

    state.setValue("Rev/rev", m_rev);

    state.beginGroup("BranchHash");
    state.remove("");
    QMapIterator<QString, QString> branchHashIt(m_branchHash);
    while(branchHashIt.hasNext())
    {
        branchHashIt.next();
        state.setValue(branchHashIt.key(), branchHashIt.value());
    }
    state.endGroup();

    state.beginGroup("BranchRev");
    state.remove("");
    QMapIterator<QString, unsigned int> branchRevIt(m_branchRev);
    while(branchRevIt.hasNext())
    {
        branchRevIt.next();
        state.setValue(branchRevIt.key(), branchRevIt.value());
    }
    state.endGroup();

    state.beginGroup("RecordHash");
    state.remove("");
    QMapIterator<QString, QString> recordHashIt(m_recordHash);
    while(recordHashIt.hasNext())
    {
        recordHashIt.next();
        state.setValue(recordHashIt.key(), recordHashIt.value());
    }
    state.endGroup();

    state.beginGroup("RecordRev");
    state.remove("");
    QMapIterator<QString, unsigned int> recordRevIt(m_recordRev);
    while(recordRevIt.hasNext())
    {
        recordRevIt.next();
        state.setValue(recordRevIt.key(), recordRevIt.value());
    }
    state.endGroup();

    state.sync();
}


unsigned int SyncStore::rev(void) const
{
    return m_rev;
}


QString SyncStore::branchHash(TreeItem *item)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    hash.addData(item->getField("id").toUtf8());
    hash.addData(item->getField("name").toUtf8());
    hash.addData(item->getField("ctime").toUtf8());
    hash.addData(item->getField("crypt").toUtf8());

    return QString(hash.result().toHex());
}


QString SyncStore::recordHash(const QMap<QString, QString> &fields,
                              const QString &dirName)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    // Поля таблицы конечных записей
    hash.addData(fields.value("id").toUtf8());
    hash.addData(fields.value("name").toUtf8());
    hash.addData(fields.value("author").toUtf8());
    hash.addData(fields.value("url").toUtf8());
    hash.addData(fields.value("tags").toUtf8());
    hash.addData(fields.value("ctime").toUtf8());
    hash.addData(fields.value("block").toUtf8());
    hash.addData(fields.value("crypt").toUtf8());

    // Байты файлов записи: текст, картинки, аттачи.
    // Шифрованные записи хешируются шифртекстом, серверу
    // открытый текст не нужен и недоступен
    QDir recordDir(mytetraConfig.get_tetradir()+"/base/"+dirName);
    QStringList fileNames=recordDir.entryList(QDir::Files, QDir::Name);
    foreach(QString fileName, fileNames)
    {
        hash.addData(fileName.toUtf8());

        QFile file(recordDir.absoluteFilePath(fileName));
        if(file.open(QIODevice::ReadOnly))
        {
            while(!file.atEnd())
                hash.addData(file.read(65536));
            file.close();
        }
    }

    return QString(hash.result().toHex());
}


void SyncStore::touchBranch(const QString &id, const QString &hash)
{
    m_rev++;
    m_branchHash[id]=hash;
    m_branchRev[id]=m_rev;
}


void SyncStore::touchRecord(const QString &id, const QString &hash)
{
    m_rev++;
    m_recordHash[id]=hash;
    m_recordRev[id]=m_rev;
}


QString SyncStore::storedBranchHash(const QString &id) const
{
    return m_branchHash.value(id, QString());
}


QString SyncStore::storedRecordHash(const QString &id) const
{
    return m_recordHash.value(id, QString());
}


unsigned int SyncStore::storedBranchRev(const QString &id) const
{
    return m_branchRev.value(id, 0);
}


unsigned int SyncStore::storedRecordRev(const QString &id) const
{
    return m_recordRev.value(id, 0);
}


void SyncStore::forgetBranch(const QString &id)
{
    m_branchHash.remove(id);
    m_branchRev.remove(id);
}


void SyncStore::forgetRecord(const QString &id)
{
    m_recordHash.remove(id);
    m_recordRev.remove(id);
}


void SyncStore::collectBranchHashesRecurse(TreeItem *item,
                                           QMap<QString, QString> &hashes) const
{
    if(item==nullptr)
        return;

    hashes[item->getField("id")]=branchHash(item);

    for(int i=0; i<item->childCount(); ++i)
        collectBranchHashesRecurse(item->child(i), hashes);
}


void SyncStore::collectBranchHashes(KnowTreeModel *model,
                                    QMap<QString, QString> &hashes) const
{
    TreeItem *root=model->getItem(QModelIndex());
    if(root==nullptr)
        return;

    // Корень не синхронизируется, только его подветки
    for(int i=0; i<root->childCount(); ++i)
        collectBranchHashesRecurse(root->child(i), hashes);
}


void SyncStore::scan(KnowTreeModel *model,
                     QMap<QString, QString> &changedBranches,
                     QMap<QString, QString> &changedRecords)
{
    changedBranches.clear();
    changedRecords.clear();

    // Ветки
    QMap<QString, QString> currentBranches;
    collectBranchHashes(model, currentBranches);

    QMapIterator<QString, QString> branchIt(currentBranches);
    while(branchIt.hasNext())
    {
        branchIt.next();
        if(m_branchHash.value(branchIt.key(), QString())!=branchIt.value())
            changedBranches[branchIt.key()]=branchIt.value();
    }

    // Забываются ветки, которых больше нет в дереве
    QStringList knownBranchIds=m_branchHash.keys();
    foreach(QString id, knownBranchIds)
    {
        if(!currentBranches.contains(id))
            forgetBranch(id);
    }

    // Записи. Обход идет по тем же веткам, таблица берется у ветки
    QMap<QString, QString> currentRecords;
    TreeItem *root=model->getItem(QModelIndex());
    if(root!=nullptr)
    {
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
                for(unsigned int pos=0; pos<table->size(); ++pos)
                {
                    QString recordId=table->getField("id", pos);

                    QMap<QString, QString> fields;
                    fields["id"]=recordId;
                    fields["name"]=table->getField("name", pos);
                    fields["author"]=table->getField("author", pos);
                    fields["url"]=table->getField("url", pos);
                    fields["tags"]=table->getField("tags", pos);
                    fields["ctime"]=table->getField("ctime", pos);
                    fields["block"]=table->getField("block", pos);
                    fields["crypt"]=table->getField("crypt", pos);

                    currentRecords[recordId]=recordHash(fields, table->getField("dir", pos));
                }
            }

            for(int i=0; i<item->childCount(); ++i)
                stack.append(item->child(i));
        }
    }

    QMapIterator<QString, QString> recordIt(currentRecords);
    while(recordIt.hasNext())
    {
        recordIt.next();
        if(m_recordHash.value(recordIt.key(), QString())!=recordIt.value())
            changedRecords[recordIt.key()]=recordIt.value();
    }

    // Забываются записи, которых больше нет в дереве
    QStringList knownRecordIds=m_recordHash.keys();
    foreach(QString id, knownRecordIds)
    {
        if(!currentRecords.contains(id))
            forgetRecord(id);
    }
}

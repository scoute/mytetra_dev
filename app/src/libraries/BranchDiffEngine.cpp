#include "BranchDiffEngine.h"

#include <QCryptographicHash>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>

namespace
{

// Упорядоченный список идентификаторов в порядке возрастания id
QStringList sortedKeys(const QHash<QString, DiffRecordData> &records)
{
    QStringList keys=records.keys();
    keys.sort();
    return keys;
}

QStringList sortedBranchKeys(const QHash<QString, DiffBranchData> &branches)
{
    QStringList keys=branches.keys();
    keys.sort();
    return keys;
}

// Внутреннее имя файла вложения — репликация Attach::constructFileName
// (двигатель не зависит от модели вложений)
QString innerAttachName(const QDomElement &fileElement)
{
    const QString type=fileElement.attribute("type");
    const QString id=fileElement.attribute("id");
    const QString fileName=fileElement.attribute("fileName");

    if(type==QStringLiteral("file"))
    {
        if(fileName.startsWith('.'))
            return "."+id;
        if(fileName.endsWith('.'))
            return id+".";
        QFileInfo fileInfo(fileName);
        QString suffix=fileInfo.suffix();
        if(!suffix.isEmpty())
            return id+"."+suffix;
        return id;
    }

    if(type==QStringLiteral("link"))
        return fileName;

    return QString();
}

} // namespace


QString DiffChange::typeName(void) const
{
    switch(type)
    {
        case RecordAdd:      return QStringLiteral("record.add");
        case RecordUpdate:   return QStringLiteral("record.update");
        case RecordDelete:   return QStringLiteral("record.delete");
        case BranchRename:   return QStringLiteral("branch.props");
        case BranchMove:     return QStringLiteral("branch.move");
        case BranchAdd:      return QStringLiteral("branch.add");
        case BranchDelete:   return QStringLiteral("branch.delete");
    }
    return QString();
}


QByteArray BranchDiffEngine::sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}


FragmentData BranchDiffEngine::parseFragment(const QString &branchXmlContent)
{
    FragmentData fragment;

    QDomDocument branchDoc;
    if(!branchDoc.setContent(branchXmlContent))
        return fragment;

    // Корневой узел ветки лежит сразу под <branch>
    QDomElement rootNode=branchDoc.documentElement().firstChildElement("node");
    if(rootNode.isNull())
        return fragment;

    fragment.rootId=rootNode.attribute("id");

    // Рекурсивный обход: ветка -> <recordtable> записей + вложенные <node>
    std::function<void(const QDomElement &, const QString &)> walk;
    walk=[&fragment, &walk](const QDomElement &nodeElement, const QString &parentId)
    {
        DiffBranchData branch;
        branch.id=nodeElement.attribute("id");
        branch.name=nodeElement.attribute("name");
        branch.parentId=parentId;

        // Записи текущей ветки
        QDomElement recordTable=nodeElement.firstChildElement("recordtable");
        QDomElement record=recordTable.firstChildElement("record");
        while(!record.isNull())
        {
            DiffRecordData recordData;
            recordData.id=record.attribute("id");
            recordData.name=record.attribute("name");
            recordData.author=record.attribute("author");
            recordData.url=record.attribute("url");
            recordData.tags=record.attribute("tags");
            recordData.dir=record.attribute("dir");
            recordData.file=record.attribute("file");

            // Вложения записи (<files>)
            QDomElement files=record.firstChildElement("files");
            QDomElement file=files.firstChildElement("file");
            while(!file.isNull())
            {
                QString innerName=innerAttachName(file);
                if(!innerName.isEmpty())
                    recordData.attachNames.append(innerName);
                file=file.nextSiblingElement("file");
            }

            fragment.records.insert(recordData.id, recordData);
            branch.recordIds.append(recordData.id);

            record=record.nextSiblingElement("record");
        }

        // Вложенные ветки
        QDomElement childNode=nodeElement.firstChildElement("node");
        while(!childNode.isNull())
        {
            branch.childIds.append(childNode.attribute("id"));
            childNode=childNode.nextSiblingElement("node");
        }

        fragment.branches.insert(branch.id, branch);

        // Рекурсивно вниз
        childNode=nodeElement.firstChildElement("node");
        while(!childNode.isNull())
        {
            walk(childNode, branch.id);
            childNode=childNode.nextSiblingElement("node");
        }
    };

    walk(rootNode, QString());
    return fragment;
}


void BranchDiffEngine::fillTextShas(FragmentData &fragment,
                                    const std::function<QByteArray(const DiffRecordData &)> &contentProvider)
{
    for(DiffRecordData &record : fragment.records)
    {
        QByteArray content=contentProvider(record);
        if(content.isEmpty())
        {
            record.textSha.clear();
        }
        else
        {
            record.textSha=BranchDiffEngine::sha256(content);
        }
    }
}


QList<DiffChange> BranchDiffEngine::buildChangeList(const FragmentData &baseline,
                                                     const FragmentData &head)
{
    QList<DiffChange> changes;

    // Родительская ветка записи во фрагменте (для привязки изменения
    // к локальной ветке-копии при импорте: записи живут не только в корне)
    auto parentBranchOf=[&](const FragmentData &fragment, const QString &recordId) -> QString
    {
        for(auto it=fragment.branches.constBegin(); it!=fragment.branches.constEnd(); ++it)
        {
            if(it.value().recordIds.contains(recordId))
                return it.key();
        }
        return QString();
    };

    // --- Изменения веток -------------------------------------------------
    QStringList allBranchIds=sortedBranchKeys(baseline.branches);
    for(const QString &id : sortedBranchKeys(head.branches))
    {
        if(!allBranchIds.contains(id))
            allBranchIds.append(id);
    }

    for(const QString &branchId : allBranchIds)
    {
        const bool inBaseline=baseline.branches.contains(branchId);
        const bool inHead=head.branches.contains(branchId);

        if(!inBaseline && inHead)
        {
            DiffChange change;
            change.type=DiffChange::BranchAdd;
            change.branchId=branchId;
            change.title=head.branches.value(branchId).name;
            if(change.title.isEmpty())
                change.title=branchId;
            changes.append(change);
        }
        else if(inBaseline && !inHead)
        {
            DiffChange change;
            change.type=DiffChange::BranchDelete;
            change.branchId=branchId;
            change.title=baseline.branches.value(branchId).name;
            if(change.title.isEmpty())
                change.title=branchId;
            changes.append(change);
        }
        else if(inBaseline && inHead)
        {
            const DiffBranchData &baseBranch=baseline.branches.value(branchId);
            const DiffBranchData &headBranch=head.branches.value(branchId);

            // Перемещение ветки
            if(baseBranch.parentId!=headBranch.parentId)
            {
                DiffChange change;
                change.type=DiffChange::BranchMove;
                change.branchId=branchId;
                change.title=headBranch.name;
                if(change.title.isEmpty())
                    change.title=branchId;
                change.oldParentId=baseBranch.parentId;
                change.newParentId=headBranch.parentId;
                changes.append(change);
            }

            // Переименование ветки (branch.props)
            if(baseBranch.name!=headBranch.name)
            {
                DiffChange change;
                change.type=DiffChange::BranchRename;
                change.branchId=branchId;
                change.title=headBranch.name;
                if(change.title.isEmpty())
                    change.title=branchId;
                change.fields.append(QPair<QString, QPair<QString, QString>>(
                    QStringLiteral("name"),
                    QPair<QString, QString>(baseBranch.name, headBranch.name)));
                changes.append(change);
            }
        }
    }

    // --- Изменения записей: добавленные и обновлённые --------------------
    for(const QString &recordId : sortedKeys(head.records))
    {
        const DiffRecordData &headRecord=head.records.value(recordId);

        if(!baseline.records.contains(recordId))
        {
            DiffChange change;
            change.type=DiffChange::RecordAdd;
            change.recordId=recordId;
            change.branchId=parentBranchOf(head, recordId);
            change.title=headRecord.name;
            if(change.title.isEmpty())
                change.title=recordId;
            changes.append(change);
        }
        else
        {
            const DiffRecordData &baseRecord=baseline.records.value(recordId);

            bool anyChange=false;
            DiffChange change;
            change.type=DiffChange::RecordUpdate;
            change.recordId=recordId;
            change.branchId=parentBranchOf(head, recordId);
            change.title=headRecord.name;
            if(change.title.isEmpty())
                change.title=recordId;

            // Натуральные поля (не id/dir/file/ctime)
            struct FieldPair { QString name; QString oldValue; QString newValue; };
            QList<FieldPair> fieldPairs={
                {QStringLiteral("name"),   baseRecord.name,   headRecord.name},
                {QStringLiteral("author"), baseRecord.author, headRecord.author},
                {QStringLiteral("url"),    baseRecord.url,    headRecord.url},
                {QStringLiteral("tags"),   baseRecord.tags,   headRecord.tags}
            };

            for(const FieldPair &field : fieldPairs)
            {
                if(field.oldValue!=field.newValue)
                {
                    change.fields.append(QPair<QString, QPair<QString, QString>>(
                        field.name,
                        QPair<QString, QString>(field.oldValue, field.newValue)));
                    anyChange=true;
                }
            }

            // Содержимое text.html (по sha256)
            if(!baseRecord.textSha.isEmpty() && !headRecord.textSha.isEmpty()
               && baseRecord.textSha!=headRecord.textSha)
            {
                change.textChanged=true;
                anyChange=true;
            }

            // Вложения: список внутренних имён файлов
            QStringList baseAttaches=baseRecord.attachNames;
            QStringList headAttaches=headRecord.attachNames;
            baseAttaches.sort();
            headAttaches.sort();
            for(const QString &name : baseAttaches)
                if(!headAttaches.contains(name))
                    change.attachRemoved.append(name);
            for(const QString &name : headAttaches)
                if(!baseAttaches.contains(name))
                    change.attachAdded.append(name);
            if(!change.attachAdded.isEmpty() || !change.attachRemoved.isEmpty())
                anyChange=true;

            if(anyChange)
                changes.append(change);
        }
    }

    // --- Изменения записей: удалённые ------------------------------------
    for(const QString &recordId : sortedKeys(baseline.records))
    {
        if(head.records.contains(recordId))
            continue;

        const DiffRecordData &baseRecord=baseline.records.value(recordId);

        DiffChange change;
        change.type=DiffChange::RecordDelete;
        change.recordId=recordId;
        change.branchId=parentBranchOf(baseline, recordId);
        change.title=baseRecord.name;
        if(change.title.isEmpty())
            change.title=recordId;
        changes.append(change);
    }

    return changes;
}
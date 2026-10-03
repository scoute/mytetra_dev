#include "SubscriptionImportEngine.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>

#include "libraries/BranchDiffEngine.h"
#include "libraries/GitWrapper.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/DiskHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "models/appConfig/AppConfig.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "models/subscription/SubscriptionRegistry.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"

#include <QSet>

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


SubscriptionImportEngine::SubscriptionImportEngine(QObject *parent)
    : QObject(parent)
{
}


void SubscriptionImportEngine::setPublicationDir(const QString &publicationDir)
{
    m_publicationDir=publicationDir;
    m_loaded=false;
}


QString SubscriptionImportEngine::publicationDir(void) const
{
    return m_publicationDir;
}


PublicationMeta SubscriptionImportEngine::getMeta(void) const
{
    return m_meta;
}


QString SubscriptionImportEngine::headCommit(void) const
{
    const QString repoRoot=repositoryRoot();
    if(repoRoot.isEmpty())
        return QString();

    return GitWrapper::getHead(repoRoot);
}


// Манифест-снимок текущего состояния публикации (branch.xml + sha256 текстов)
QString SubscriptionImportEngine::buildBaselineSnapshot(void) const
{
    QJsonObject root;
    root["schemaVersion"]=1;
    root["publishVersion"]=m_meta.publishVersion;

    // Сериализованный branch.xml среза
    root["branchXml"]=m_branchXmlContent;

    // Карта recordId -> sha256 содержимого записей (посчитаны при loadPublication)
    QJsonObject shas;
    for(auto it=m_headFragment.records.constBegin(); it!=m_headFragment.records.constEnd(); ++it)
        shas.insert(it.key(), QString::fromLatin1(it.value().textSha.toHex()));
    root["recordShas"]=shas;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}


// Репозиторий журнала для текущей публикации
QString SubscriptionImportEngine::repositoryRoot(void) const
{
    // Устаревший макет: per-branch репозиторий внутри публикации
    if(QFileInfo::exists(m_publicationDir+"/.git"))
        return m_publicationDir;

    // Новый макет: единый журнал в корне каталога обмена (shared/.git)
    return GitWrapper::findRepositoryRoot(m_publicationDir);
}


QList<DiffChange> SubscriptionImportEngine::changesSince(const QString &baselineCommit)
{
    FragmentData baselineFragment;

    // Репозиторий журнала и префикс пути публикации внутри него:
    // устаревший макет — файлы в репозитории без префикса,
    // новый единый журнал — под путём sync/<owner>/<branch>/
    const QString repoRoot=repositoryRoot();
    QString pathPrefix;
    if(!repoRoot.isEmpty())
    {
        if(repoRoot!=m_publicationDir)
            pathPrefix=QDir(repoRoot).relativeFilePath(m_publicationDir)+"/";
    }

    // Фрагмент базовой точки строится из git-истории публикации
    if(!baselineCommit.isEmpty() && !repoRoot.isEmpty())
    {
        const QString baselineXml=GitWrapper::getShow(repoRoot, baselineCommit, pathPrefix+"branch.xml");
        if(!baselineXml.isEmpty())
        {
            baselineFragment=BranchDiffEngine::parseFragment(baselineXml);

            BranchDiffEngine::fillTextShas(baselineFragment, [this, &repoRoot, &pathPrefix, &baselineCommit](const DiffRecordData &recordData)
            {
                const QString pathInRepo=pathPrefix+"records/"+recordData.dir+"/"
                        +(recordData.file.isEmpty()? QStringLiteral("text.html") : recordData.file);
                return GitWrapper::getShow(repoRoot, baselineCommit, pathInRepo).toUtf8();
            });
        }
    }

    return BranchDiffEngine::buildChangeList(baselineFragment, m_headFragment);
}


// Изменения между манифест-снимком подписчика и текущим HEAD публикации.
// git не используется: baseline строится из сохранённого снимка,
// содержимое базовых записей берётся из карты sha256 в снимке.
QList<DiffChange> SubscriptionImportEngine::changesSinceSnapshot(const QString &baselineSnapshotJson)
{
    FragmentData baselineFragment;

    if(!baselineSnapshotJson.isEmpty())
    {
        const QJsonDocument doc=QJsonDocument::fromJson(baselineSnapshotJson.toUtf8());
        const QJsonObject root=doc.object();

        const QString baselineXml=root.value("branchXml").toString();
        if(!baselineXml.isEmpty())
        {
            baselineFragment=BranchDiffEngine::parseFragment(baselineXml);

            // В снимке лежат готовые sha256 (hex), а не контент записей,
            // поэтому хеши подставляются напрямую: прогон через fillTextShas
            // хешировал бы хеш повторно и давал фантомные RecordText-изменения
            const QJsonObject shas=root.value("recordShas").toObject();
            for(auto it=baselineFragment.records.begin(); it!=baselineFragment.records.end(); ++it)
            {
                const QByteArray hex=shas.value(it.key()).toString().toLatin1();
                it.value().textSha=hex.isEmpty() ? QByteArray() : QByteArray::fromHex(hex);
            }
        }
    }

    return BranchDiffEngine::buildChangeList(baselineFragment, m_headFragment);
}


bool SubscriptionImportEngine::loadPublication(QString *errorText)
{
    m_loaded=false;
    m_branchDoc.clear();
    m_headFragment=FragmentData();

    m_meta=BranchPublisher::readPublication(m_publicationDir);

    QFile branchXmlFile(m_publicationDir+"/branch.xml");
    if(!branchXmlFile.open(QIODevice::ReadOnly))
    {
        if(errorText)
            *errorText=tr("Не удалось открыть %1/branch.xml").arg(m_publicationDir);
        return false;
    }
    m_branchXmlContent=QString::fromUtf8(branchXmlFile.readAll());
    branchXmlFile.close();

    if(!m_branchDoc.setContent(m_branchXmlContent))
    {
        if(errorText)
            *errorText=tr("Файл публикации branch.xml повреждён");
        return false;
    }

    m_headFragment=BranchDiffEngine::parseFragment(m_branchXmlContent);

    // Готовность публикации: манифест версии наличествует и совпадает по
    // заголовку, либо устаревший per-branch макет (целостность за .git).
    // Полная сверка записей выполняется ниже, в том же проходе чтения
    m_manifestFileShas.clear();
    int manifestVersion=0;
    BranchPublisher::readManifest(m_publicationDir, m_manifestFileShas, &manifestVersion);
    const bool manifestOk=(!m_manifestFileShas.isEmpty() && manifestVersion==m_meta.publishVersion);
    m_pubComplete=manifestOk || QFileInfo::exists(m_publicationDir+"/.git");

    // Хеши текста записей актуальной версии читаются из каталога публикации.
    // Заодно (без второго прохода) сверяются с манифестом: любое расхождение
    // или отсутствие файла помечает публикацию неготовой («данные в пути»)
    BranchDiffEngine::fillTextShas(m_headFragment, [this, manifestOk](const DiffRecordData &recordData)
    {
        QFile recordFile(this->sharedRecordDir(recordData)+"/"
                         + (recordData.file.isEmpty()? QStringLiteral("text.html") : recordData.file));
        if(!recordFile.open(QIODevice::ReadOnly))
        {
            if(manifestOk)
                this->m_pubComplete=false;
            return QByteArray();
        }
        QByteArray content=recordFile.readAll();
        recordFile.close();

        if(manifestOk)
        {
            const QString relative=QStringLiteral("records/")+recordData.dir+"/"
                    +(recordData.file.isEmpty()? QStringLiteral("text.html") : recordData.file);
            const auto it=this->m_manifestFileShas.constFind(relative);
            if(it==this->m_manifestFileShas.constEnd()
               || QString::fromLatin1(QCryptographicHash::hash(content,
                                                               QCryptographicHash::Sha256).toHex())!=it.value())
                this->m_pubComplete=false;
        }

        return content;
    });

    m_loaded=true;

    if(errorText)
        errorText->clear();

    return true;
}


// Готовность публикации к диффу/импорту
bool SubscriptionImportEngine::isPublicationComplete(void) const
{
    return m_loaded && m_pubComplete;
}


// Готовность отдельной записи: её файлы совпадают с манифестом версии
bool SubscriptionImportEngine::isRecordComplete(const QString &sharedRecordId) const
{
    const auto it=m_headFragment.records.constFind(sharedRecordId);
    if(it==m_headFragment.records.constEnd())
        return false;

    const DiffRecordData &record=it.value();
    const QString relative=QStringLiteral("records/")+record.dir+"/"
            +(record.file.isEmpty()? QStringLiteral("text.html") : record.file);

    const auto manifestIt=m_manifestFileShas.constFind(relative);
    if(manifestIt==m_manifestFileShas.constEnd())
        return false;

    QFile recordFile(this->sharedRecordDir(record)+"/"
                     + (record.file.isEmpty()? QStringLiteral("text.html") : record.file));
    if(!recordFile.open(QIODevice::ReadOnly))
        return false;

    const QByteArray content=recordFile.readAll();
    recordFile.close();

    return QString::fromLatin1(QCryptographicHash::hash(content,
                                                        QCryptographicHash::Sha256).toHex())
           ==manifestIt.value();
}


QString SubscriptionImportEngine::localTargetBranchId(void) const
{
    return m_localTargetBranchId;
}


bool SubscriptionImportEngine::importFullSubtree(SubscriptionRegistry *registry,
                                                 KnowTreeModel *model,
                                                 QString *errorText)
{
    if(!m_loaded)
    {
        if(errorText)
            *errorText=tr("Публикация не загружена");
        return false;
    }

    if(!model)
    {
        if(errorText)
            *errorText=tr("Модель дерева не передана");
        return false;
    }

    if(!registry)
    {
        if(errorText)
            *errorText=tr("Реестр подписок не передан");
        return false;
    }

    // Копия уже существует (импорт идемпотентен): ветки привязаны к подписке
    const QString existingRoot=registry->getSubscription(m_meta.branchId)
                               .branchesMap.value(m_meta.branchId);
    if(!existingRoot.isEmpty())
    {
        m_localTargetBranchId=existingRoot;
        if(errorText)
            errorText->clear();
        return true;
    }

    // Контейнер "imported" верхнего уровня
    const QString importedRootId=ensureImportedContainer(model, errorText);
    if(importedRootId.isEmpty())
        return false;

    // Папка владельца внутри контейнера
    QString ownerName=m_meta.ownerName;
    if(ownerName.isEmpty())
        ownerName=m_meta.ownerId;
    if(ownerName.isEmpty())
        ownerName=tr("unknown");
    const QString ownerFolderId=ensureOwnerFolder(model, importedRootId, ownerName, errorText);
    if(ownerFolderId.isEmpty())
        return false;

    // Корневая копия публикации (имя — свежий заголовок из meta.json)
    QString rootBranchName=m_meta.title;
    if(rootBranchName.isEmpty())
        rootBranchName=tr("branch");
    if(!importBranchRecursive(registry, model, ownerFolderId, m_meta.branchId,
                              rootBranchName, errorText))
        return false;

    if(errorText)
        errorText->clear();

    return true;
}


QString SubscriptionImportEngine::ensureImportedContainer(KnowTreeModel *model,
                                                          QString *errorText)
{
    const QString containerName=QStringLiteral("imported");
    const TreeItem *root=model->getRootItem();
    if(root)
    {
        for(int i=0; i<root->childCount(); ++i)
        {
            TreeItem *child=root->child(i);
            if(child->getField("name")==containerName)
                return child->getField("id");
        }
    }

    QMap<QString, QString> branchFields;
    branchFields.insert(QStringLiteral("id"), getUniqueId());
    branchFields.insert(QStringLiteral("name"), containerName);
    model->addNewChildBranch(QModelIndex(), branchFields);

    if(!root)
    {
        if(errorText)
            *errorText=tr("Не удалось создать контейнер импорта");
        return QString();
    }

    const int lastRow=root->childCount()-1;
    if(lastRow<0)
    {
        if(errorText)
            *errorText=tr("Не удалось создать контейнер импорта");
        return QString();
    }

    return root->child(lastRow)->getField("id");
}


QString SubscriptionImportEngine::ensureOwnerFolder(KnowTreeModel *model,
                                                    const QString &importedRootId,
                                                    const QString &ownerName,
                                                    QString *errorText)
{
    TreeItem *container=model->getItemById(importedRootId);
    if(!container)
    {
        if(errorText)
            *errorText=tr("Контейнер импорта не найден в дереве");
        return QString();
    }

    for(int i=0; i<container->childCount(); ++i)
    {
        TreeItem *child=container->child(i);
        if(child->getField("name")==ownerName)
            return child->getField("id");
    }

    QMap<QString, QString> branchFields;
    branchFields.insert(QStringLiteral("id"), getUniqueId());
    branchFields.insert(QStringLiteral("name"), ownerName);
    model->addNewChildBranch(model->getIndexByItem(container), branchFields);

    const int lastRow=container->childCount()-1;
    if(lastRow<0)
    {
        if(errorText)
            *errorText=tr("Не удалось создать папку владельца");
        return QString();
    }

    return container->child(lastRow)->getField("id");
}


bool SubscriptionImportEngine::importBranchRecursive(
        SubscriptionRegistry *registry,
        KnowTreeModel *model,
        const QString &localParentId,
        const QString &sharedBranchId,
        const QString &localBranchName,
        QString *errorText)
{
    // Данные ветки публикации
    if(!m_headFragment.branches.contains(sharedBranchId))
    {
        if(errorText)
            *errorText=tr("Ветка %1 не найдена в публикации").arg(sharedBranchId);
        return false;
    }
    const DiffBranchData branchData=m_headFragment.branches.value(sharedBranchId);

    // Локальная ветка-родитель
    TreeItem *parentItem=model->getItemById(localParentId);
    if(!parentItem)
    {
        if(errorText)
            *errorText=tr("Ветка-родитель %1 не найдена в дереве").arg(localParentId);
        return false;
    }

    // Создание локальной копии ветки
    QMap<QString, QString> branchFields;
    branchFields.insert(QStringLiteral("id"), getUniqueId());
    branchFields.insert(QStringLiteral("name"), localBranchName);
    model->addNewChildBranch(model->getIndexByItem(parentItem), branchFields);

    TreeItem *localBranch=parentItem->child(parentItem->childCount()-1);
    const QString localBranchId=localBranch->getField("id");

    // Карта веток подписки: sharedBranchId -> localBranchId
    SubscriptionRecord record=registry->getSubscription(m_meta.branchId);
    if(record.sharedPath.isEmpty())
        record.sharedPath=m_publicationDir;
    if(record.ownerName.isEmpty())
        record.ownerName=m_meta.ownerName;
    record.branchesMap.insert(sharedBranchId, localBranchId);
    registry->addOrUpdateSubscription(record);

    // Записи ветки
    RecordTableData *table=localBranch->recordtableGetTableData();
    for(const QString &sharedRecordId : branchData.recordIds)
    {
        QString localRecordId;
        const AppStatus status=doCreate(registry, table, sharedRecordId, errorText, &localRecordId);
        if(status!=AppStatus::AppliedOk)
            return false;
        Q_UNUSED(localRecordId)
    }

    // Вложенные ветки (рекурсивно)
    for(const QString &childSharedId : branchData.childIds)
    {
        if(!m_headFragment.branches.contains(childSharedId))
            continue;
        const DiffBranchData childData=m_headFragment.branches.value(childSharedId);
        if(!importBranchRecursive(registry, model, localBranchId, childSharedId,
                                  childData.name, errorText))
            return false;
    }

    if(sharedBranchId==m_meta.branchId)
        m_localTargetBranchId=localBranchId;

    return true;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyChange(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const DiffChange &change,
        QString *errorText,
        QString *localRecordIdOut,
        KnowTreeModel *model)
{
    if(!m_loaded)
    {
        if(errorText)
            *errorText=tr("Публикация не загружена");
        return AppStatus::Failed;
    }

    switch(change.type)
    {
        case DiffChange::RecordAdd:
            return applyAdd(registry, table, change, errorText, localRecordIdOut);

        case DiffChange::RecordUpdate:
            return applyUpdate(registry, table, change, ConflictDecision::DecisionReplace,
                               false, errorText, localRecordIdOut);

        case DiffChange::RecordDelete:
            return applyDelete(registry, table, change, errorText);

        case DiffChange::BranchAdd:
            return applyBranchAdd(registry, model, change, errorText);

        case DiffChange::BranchRename:
            return applyBranchRename(registry, model, change, errorText);

        case DiffChange::BranchMove:
            return applyBranchMove(registry, model, change, errorText);

        case DiffChange::BranchDelete:
            return applyBranchDelete(registry, model, change, errorText);
    }

    return AppStatus::Failed;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyChangeWithDecision(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const DiffChange &change,
        ConflictDecision decision,
        QString *errorText,
        QString *localRecordIdOut,
        KnowTreeModel *model)
{
    if(!m_loaded)
    {
        if(errorText)
            *errorText=tr("Публикация не загружена");
        return AppStatus::Failed;
    }

    switch(change.type)
    {
        case DiffChange::RecordUpdate:
            return applyUpdate(registry, table, change, decision, true, errorText, localRecordIdOut);

        case DiffChange::RecordAdd:
            return applyAdd(registry, table, change, errorText, localRecordIdOut);

        case DiffChange::RecordDelete:
            return applyDelete(registry, table, change, errorText);

        case DiffChange::BranchAdd:
            return applyBranchAdd(registry, model, change, errorText);

        case DiffChange::BranchRename:
            return applyBranchRename(registry, model, change, errorText);

        case DiffChange::BranchMove:
            return applyBranchMove(registry, model, change, errorText);

        case DiffChange::BranchDelete:
            return applyBranchDelete(registry, model, change, errorText);
    }

    return AppStatus::Failed;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyAdd(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const DiffChange &change,
        QString *errorText,
        QString *localRecordIdOut)
{
    // Запись может быть уже импортирована (baseline ещё не продвинут) —
    // тогда повторное применение не создаёт дубликат, а обновляет её
    if(getRecordsMap(registry).contains(change.recordId))
        return applyUpdate(registry, table, change, ConflictDecision::DecisionReplace,
                           false, errorText, localRecordIdOut);

    return doCreate(registry, table, change.recordId, errorText, localRecordIdOut);
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyUpdate(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const DiffChange &change,
        ConflictDecision decision,
        bool decisionExplicit,
        QString *errorText,
        QString *localRecordIdOut)
{
    const QMap<QString, QPair<QString, QString>> recordsMap=getRecordsMap(registry);

    // Связи нет — запись воспринимается как новая
    if(!recordsMap.contains(change.recordId))
        return doCreate(registry, table, change.recordId, errorText, localRecordIdOut);

    const QString localRecordId=recordsMap.value(change.recordId).first;
    const QString lastSnapshot=recordsMap.value(change.recordId).second;

    // Локальная копия удалена пользователем — создаётся заново
    const int pos=table->getPosById(localRecordId);
    if(pos<0)
        return doCreate(registry, table, change.recordId, errorText, localRecordIdOut);

    // Детект локальной правки: сравнивается текущий текст с сохранённым снапшотом
    Record fatRecord=table->getRecordFat(pos);
    const QString currentText=fatRecord.getText();

    if(BranchDiffEngine::sha256(currentText.toUtf8())!=lastSnapshot)
    {
        if(!decisionExplicit)
            return AppStatus::NeedsDecision;

        if(decision==ConflictDecision::DecisionKeepBoth)
            return doKeepBoth(registry, table, change, localRecordId, errorText, localRecordIdOut);
        // DecisionReplace — замена текущей версией владельца
    }

    return doReplace(registry, table, change.recordId, localRecordId, errorText, localRecordIdOut);
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyDelete(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const DiffChange &change,
        QString *errorText)
{
    Q_UNUSED(errorText)
    const QMap<QString, QPair<QString, QString>> recordsMap=getRecordsMap(registry);

    if(recordsMap.contains(change.recordId))
    {
        const QString localRecordId=recordsMap.value(change.recordId).first;

        // Локальная запись (если она ещё существует) переносится в корзину
        if(table && table->isRecordExists(localRecordId))
            table->deleteRecordById(localRecordId);

        removeRecordMapping(registry, change.recordId);
    }

    // Факт применения запоминается отдельно: связь стёрта и локальный объект
    // исчез, иначе свежий дифф показывал бы удаление вечно
    appendAppliedDelete(registry, QStringLiteral("record:")+change.recordId);

    return AppStatus::AppliedOk;
}


// Локальная ветка для shared-ветки: прямое отображение, иначе подъём
// по родителям HEAD-фрагмента до ближайшей отображённой (корень — фолбэк
// вызывающего кода). Пусто, если не отображено ничего
QString SubscriptionImportEngine::localBranchForShared(SubscriptionRegistry *registry,
                                                       const QString &sharedBranchId) const
{
    if(sharedBranchId.isEmpty() || !registry)
        return QString();

    const SubscriptionRecord record=registry->getSubscription(m_meta.branchId);

    if(record.branchesMap.contains(sharedBranchId))
        return record.branchesMap.value(sharedBranchId);

    QString current=sharedBranchId;
    QSet<QString> visited;
    while(!current.isEmpty() && !visited.contains(current))
    {
        visited.insert(current);
        if(!m_headFragment.branches.contains(current))
            break;
        current=m_headFragment.branches.value(current).parentId;
        if(record.branchesMap.contains(current))
            return record.branchesMap.value(current);
    }

    return QString();
}


// Сохранение связи sharedBranchId -> localBranchId
void SubscriptionImportEngine::setBranchMapping(SubscriptionRegistry *registry,
                                                const QString &sharedBranchId,
                                                const QString &localBranchId)
{
    if(!registry)
        return;

    SubscriptionRecord record=registry->getSubscription(m_meta.branchId);

    if(record.sharedPath.isEmpty())
        record.sharedPath=m_publicationDir;
    if(record.ownerName.isEmpty())
        record.ownerName=m_meta.ownerName;

    record.branchesMap.insert(sharedBranchId, localBranchId);

    registry->addOrUpdateSubscription(record);
}


// Удаление из карт всех связей поддерева локальной ветки
// (вызывается ДО удаления ветки из модели, пока дерево живо)
void SubscriptionImportEngine::purgeLocalSubtreeMappings(SubscriptionRegistry *registry,
                                                         TreeItem *localSubtreeRoot)
{
    if(!registry || !localSubtreeRoot)
        return;

    QStringList localBranchIds;
    QStringList localRecordIds;

    QList<TreeItem*> stack;
    stack.append(localSubtreeRoot);
    while(!stack.isEmpty())
    {
        TreeItem *item=stack.takeLast();
        localBranchIds.append(item->getField("id"));

        RecordTableData *branchTable=item->recordtableGetTableData();
        if(branchTable)
        {
            for(unsigned int i=0; i<branchTable->size(); ++i)
            {
                Record *record=branchTable->getRecord(static_cast<int>(i));
                if(record)
                    localRecordIds.append(record->getField("id"));
            }
        }

        for(int i=0; i<item->childCount(); ++i)
            stack.append(item->child(i));
    }

    SubscriptionRecord record=registry->getSubscription(m_meta.branchId);

    const QList<QString> branchKeys=record.branchesMap.keys();
    for(const QString &key : branchKeys)
    {
        if(localBranchIds.contains(record.branchesMap.value(key)))
            record.branchesMap.remove(key);
    }

    const QList<QString> recordKeys=record.recordsMap.keys();
    for(const QString &key : recordKeys)
    {
        if(localRecordIds.contains(record.recordsMap.value(key).first))
            record.recordsMap.remove(key);
    }

    registry->addOrUpdateSubscription(record);
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyBranchAdd(
        SubscriptionRegistry *registry,
        KnowTreeModel *model,
        const DiffChange &change,
        QString *errorText)
{
    if(!registry || !model)
    {
        if(errorText)
            *errorText=tr("Нет модели дерева для создания ветки");
        return AppStatus::Failed;
    }

    // Уже создана (идемпотентность: baseline ещё не продвинут)
    const SubscriptionRecord record=registry->getSubscription(m_meta.branchId);
    if(record.branchesMap.contains(change.branchId))
        return AppStatus::AppliedOk;

    if(!m_headFragment.branches.contains(change.branchId))
    {
        if(errorText)
            *errorText=tr("Ветка %1 не найдена в публикации").arg(change.branchId);
        return AppStatus::Skipped;
    }

    // Родитель: прямой или ближайший отображённый предок, корень — фолбэк
    const QString headParentId=m_headFragment.branches.value(change.branchId).parentId;
    QString localParentId=localBranchForShared(registry, headParentId);
    if(localParentId.isEmpty())
        localParentId=record.branchesMap.value(m_meta.branchId);
    if(localParentId.isEmpty())
    {
        if(errorText)
            *errorText=tr("Локальная копия публикации не найдена");
        return AppStatus::Failed;
    }

    TreeItem *parentItem=model->getItemById(localParentId);
    if(!parentItem)
    {
        if(errorText)
            *errorText=tr("Ветка-приёмник не найдена в дереве");
        return AppStatus::Failed;
    }

    QString branchName=m_headFragment.branches.value(change.branchId).name;
    if(branchName.isEmpty())
        branchName=change.title;

    QMap<QString, QString> branchFields;
    branchFields.insert(QStringLiteral("id"), getUniqueId());
    branchFields.insert(QStringLiteral("name"), branchName);
    model->addNewChildBranch(model->getIndexByItem(parentItem), branchFields);

    TreeItem *newBranch=parentItem->child(parentItem->childCount()-1);
    if(!newBranch)
    {
        if(errorText)
            *errorText=tr("Не удалось создать ветку");
        return AppStatus::Failed;
    }

    setBranchMapping(registry, change.branchId, newBranch->getField("id"));

    return AppStatus::AppliedOk;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyBranchRename(
        SubscriptionRegistry *registry,
        KnowTreeModel *model,
        const DiffChange &change,
        QString *errorText)
{
    Q_UNUSED(errorText)

    if(!registry || !model)
        return AppStatus::Skipped;

    const QString localBranchId=registry->getSubscription(m_meta.branchId)
                                .branchesMap.value(change.branchId);
    if(localBranchId.isEmpty())
        return AppStatus::Skipped; // Ветка не импортирована — нечего переименовывать

    TreeItem *item=model->getItemById(localBranchId);
    if(!item)
        return AppStatus::Skipped;

    QString newName;
    if(m_headFragment.branches.contains(change.branchId))
        newName=m_headFragment.branches.value(change.branchId).name;
    if(newName.isEmpty())
        newName=change.title;
    if(newName.isEmpty() || item->getField("name")==newName)
        return AppStatus::AppliedOk;

    item->setField("name", newName);

    return AppStatus::AppliedOk;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyBranchMove(
        SubscriptionRegistry *registry,
        KnowTreeModel *model,
        const DiffChange &change,
        QString *errorText)
{
    if(!registry || !model)
    {
        if(errorText)
            *errorText=tr("Нет модели дерева для перемещения ветки");
        return AppStatus::Failed;
    }

    const QString localBranchId=registry->getSubscription(m_meta.branchId)
                                .branchesMap.value(change.branchId);
    if(localBranchId.isEmpty())
        return AppStatus::Skipped;

    TreeItem *item=model->getItemById(localBranchId);
    if(!item)
        return AppStatus::Skipped;

    const QString localNewParentId=localBranchForShared(registry, change.newParentId);
    if(localNewParentId.isEmpty())
        return AppStatus::Skipped; // Родитель не импортирован — честно пропускаем

    TreeItem *newParent=model->getItemById(localNewParentId);
    if(!newParent)
        return AppStatus::Skipped;

    if(item->parent()==newParent)
        return AppStatus::AppliedOk;

    if(!model->moveBranchToParent(item, newParent))
    {
        if(errorText)
            *errorText=tr("Не удалось переместить ветку «%1»").arg(change.title);
        return AppStatus::Failed;
    }

    return AppStatus::AppliedOk;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::applyBranchDelete(
        SubscriptionRegistry *registry,
        KnowTreeModel *model,
        const DiffChange &change,
        QString *errorText)
{
    Q_UNUSED(errorText)

    if(!registry || !model)
        return AppStatus::Skipped;

    // Корень подписки не удаляем никогда (отзыв публикации — отдельный путь)
    if(change.branchId==m_meta.branchId)
        return AppStatus::Skipped;

    SubscriptionRecord record=registry->getSubscription(m_meta.branchId);
    const QString localBranchId=record.branchesMap.value(change.branchId);
    if(localBranchId.isEmpty())
        return AppStatus::AppliedOk; // Локальной копии нет — нечего удалять

    TreeItem *item=model->getItemById(localBranchId);
    if(!item)
    {
        // Призрачное отображение — чистим и считаем применённым
        record.branchesMap.remove(change.branchId);
        registry->addOrUpdateSubscription(record);
        appendAppliedDelete(registry, QStringLiteral("branch:")+change.branchId);
        return AppStatus::AppliedOk;
    }

    // Чистка карт, пока поддерево живо
    purgeLocalSubtreeMappings(registry, item);

    // Удаление через штатный путь модели: файлы записей — в корзину,
    // откреплённые окна закрываются, виды уведомляются
    QModelIndexList indexList;
    indexList << model->getIndexByItem(item);
    model->deleteItemsByModelIndexList(indexList);

    appendAppliedDelete(registry, QStringLiteral("branch:")+change.branchId);

    return AppStatus::AppliedOk;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::doCreate(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const QString &sharedRecordId,
        QString *errorText,
        QString *localRecordIdOut)
{
    if(!m_headFragment.records.contains(sharedRecordId))
    {
        if(errorText)
            *errorText=tr("Запись %1 не найдена в публикации").arg(sharedRecordId);
        return AppStatus::Failed;
    }

    const DiffRecordData recordData=m_headFragment.records.value(sharedRecordId);

    const QDomElement recordElement=findRecordElement(sharedRecordId);
    if(recordElement.isNull())
    {
        if(errorText)
            *errorText=tr("Запись %1 не найдена в branch.xml").arg(sharedRecordId);
        return AppStatus::Failed;
    }

    const QString sharedText=readSharedRecordText(recordData);

    // Создаётся полновесная запись: поля и вложения из branch.xml
    Record record;
    record.setupDataFromDom(recordElement);

    // Принудительно новые локальные id и каталог (защита от конфликта ID)
    record.setField("id", QString());

    // Переход в полновесный режим и запись текста
    record.switchToFat();
    record.setText(sharedText);

    // Вставка в конец таблицы через штатный механизм
    const int pos=table->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                         0, record);
    if(pos<0)
    {
        if(errorText)
            *errorText=tr("Не удалось вставить запись в ветку");
        return AppStatus::Failed;
    }

    Record *inserted=table->getRecord(pos);
    if(!inserted)
    {
        if(errorText)
            *errorText=tr("Запись вставлена, но не найдена в таблице");
        return AppStatus::Failed;
    }

    const QString localRecordId=inserted->getField("id");
    const QString localDirPath=mytetraConfig.get_tetradir()+"/base/"+inserted->getField("dir");

    // Копирование файлов записи (картинки, вложения) из каталога публикации
    if(!copyRecordFiles(recordData, localDirPath, errorText))
        return AppStatus::Failed;

    // Снапшот импортированного текста — для детекта локальных правок
    const QString snapshotHash=BranchDiffEngine::sha256(sharedText.toUtf8());
    setRecordMapping(registry, sharedRecordId, localRecordId, snapshotHash);

    if(localRecordIdOut)
        *localRecordIdOut=localRecordId;

    return AppStatus::AppliedOk;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::doReplace(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const QString &sharedRecordId,
        const QString &localRecordId,
        QString *errorText,
        QString *localRecordIdOut)
{
    if(!m_headFragment.records.contains(sharedRecordId))
    {
        if(errorText)
            *errorText=tr("Запись %1 не найдена в публикации").arg(sharedRecordId);
        return AppStatus::Failed;
    }

    const DiffRecordData recordData=m_headFragment.records.value(sharedRecordId);
    const QString sharedText=readSharedRecordText(recordData);

    const int pos=table->getPosById(localRecordId);
    if(pos<0)
    {
        // Локальная копия исчезла — запись создаётся заново
        return doCreate(registry, table, sharedRecordId, errorText, localRecordIdOut);
    }

    // Файлы: устаревшие — в корзину, новые — из публикации
    const QString localDirPath=mytetraConfig.get_tetradir()+"/base/"+table->getField("dir", pos);
    removeStaleFiles(localDirPath, recordData);
    if(!copyRecordFiles(recordData, localDirPath, errorText))
        return AppStatus::Failed;

    // Натуральные поля
    QMap<QString, QString> editFields;
    editFields.insert(QStringLiteral("name"), recordData.name);
    editFields.insert(QStringLiteral("author"), recordData.author);
    editFields.insert(QStringLiteral("url"), recordData.url);
    editFields.insert(QStringLiteral("tags"), recordData.tags);
    table->editRecordFields(pos, editFields);

    // Текст и привязка вложений
    if(writeLocalRecordContent(table, pos, recordData, errorText)!=AppStatus::AppliedOk)
        return AppStatus::Failed;

    // Снапшот обновляется под новое содержимое
    const QString snapshotHash=BranchDiffEngine::sha256(sharedText.toUtf8());
    setRecordMapping(registry, sharedRecordId, localRecordId, snapshotHash);

    if(localRecordIdOut)
        *localRecordIdOut=localRecordId;

    return AppStatus::AppliedOk;
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::doKeepBoth(
        SubscriptionRegistry *registry,
        RecordTableData *table,
        const DiffChange &change,
        const QString &localRecordId,
        QString *errorText,
        QString *localRecordIdOut)
{
    // Локальная правка сохраняется как отдельная копия с суффиксом _conflict-<дата>
    const int pos=table->getPosById(localRecordId);
    if(pos>=0)
    {
        QMap<QString, QString> editFields;
        editFields.insert(QStringLiteral("name"),
                          change.title+QStringLiteral("_conflict-")+(
                              QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
        table->editRecordFields(pos, editFields);
    }

    // Импортируется версия владельца (новая карта ставится на неё)
    return doCreate(registry, table, change.recordId, errorText, localRecordIdOut);
}


QDomElement SubscriptionImportEngine::findRecordElement(const QString &sharedRecordId) const
{
    // Рекурсивный поиск <record> по id в кешированном branch.xml
    std::function<QDomElement(const QDomElement &)> search;
    search=[&search, &sharedRecordId](const QDomElement &parent) -> QDomElement
    {
        QDomElement recordTable=parent.firstChildElement("recordtable");
        QDomElement record=recordTable.firstChildElement("record");
        while(!record.isNull())
        {
            if(record.attribute("id")==sharedRecordId)
                return record;
            record=record.nextSiblingElement("record");
        }

        QDomElement childNode=parent.firstChildElement("node");
        while(!childNode.isNull())
        {
            QDomElement found=search(childNode);
            if(!found.isNull())
                return found;
            childNode=childNode.nextSiblingElement("node");
        }

        return QDomElement();
    };

    QDomElement rootNode=m_branchDoc.documentElement().firstChildElement("node");
    if(rootNode.isNull())
        return QDomElement();

    return search(rootNode);
}


QString SubscriptionImportEngine::readSharedRecordText(const DiffRecordData &recordData) const
{
    QFile recordFile(sharedRecordDir(recordData)+"/"
                     + (recordData.file.isEmpty()? QStringLiteral("text.html") : recordData.file));
    if(!recordFile.open(QIODevice::ReadOnly))
        return QString();
    const QString content=QString::fromUtf8(recordFile.readAll());
    recordFile.close();
    return content;
}


QString SubscriptionImportEngine::sharedRecordDir(const DiffRecordData &recordData) const
{
    return m_publicationDir+"/records/"+recordData.dir;
}


// Текст записи публикации (HEAD) для построчного diff
QString SubscriptionImportEngine::headRecordText(const QString &sharedRecordId) const
{
    if(!m_loaded || !m_headFragment.records.contains(sharedRecordId))
        return QString();

    return readSharedRecordText(m_headFragment.records.value(sharedRecordId));
}


// Базовые каталоги для рендера «Было / Стало»
QPair<QString, QString> SubscriptionImportEngine::recordBaseDirsForChange(
        SubscriptionRegistry *registry,
        KnowTreeModel *model,
        const DiffChange &change,
        RecordTableData *fallbackTable) const
{
    QPair<QString, QString> result;

    if(!m_loaded || !registry || change.recordId.isEmpty())
        return result;

    // Стало: каталог записи публикации
    if(m_headFragment.records.contains(change.recordId))
    {
        const DiffRecordData &headRecord=m_headFragment.records.value(change.recordId);
        result.second=QFileInfo(sharedRecordDir(headRecord)).absoluteFilePath();
    }

    // Было: каталог локальной копии
    const QMap<QString, QPair<QString, QString>> recordsMap=getRecordsMap(registry);
    if(!recordsMap.contains(change.recordId))
        return result;

    RecordTableData *table=resolveTable(registry, model, change, fallbackTable);
    if(!table)
        return result;

    const int pos=table->getPosById(recordsMap.value(change.recordId).first);
    if(pos<0)
        return result;

    Record *liteRecord=table->getRecord(pos);
    if(!liteRecord)
        return result;

    const QString dir=liteRecord->getField("dir");
    if(dir.isEmpty())
        return result;

    result.first=QFileInfo(mytetraConfig.get_tetradir()+"/base/"+dir).absoluteFilePath();
    return result;
}


// Живой текст локальной копии записи для построчного diff
QString SubscriptionImportEngine::localRecordTextForChange(SubscriptionRegistry *registry,
                                                           KnowTreeModel *model,
                                                           const DiffChange &change,
                                                           RecordTableData *fallbackTable) const
{
    if(!m_loaded || !registry || change.recordId.isEmpty())
        return QString();

    const QMap<QString, QPair<QString, QString>> recordsMap=getRecordsMap(registry);
    if(!recordsMap.contains(change.recordId))
        return QString();

    RecordTableData *table=resolveTable(registry, model, change, fallbackTable);
    if(!table)
        return QString();

    const int pos=table->getPosById(recordsMap.value(change.recordId).first);
    if(pos<0)
        return QString();

    QByteArray content;
    if(!readLocalRecordText(table, pos, &content))
        return QString();

    return QString::fromUtf8(content);
}


bool SubscriptionImportEngine::copyRecordFiles(const DiffRecordData &recordData,
                                               const QString &localDirPath,
                                               QString *errorText) const
{
    const QString skipFileName=recordData.file.isEmpty()? QStringLiteral("text.html") : recordData.file;

    QDir sourceDir(sharedRecordDir(recordData));
    if(!sourceDir.exists())
    {
        if(errorText)
            *errorText=tr("Каталог записи %1 не найден в публикации").arg(sourceDir.absolutePath());
        return false;
    }

    if(!QDir().mkpath(localDirPath))
    {
        if(errorText)
            *errorText=tr("Не удалось создать каталог записи %1").arg(localDirPath);
        return false;
    }

    const QStringList entries=sourceDir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden);
    for(const QString &entry : entries)
    {
        if(entry==skipFileName)
            continue;

        const QString sourcePath=sourceDir.absoluteFilePath(entry);
        const QString targetPath=localDirPath+"/"+entry;

        // Запись существующего файла не поддерживает перезапись — удаляем старую копию
        if(QFile::exists(targetPath))
        {
            QFile targetFile(targetPath);
            targetFile.setPermissions(targetFile.permissions() | QFile::ReadUser | QFile::WriteUser);
            targetFile.remove();
        }

        if(!QFile::copy(sourcePath, targetPath))
        {
            if(errorText)
                *errorText=tr("Не удалось скопировать файл %1 в запись").arg(entry);
            return false;
        }
    }

    return true;
}


void SubscriptionImportEngine::removeStaleFiles(const QString &localDirPath,
                                                const DiffRecordData &recordData) const
{
    const QString skipFileName=recordData.file.isEmpty()? QStringLiteral("text.html") : recordData.file;

    QDir targetDir(localDirPath);
    if(!targetDir.exists())
        return;

    QDir sourceDir(sharedRecordDir(recordData));
    const QStringList sharedEntries=sourceDir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden);

    const QStringList existing=targetDir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden);
    for(const QString &entry : existing)
    {
        if(entry==skipFileName)
            continue;

        if(!sharedEntries.contains(entry))
            DiskHelper::removeFileToTrash(targetDir.absoluteFilePath(entry), false);
    }
}


SubscriptionImportEngine::AppStatus SubscriptionImportEngine::writeLocalRecordContent(
        RecordTableData *table,
        int pos,
        const DiffRecordData &recordData,
        QString *errorText)
{
    // Полновесная копия текущей записи: очищаются картинки и вложения,
    // чтобы pushFatAttributes записал только текст (файлы уже скопированы)
    Record fatRecord=table->getRecordFat(pos);
    fatRecord.setPictureFiles(QMap<QString, QByteArray>());

    AttachTableData emptyAttachTable;
    emptyAttachTable.switchToFat();
    fatRecord.setAttachTable(emptyAttachTable);

    fatRecord.setText(readSharedRecordText(recordData));
    fatRecord.pushFatAttributes();

    // Вложения lite-записи приводим к набору публикации (для XML и интерфейса)
    Record *liteRecord=table->getRecord(pos);
    if(liteRecord)
    {
        const QDomElement recordElement=findRecordElement(recordData.id);
        QDomElement filesElement=recordElement.firstChildElement("files");

        AttachTableData *attachTable=liteRecord->getAttachTablePointer();
        attachTable->clear();
        if(!filesElement.isNull())
            attachTable->setupDataFromDom(filesElement);
    }

    if(errorText)
        errorText->clear();

    return AppStatus::AppliedOk;
}


QMap<QString, QPair<QString, QString>> SubscriptionImportEngine::getRecordsMap(
        SubscriptionRegistry *registry) const
{
    if(!registry)
        return QMap<QString, QPair<QString, QString>>();

    return registry->getSubscription(m_meta.branchId).recordsMap;
}


void SubscriptionImportEngine::setRecordMapping(SubscriptionRegistry *registry,
                                                const QString &sharedRecordId,
                                                const QString &localRecordId,
                                                const QString &snapshotHash)
{
    if(!registry)
        return;

    SubscriptionRecord record=registry->getSubscription(m_meta.branchId);

    if(record.sharedPath.isEmpty())
        record.sharedPath=m_publicationDir;
    if(record.ownerName.isEmpty())
        record.ownerName=m_meta.ownerName;

    record.recordsMap.insert(sharedRecordId,
                             QPair<QString, QString>(localRecordId, snapshotHash));

    registry->addOrUpdateSubscription(record);
}


void SubscriptionImportEngine::removeRecordMapping(SubscriptionRegistry *registry,
                                                   const QString &sharedRecordId)
{
    if(!registry)
        return;

    SubscriptionRecord record=registry->getSubscription(m_meta.branchId);
    if(!record.recordsMap.contains(sharedRecordId))
        return;

    record.recordsMap.remove(sharedRecordId);
    registry->addOrUpdateSubscription(record);
}


// Запись применённого удаления ("record:<id>" / "branch:<id>")
void SubscriptionImportEngine::appendAppliedDelete(SubscriptionRegistry *registry,
                                                   const QString &key)
{
    if(!registry || key.isEmpty())
        return;

    SubscriptionRecord record=registry->getSubscription(m_meta.branchId);
    if(record.appliedDeletes.contains(key))
        return;

    record.appliedDeletes.append(key);
    registry->addOrUpdateSubscription(record);
}


// Таблица-приёмник изменения: где запись реально живёт (чинит наследие,
// когда всё писалось в корневую таблицу) либо таблица своей ветки-копии
RecordTableData *SubscriptionImportEngine::resolveTable(SubscriptionRegistry *registry,
                                                         KnowTreeModel *model,
                                                         const DiffChange &change,
                                                         RecordTableData *fallbackTable) const
{
    // 1. Запись уже импортирована — таблица, где она реально живёт
    if(!change.recordId.isEmpty() && registry)
    {
        const QMap<QString, QPair<QString, QString>> recordsMap=getRecordsMap(registry);
        if(recordsMap.contains(change.recordId))
        {
            const QString localId=recordsMap.value(change.recordId).first;

            // Кандидаты: сначала fallback (корень копии), затем все ветки-копии
            QList<RecordTableData*> candidates;
            if(fallbackTable)
                candidates.append(fallbackTable);
            if(model)
            {
                const SubscriptionRecord sub=registry->getSubscription(m_meta.branchId);
                for(auto it=sub.branchesMap.constBegin(); it!=sub.branchesMap.constEnd(); ++it)
                {
                    TreeItem *branchItem=model->getItemById(it.value());
                    if(branchItem && branchItem->recordtableGetTableData()
                       && !candidates.contains(branchItem->recordtableGetTableData()))
                        candidates.append(branchItem->recordtableGetTableData());
                }
            }

            for(RecordTableData *candidate : candidates)
            {
                if(candidate && candidate->getPosById(localId)>=0)
                    return candidate;
            }
        }
    }

    // 2. Таблица своей ветки-копии
    if(model && !change.branchId.isEmpty() && registry)
    {
        const QString localBranchId=registry->getSubscription(m_meta.branchId)
                                     .branchesMap.value(change.branchId);
        if(!localBranchId.isEmpty())
        {
            TreeItem *branchItem=model->getItemById(localBranchId);
            if(branchItem && branchItem->recordtableGetTableData())
                return branchItem->recordtableGetTableData();
        }
    }

    return fallbackTable;
}


// Отражено ли изменение в локальной копии (для остаточного diff)
bool SubscriptionImportEngine::isChangeReflected(SubscriptionRegistry *registry,
                                                 KnowTreeModel *model,
                                                 const DiffChange &change,
                                                 RecordTableData *fallbackTable)
{
    if(!registry)
        return false;

    const SubscriptionRecord sub=registry->getSubscription(m_meta.branchId);

    switch(change.type)
    {
        case DiffChange::RecordAdd:
        {
            // Добавлена, если связь есть и локальная запись на месте.
            // Ручное удаление копии подписчиком — снова показать как новую
            if(!sub.recordsMap.contains(change.recordId))
                return false;
            RecordTableData *table=resolveTable(registry, model, change, fallbackTable);
            return table && table->getPosById(sub.recordsMap.value(change.recordId).first)>=0;
        }

        case DiffChange::RecordUpdate:
        {
            // Обновлена, если локальное содержимое совпадает с HEAD.
            // Локальная правка после импорта — показать снова (конфликт)
            if(!sub.recordsMap.contains(change.recordId))
                return false;
            if(!m_headFragment.records.contains(change.recordId))
                return false;
            RecordTableData *table=resolveTable(registry, model, change, fallbackTable);
            if(!table)
                return false;
            const QString localId=sub.recordsMap.value(change.recordId).first;
            const int pos=table->getPosById(localId);
            if(pos<0)
                return false;

            // Живой текст — напрямую из файла, без побочных эффектов
            // (getRecordFat показывает модальное окно при битом каталоге)
            QByteArray liveContent;
            if(!readLocalRecordText(table, pos, &liveContent))
                return false;
            const DiffRecordData &headRecord=m_headFragment.records.value(change.recordId);
            if(BranchDiffEngine::sha256(liveContent)!=headRecord.textSha)
                return false;

            Record *liteRecord=table->getRecord(pos);
            if(!liteRecord)
                return false;
            if(liteRecord->getField("name")!=headRecord.name
               || liteRecord->getField("author")!=headRecord.author
               || liteRecord->getField("url")!=headRecord.url
               || liteRecord->getField("tags")!=headRecord.tags)
                return false;

            return true;
        }

        case DiffChange::RecordDelete:
            return sub.appliedDeletes.contains(QStringLiteral("record:")+change.recordId);

        case DiffChange::BranchAdd:
        {
            if(!sub.branchesMap.contains(change.branchId))
                return false;
            return model && model->getItemById(sub.branchesMap.value(change.branchId))!=nullptr;
        }

        case DiffChange::BranchRename:
        {
            if(!sub.branchesMap.contains(change.branchId))
                return false;
            if(!model)
                return false;
            TreeItem *item=model->getItemById(sub.branchesMap.value(change.branchId));
            if(!item)
                return false;
            QString headName;
            if(m_headFragment.branches.contains(change.branchId))
                headName=m_headFragment.branches.value(change.branchId).name;
            if(headName.isEmpty())
                headName=change.title;
            return item->getField("name")==headName;
        }

        case DiffChange::BranchMove:
        {
            if(!sub.branchesMap.contains(change.branchId))
                return false;
            if(!model)
                return false;
            TreeItem *item=model->getItemById(sub.branchesMap.value(change.branchId));
            if(!item || !item->parent())
                return false;
            QString expectedParent=localBranchForShared(registry, change.newParentId);
            if(expectedParent.isEmpty())
                expectedParent=sub.branchesMap.value(m_meta.branchId);
            if(expectedParent.isEmpty())
                return false;
            return item->parent()->getField("id")==expectedParent;
        }

        case DiffChange::BranchDelete:
            return sub.appliedDeletes.contains(QStringLiteral("branch:")+change.branchId);
    }

    return false;
}


// Живой текст локальной записи напрямую из файла, без побочных эффектов
bool SubscriptionImportEngine::readLocalRecordText(RecordTableData *table, int pos,
                                                   QByteArray *contentOut) const
{
    if(!table || !contentOut)
        return false;

    Record *liteRecord=table->getRecord(pos);
    if(!liteRecord)
        return false;

    // Шифрованные записи дёшево не сверить — показываем изменение
    if(liteRecord->getField("crypt")=="1")
        return false;

    QString dir=liteRecord->getField("dir");
    QString file=liteRecord->getField("file");
    if(file.isEmpty())
        file=QStringLiteral("text.html");
    if(dir.isEmpty())
        return false;

    QFile textFile(mytetraConfig.get_tetradir()+"/base/"+dir+"/"+file);
    if(!textFile.open(QIODevice::ReadOnly))
        return false;

    *contentOut=textFile.readAll();
    return true;
}


// Остаточный diff: свежие изменения минус уже отражённые в локальной копии
// минус применённые удаления
QList<DiffChange> SubscriptionImportEngine::pendingChanges(SubscriptionRegistry *registry,
                                                           KnowTreeModel *model,
                                                           const QString &baselineSnapshot,
                                                           RecordTableData *fallbackTable)
{
    QList<DiffChange> fresh;
    if(!baselineSnapshot.isEmpty())
        fresh=changesSinceSnapshot(baselineSnapshot);
    else
        fresh=changesSince(QString());

    QList<DiffChange> pending;
    for(const DiffChange &change : fresh)
    {
        if(!isChangeReflected(registry, model, change, fallbackTable))
            pending.append(change);
    }

    return pending;
}
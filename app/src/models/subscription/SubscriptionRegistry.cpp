#include <QFile>
#include <QDebug>
#include <QTextCodec>
#include <QSettings>

#include "SubscriptionRegistry.h"

#include "libraries/GlobalParameters.h"

extern GlobalParameters globalParameters;


const QString SUBSCRIPTION_FIELD_SHARED_PATH="sharedPath";
const QString SUBSCRIPTION_FIELD_OWNER_NAME="ownerName";
const QString SUBSCRIPTION_FIELD_BASELINE_SNAPSHOT="baselineSnapshot";
const QString SUBSCRIPTION_FIELD_BASELINE_VERSION="baselinePublishVersion";
const QString SUBSCRIPTION_FIELD_LOCAL_TARGET="localTargetBranchId";
const QString SUBSCRIPTION_FIELD_APPLIED_DELETES="appliedDeletes";


SubscriptionRegistry::SubscriptionRegistry(QObject *pobj)
{
    Q_UNUSED(pobj)
    m_conf=nullptr;
    isInitFlag=false;
}


SubscriptionRegistry::~SubscriptionRegistry()
{
    if(isInitFlag && m_conf)
    {
        qDebug() << "Save subscriptions config file";
        m_conf->sync();
        delete m_conf;
    }
}


void SubscriptionRegistry::init(void)
{
    if(isInitFlag)
        return;

    // Открывается хранилище конфигурации (файл создается при первой записи)
    m_conf=new QSettings(this->getConfigFileName(), QSettings::IniFormat, this);
    m_conf->setIniCodec( QTextCodec::codecForName("UTF-8") );
    m_conf->sync();

    isInitFlag=true;
}


bool SubscriptionRegistry::isInit(void)
{
    return isInitFlag;
}


QString SubscriptionRegistry::getConfigFileName(void) const
{
    return globalParameters.getWorkDirectory()+"/subscriptions.ini";
}


QStringList SubscriptionRegistry::listSubscriptionKeys(void) const
{
    if(!m_conf)
        return QStringList();

    return m_conf->childGroups();
}


bool SubscriptionRegistry::isSubscribed(const QString &branchId) const
{
    if(!m_conf)
        return false;

    return m_conf->contains(branchId+"/"+SUBSCRIPTION_FIELD_SHARED_PATH);
}


SubscriptionRecord SubscriptionRegistry::getSubscription(const QString &branchId) const
{
    SubscriptionRecord record;
    record.branchId=branchId;

    if(!m_conf)
        return record;

    record.sharedPath=m_conf->value(branchId+"/"+SUBSCRIPTION_FIELD_SHARED_PATH).toString();
    record.ownerName=m_conf->value(branchId+"/"+SUBSCRIPTION_FIELD_OWNER_NAME).toString();
    record.baselineSnapshot=m_conf->value(branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_SNAPSHOT).toString();
    record.baselinePublishVersion=m_conf->value(branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_VERSION).toInt();
    record.localTargetBranchId=m_conf->value(branchId+"/"+SUBSCRIPTION_FIELD_LOCAL_TARGET).toString();
    record.appliedDeletes=m_conf->value(branchId+"/"+SUBSCRIPTION_FIELD_APPLIED_DELETES).toStringList();

    loadRecordsMap(record);
    loadBranchesMap(record);

    return record;
}


void SubscriptionRegistry::addOrUpdateSubscription(const SubscriptionRecord &record)
{
    if(!m_conf)
        return;

    m_conf->setValue(record.branchId+"/"+SUBSCRIPTION_FIELD_SHARED_PATH, record.sharedPath);
    m_conf->setValue(record.branchId+"/"+SUBSCRIPTION_FIELD_OWNER_NAME, record.ownerName);
    if(!record.baselineSnapshot.isEmpty())
        m_conf->setValue(record.branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_SNAPSHOT, record.baselineSnapshot);
    if(record.baselinePublishVersion>0)
        m_conf->setValue(record.branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_VERSION, record.baselinePublishVersion);
    if(!record.localTargetBranchId.isEmpty())
        m_conf->setValue(record.branchId+"/"+SUBSCRIPTION_FIELD_LOCAL_TARGET, record.localTargetBranchId);
    if(!record.appliedDeletes.isEmpty())
        m_conf->setValue(record.branchId+"/"+SUBSCRIPTION_FIELD_APPLIED_DELETES, record.appliedDeletes);
    else
        m_conf->remove(record.branchId+"/"+SUBSCRIPTION_FIELD_APPLIED_DELETES);

    writeRecordsMap(record);
    writeBranchesMap(record);

    m_conf->sync();

    emit subscriptionsChanged();
}


void SubscriptionRegistry::removeSubscription(const QString &branchId)
{
    if(!m_conf)
        return;

    m_conf->beginGroup(branchId);
    QStringList keys=m_conf->allKeys();
    for(const QString &key : keys)
        m_conf->remove(key);
    m_conf->endGroup();

    m_conf->sync();

    emit subscriptionsChanged();
}


void SubscriptionRegistry::setBaselineState(const QString &branchId, const QString &snapshot,
                                            int publishVersion)
{
    if(!m_conf)
        return;

    if(snapshot.isEmpty())
        m_conf->remove(branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_SNAPSHOT);
    else
        m_conf->setValue(branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_SNAPSHOT, snapshot);

    if(publishVersion<=0)
        m_conf->remove(branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_VERSION);
    else
        m_conf->setValue(branchId+"/"+SUBSCRIPTION_FIELD_BASELINE_VERSION, publishVersion);

    // Базовая точка сдвинута к HEAD: применённые ранее удаления больше
    // не нужны (свежий дифф их не содержит), список чистится
    m_conf->remove(branchId+"/"+SUBSCRIPTION_FIELD_APPLIED_DELETES);

    m_conf->sync();

    emit subscriptionsChanged();
}


void SubscriptionRegistry::sync(void)
{
    if(m_conf)
        m_conf->sync();
}


// Чтение карты импортированных записей (recordsMap) в запись подписки
void SubscriptionRegistry::loadRecordsMap(SubscriptionRecord &record) const
{
    if(!m_conf)
        return;

    m_conf->beginGroup(record.branchId);
    m_conf->beginGroup("recordsMap");

    const QStringList sharedIds=m_conf->childGroups();
    for(const QString &sharedId : sharedIds)
    {
        m_conf->beginGroup(sharedId);
        const QString localId=m_conf->value("localId").toString();
        const QString snapshot=m_conf->value("snapshot").toString();
        if(!localId.isEmpty() && !snapshot.isEmpty())
            record.recordsMap.insert(sharedId, QPair<QString, QString>(localId, snapshot));
        m_conf->endGroup();
    }

    m_conf->endGroup();
    m_conf->endGroup();
}


// Полная перезапись карты импортированных записей подписки
void SubscriptionRegistry::writeRecordsMap(const SubscriptionRecord &record)
{
    if(!m_conf)
        return;

    m_conf->beginGroup(record.branchId);
    m_conf->beginGroup("recordsMap");

    // Удаляются прежние связи
    const QStringList oldSharedIds=m_conf->childGroups();
    for(const QString &sharedId : oldSharedIds)
        m_conf->remove(sharedId);

    for(auto it=record.recordsMap.constBegin(); it!=record.recordsMap.constEnd(); ++it)
    {
        m_conf->beginGroup(it.key());
        m_conf->setValue("localId", it.value().first);
        m_conf->setValue("snapshot", it.value().second);
        m_conf->endGroup();
    }

    m_conf->endGroup();
    m_conf->endGroup();
}


// Чтение карты импортированных веток (branchesMap) в запись подписки
void SubscriptionRegistry::loadBranchesMap(SubscriptionRecord &record) const
{
    if(!m_conf)
        return;

    m_conf->beginGroup(record.branchId);
    m_conf->beginGroup("branchesMap");

    const QStringList sharedIds=m_conf->childGroups();
    for(const QString &sharedId : sharedIds)
    {
        m_conf->beginGroup(sharedId);
        const QString localId=m_conf->value("localId").toString();
        if(!localId.isEmpty())
            record.branchesMap.insert(sharedId, localId);
        m_conf->endGroup();
    }

    m_conf->endGroup();
    m_conf->endGroup();
}


// Полная перезапись карты импортированных веток подписки
void SubscriptionRegistry::writeBranchesMap(const SubscriptionRecord &record)
{
    if(!m_conf)
        return;

    m_conf->beginGroup(record.branchId);
    m_conf->beginGroup("branchesMap");

    // Удаляются прежние связи
    const QStringList oldSharedIds=m_conf->childGroups();
    for(const QString &sharedId : oldSharedIds)
        m_conf->remove(sharedId);

    for(auto it=record.branchesMap.constBegin(); it!=record.branchesMap.constEnd(); ++it)
    {
        m_conf->beginGroup(it.key());
        m_conf->setValue("localId", it.value());
        m_conf->endGroup();
    }

    m_conf->endGroup();
    m_conf->endGroup();
}
#ifndef _SUBSCRIPTIONREGISTRY_H_
#define _SUBSCRIPTIONREGISTRY_H_

#include <QObject>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

class QSettings;

// Запись об одной подписке
struct SubscriptionRecord
{
    QString branchId;               // Машинный ключ подписки (branchId из имени каталога публикации)
    QString sharedPath;             // Путь к каталогу публикации в shared/
    QString ownerName;              // Имя владельца публикации (для отображения)

    // Манифест-снимок состояния публикации на момент последнего просмотра/импорта
    // (JSON: branch.xml среза + карта recordId -> sha256 текстов записей).
    // Дифф и детект не зависят от git
    QString baselineSnapshot;

    // publishVersion публикации в момент последнего просмотра/импорта
    // (дешёвый признак обновления; точное сравнение - по диджестам)
    int baselinePublishVersion=0;

    QString localTargetBranchId;    // Локальная ветка-копия (для импорта); может быть пустой

    // Карта импортированных записей: sharedRecordId -> (localRecordId, snapshotHash)
    // snapshotHash - sha256 локального text.html на момент последнего импорта
    QMap<QString, QPair<QString, QString>> recordsMap;

    // Карта импортированных веток: sharedBranchId -> localBranchId
    QMap<QString, QString> branchesMap;

    // Применённые удаления: "record:<sharedRecordId>" / "branch:<sharedBranchId>".
    // Хранятся отдельно: после применения связь из карт стирается,
    // а свежий дифф baseline->HEAD удаление уже не содержит
    QStringList appliedDeletes;
};

// Реестр подписок на публикации в общем каталоге shared/.
// Хранится в <рабочий каталог>/subscriptions.ini (QSettings, INI-формат),
// секция на каждую подписку. Файл не входит в БД и не реплицируется

class SubscriptionRegistry : public QObject
{
    Q_OBJECT

public:
    SubscriptionRegistry(QObject *pobj=nullptr);
    ~SubscriptionRegistry();

    void init(void);
    bool isInit(void);
    void sync(void);

    QString getConfigFileName(void) const;

    QStringList listSubscriptionKeys(void) const;
    bool isSubscribed(const QString &branchId) const;
    SubscriptionRecord getSubscription(const QString &branchId) const;
    void addOrUpdateSubscription(const SubscriptionRecord &record);
    void removeSubscription(const QString &branchId);

    // Сдвинуть базовую точку к HEAD: свежий дифф применённые
    // ранее удаления уже не содержит, список чистится
    void setBaselineState(const QString &branchId, const QString &snapshot,
                          int publishVersion);

signals:
    void subscriptionsChanged(void);

private:

    void loadRecordsMap(SubscriptionRecord &record) const;
    void writeRecordsMap(const SubscriptionRecord &record);
    void loadBranchesMap(SubscriptionRecord &record) const;
    void writeBranchesMap(const SubscriptionRecord &record);

    QSettings *m_conf;
    bool isInitFlag;
};

#endif /* _SUBSCRIPTIONREGISTRY_H_ */

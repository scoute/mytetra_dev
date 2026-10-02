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
    QString sharedPath;             // Путь к каталогу публикации в shared/ (sync/<owner>/<branch> или legacy)
    QString ownerName;              // Имя владельца публикации (для отображения)

    // Манифест-снимок состояния публикации на момент последнего просмотра/импорта
    // (JSON: branch.xml среза + карта recordId -> sha256 текстов записей).
    // Заменяет прежний baselineCommit: diff/детект больше не зависят от git (ADR-015, 7.2).
    QString baselineSnapshot;

    // publishVersion публикации в момент последнего просмотра/импорта
    // (дешёвый признак обновления; точное сравнение — по диджестам)
    int baselinePublishVersion=0;

    QString localTargetBranchId;    // Локальная ветка-копия (для импорта, Фаза 4); может быть пустой

    // Карта импортированных записей: sharedRecordId -> (localRecordId, snapshotHash)
    // snapshotHash — sha256 локального text.html на момент последнего импорта
    QMap<QString, QPair<QString, QString>> recordsMap;

    // Карта импортированных веток: sharedBranchId -> localBranchId.
    // Нужна для обновления структуры копии (создание/переименование/перемещение
    // под-веток владельца) и для определения зоны read-only
    QMap<QString, QString> branchesMap;

    // Применённые удаления: "record:<sharedRecordId>" / "branch:<sharedBranchId>".
    // После применения удаления связь из карт стирается и локальный объект
    // исчезает, поэтому факт «уже применено» хранится отдельно: иначе свежий
    // дифф baseline→HEAD показывал бы удаление вечно (baseline двигается
    // только при полном применении или «отметить просмотренным», тогда
    // список чистится). Остаточный diff = свежие изменения минус отражённые
    // в локальной копии минус применённые удаления
    QStringList appliedDeletes;
};

// Реестр подписок на публикации в общем каталоге shared/.
// Хранится в <рабочий каталог>/subscriptions.ini (QSettings, INI-формат),
// секция на каждую подписку (по образцу KnownBasesConfig).
// Файл не входит в БД и не реплицируется (см. docs/06, раздел 9)

class SubscriptionRegistry : public QObject
{
    Q_OBJECT

public:
    SubscriptionRegistry(QObject *pobj=nullptr);
    ~SubscriptionRegistry();

    void init(void);
    bool isInit(void);

    QString getConfigFileName(void) const;

    // Список машинных ключей всех подписок
    QStringList listSubscriptionKeys(void) const;

    // Признак наличия подписки на ветку
    bool isSubscribed(const QString &branchId) const;

    // Получение записи о подписке (пустые поля, если подписки нет)
    SubscriptionRecord getSubscription(const QString &branchId) const;

    // Добавление или обновление подписки
    void addOrUpdateSubscription(const SubscriptionRecord &record);

    // Удаление подписки (не влияет на данные в shared/)
    void removeSubscription(const QString &branchId);

    // Обновление базовой точки (baselineSnapshot) подписки на состояние публикации
    void setBaselineState(const QString &branchId, const QString &snapshot,
                          int publishVersion);

    // Принудительная запись изменений на диск
    void sync(void);

signals:
    void subscriptionsChanged(void);

private:
    // Чтение карты импортированных записей подписки
    void loadRecordsMap(SubscriptionRecord &record) const;

    // Полная перезапись карты импортированных записей подписки
    void writeRecordsMap(const SubscriptionRecord &record);

    // Чтение карты импортированных веток подписки
    void loadBranchesMap(SubscriptionRecord &record) const;

    // Полная перезапись карты импортированных веток подписки
    void writeBranchesMap(const SubscriptionRecord &record);

    QSettings *m_conf;
    bool isInitFlag;
};

#endif // _SUBSCRIPTIONREGISTRY_H_
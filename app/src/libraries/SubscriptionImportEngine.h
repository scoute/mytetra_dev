#ifndef _SUBSCRIPTIONIMPORTENGINE_H_
#define _SUBSCRIPTIONIMPORTENGINE_H_

#include <QObject>
#include <QMap>
#include <QPair>
#include <QString>
#include <QDomDocument>

#include "libraries/BranchDiffEngine.h"
#include "libraries/BranchPublisher.h"

class Record;
class QDomElement;
class SubscriptionRegistry;
class RecordTableData;
class KnowTreeModel;
class TreeItem;

// Двигатель выборочного ручного импорта изменений публикации в личную БД.
//
// Жизненный цикл:
//   1. setPublicationDir(path) + loadPublication() — читаются meta.json и
//      актуальный branch.xml (фрагмент HEAD);
//   2. changesSinceSnapshot(baselineSnapshot) — список изменений между
//      манифест-снимком подписчика и HEAD (делегируется BranchDiffEngine;
//      baseline парсится из снимка: branch.xml + карта recordId -> sha256,
//      git для diff/импорта/детекта не нужен — только журнал и восстановление);
//   3. applyCreate/applyUpdate/applyDelete — применение выбранных изменений
//      в указанную таблицу целевой ветки, с сохранением карты
//      sharedRecordId → {localRecordId, snapshotHash} в SubscriptionRegistry.
//
// Импорт идемпотентен: повторное применение уже импортированного изменения
// не создаёт дубликатов (карта проверяется). Никакого автоматического
// импорта нет — только вызовы из UI после явных действий пользователя.

class SubscriptionImportEngine : public QObject
{
    Q_OBJECT

public:
    // Статус применения одного изменения
    enum class AppStatus
    {
        AppliedOk,          // Изменение применено
        NeedsDecision,      // Обнаружена локальная правка — нужно решение пользователя
        Skipped,            // Изменение не применено (пропущено пользователем или не поддерживается)
        Failed              // Ошибка применения
    };

    // Решение при конфликте «локальная правка против обновления владельца»
    enum class ConflictDecision
    {
        DecisionReplace,    // Заменить локальную запись версией владельца
        DecisionKeepBoth    // Сохранить локальную (суффикс _conflict-<дата>) и импортировать версию владельца
    };

    explicit SubscriptionImportEngine(QObject *parent=nullptr);

    // Каталог публикации в shared/
    void setPublicationDir(const QString &publicationDir);
    QString publicationDir(void) const;

    // Загрузка meta.json и актуального branch.xml (HEAD публикации).
    // Метаданные и фрагмент HEAD кешируются для последующих вызовов.
    bool loadPublication(QString *errorText=nullptr);

    // Метаданные публикации (валидны после loadPublication)
    PublicationMeta getMeta(void) const;

    // Готовность публикации к диффу/импорту (манифест версии совпал с
    // фактическими файлами, либо устаревший per-branch макет).
    // Проверка выполняется в момент чтения публикации — без отдельного
    // полного прохода (тексты записей и так читаются для хешей фрагмента).
    // false — данные не доехали полностью (Syncthing) или неверифицируемы
    bool isPublicationComplete(void) const;

    // Готовность отдельной записи к импорту: её файлы совпадают с манифестом
    // версии (защита от частичной доставки в момент применения)
    bool isRecordComplete(const QString &sharedRecordId) const;

    // Текущий HEAD репозитория публикации (пусто, если git недоступен
    // или журнал не найден). Для diff/импорта/детекта не обязателен.
    QString headCommit(void) const;

    // Репозиторий журнала для текущей публикации
    QString repositoryRoot(void) const;

    // Манифест-снимок текущего состояния публикации (JSON): сериализованный
    // branch.xml + карта recordId -> sha256 текстов записей.
    // Сохраняется в подписке (baselineSnapshot) когда базовая точка
    // выравнивается к текущему HEAD.
    QString buildBaselineSnapshot(void) const;

    // Список изменений между baselineSnapshot подписчика и текущим HEAD
    // публикации. Пустой snapshot означает «всё новое».
    QList<DiffChange> changesSinceSnapshot(const QString &baselineSnapshot);

    // Остаточный diff: свежие изменения минус уже отражённые в локальной
    // копии минус применённые удаления. Именно этот список показывается
    // в диалоге: после полного импорта он пуст, после частичного —
    // только остаток. fallbackTable — корневая таблица копии (может быть null)
    QList<DiffChange> pendingChanges(SubscriptionRegistry *registry,
                                     KnowTreeModel *model,
                                     const QString &baselineSnapshot,
                                     RecordTableData *fallbackTable);

    // Таблица-приёмник изменения: где запись реально живёт либо таблица
    // своей ветки-копии. fallbackTable — корневая таблица копии (может быть null)
    RecordTableData *resolveTable(SubscriptionRegistry *registry,
                                  KnowTreeModel *model,
                                  const DiffChange &change,
                                  RecordTableData *fallbackTable) const;

    // Текст записи публикации (HEAD) для построчного diff.
    // Пусто, если запись не найдена или файл недоступен
    QString headRecordText(const QString &sharedRecordId) const;

    // Живой текст локальной копии записи для построчного diff
    // (что у подписчика сейчас). Пусто, если связи нет, копия не найдена
    // или сверить нельзя (шифро/файлы).
    // fallbackTable — корневая таблица копии (может быть null)
    QString localRecordTextForChange(SubscriptionRegistry *registry,
                                     KnowTreeModel *model,
                                     const DiffChange &change,
                                     RecordTableData *fallbackTable) const;

    // Базовые каталоги для рендера «Было / Стало»:
    // (каталог локальной копии, каталог записи публикации).
    // Пустая часть пары — соответствующий рендер недоступен
    QPair<QString, QString> recordBaseDirsForChange(SubscriptionRegistry *registry,
                                                    KnowTreeModel *model,
                                                    const DiffChange &change,
                                                    RecordTableData *fallbackTable) const;

    // Список изменений между baselineCommit и текущим HEAD публикации
    // (git-режим: для устаревших подписок и диагностики).
    // Пустой baselineCommit означает «всё новое».
    QList<DiffChange> changesSince(const QString &baselineCommit);

    // Полный первый импорт поддерева публикации в личную БД.
    //
    // Структура копии:
    //   imported / <ownerName> / <название публикации>
    // (контейнер "imported" и папка владельца создаются при отсутствии).
    // Каждая ветка публикации воспроизводится отдельной веткой:
    // для корня создаётся копия, для под-веток — вложенные копии; записи
    // ветки импортируются с заполнением recordsMap.
    // Ветки реплицируются в ветках branchesMap подписки
    // (sharedBranchId -> localBranchId), поэтому повторный вызов идемпотентен:
    // если копия уже существует — возвращается true без изменений.
    //
    // После успешного вызова localTargetBranchId() возвращает id локальной
    // копии корня публикации.
    bool importFullSubtree(SubscriptionRegistry *registry,
                           KnowTreeModel *model,
                           QString *errorText=nullptr);

    // id локальной копии корня публикации (после importFullSubtree)
    QString localTargetBranchId(void) const;

    // Применение изменения.
    //   table — таблица целевой ветки (RecordTableData).
    //   change — изменение, которое выбрал пользователь.
    //   model — модель дерева (обязательна для структурных изменений веток;
    //     для записей может быть null).
    // Возвращает AppliedOk либо NeedsDecision (локальная правка).
    // Вариант с decision применяет уже принятое пользователем решение
    // (после того как UI показал диалог конфликта).
    AppStatus applyChange(SubscriptionRegistry *registry,
                          RecordTableData *table,
                          const DiffChange &change,
                          QString *errorText=nullptr,
                          QString *localRecordIdOut=nullptr,
                          KnowTreeModel *model=nullptr);

    AppStatus applyChangeWithDecision(SubscriptionRegistry *registry,
                                      RecordTableData *table,
                                      const DiffChange &change,
                                      ConflictDecision decision,
                                      QString *errorText=nullptr,
                                      QString *localRecordIdOut=nullptr,
                                      KnowTreeModel *model=nullptr);

private:
    // Общие шаги применения отдельного типа изменения
    AppStatus applyAdd(SubscriptionRegistry *registry, RecordTableData *table,
                       const DiffChange &change, QString *errorText,
                       QString *localRecordIdOut);
    AppStatus applyUpdate(SubscriptionRegistry *registry, RecordTableData *table,
                          const DiffChange &change, ConflictDecision decision,
                          bool decisionExplicit, QString *errorText,
                          QString *localRecordIdOut);
    AppStatus applyDelete(SubscriptionRegistry *registry, RecordTableData *table,
                          const DiffChange &change, QString *errorText);

    // Структурные изменения веток (требуют model)
    AppStatus applyBranchAdd(SubscriptionRegistry *registry, KnowTreeModel *model,
                             const DiffChange &change, QString *errorText);
    AppStatus applyBranchRename(SubscriptionRegistry *registry, KnowTreeModel *model,
                                const DiffChange &change, QString *errorText);
    AppStatus applyBranchMove(SubscriptionRegistry *registry, KnowTreeModel *model,
                              const DiffChange &change, QString *errorText);
    AppStatus applyBranchDelete(SubscriptionRegistry *registry, KnowTreeModel *model,
                                const DiffChange &change, QString *errorText);

    // Создание записи из публикации (общее для импорта и обновлений)
    AppStatus doCreate(SubscriptionRegistry *registry, RecordTableData *table,
                       const QString &sharedRecordId, QString *errorText,
                       QString *localRecordIdOut);
    AppStatus doReplace(SubscriptionRegistry *registry, RecordTableData *table,
                        const QString &sharedRecordId, const QString &localRecordId,
                        QString *errorText, QString *localRecordIdOut);
    AppStatus doKeepBoth(SubscriptionRegistry *registry, RecordTableData *table,
                         const DiffChange &change, const QString &localRecordId,
                         QString *errorText, QString *localRecordIdOut);

    // Локальная ветка для shared-ветки: прямое отображение, иначе подъём
    // по родителям HEAD-фрагмента до ближайшей отображённой (корень — фолбэк
    // вызывающего кода). Пусто, если не отображено ничего (нет даже корня)
    QString localBranchForShared(SubscriptionRegistry *registry,
                                 const QString &sharedBranchId) const;

    // Сохранение связи sharedBranchId -> localBranchId
    void setBranchMapping(SubscriptionRegistry *registry, const QString &sharedBranchId,
                          const QString &localBranchId);

    // Удаление из карт всех связей поддерева локальной ветки
    // (вызывается ДО удаления ветки из модели, пока дерево живо)
    void purgeLocalSubtreeMappings(SubscriptionRegistry *registry,
                                   TreeItem *localSubtreeRoot);

    // Карта импортированных записей подписки
    QMap<QString, QPair<QString, QString>> getRecordsMap(
        SubscriptionRegistry *registry) const;
    void setRecordMapping(SubscriptionRegistry *registry,
                          const QString &sharedRecordId,
                          const QString &localRecordId,
                          const QString &snapshotHash);
    void removeRecordMapping(SubscriptionRegistry *registry,
                             const QString &sharedRecordId);

    // Запись применённого удаления ("record:<id>" / "branch:<id>")
    void appendAppliedDelete(SubscriptionRegistry *registry, const QString &key);

    // Поиск элемента <record> в кешированном branch.xml
    QDomElement findRecordElement(const QString &sharedRecordId) const;

    // Текст и каталог записи публикации
    QString readSharedRecordText(const DiffRecordData &recordData) const;
    QString sharedRecordDir(const DiffRecordData &recordData) const;

    // Копирование файлов записи из публикации, чистка устаревших,
    // запись текста локальной копии
    bool copyRecordFiles(const DiffRecordData &recordData,
                         const QString &localDirPath,
                         QString *errorText) const;
    void removeStaleFiles(const QString &localDirPath,
                          const DiffRecordData &recordData) const;
    AppStatus writeLocalRecordContent(RecordTableData *table, int pos,
                                      const DiffRecordData &recordData,
                                      QString *errorText);

    // Живой текст локальной записи из файла
    bool readLocalRecordText(RecordTableData *table, int pos,
                             QByteArray *contentOut) const;

    // Отражено ли изменение в локальной копии (для остаточного diff)
    bool isChangeReflected(SubscriptionRegistry *registry,
                           KnowTreeModel *model,
                           const DiffChange &change,
                           RecordTableData *fallbackTable);

    // Рекурсивный импорт ветки и её записей
    bool importBranchRecursive(SubscriptionRegistry *registry,
                               KnowTreeModel *model,
                               const QString &localParentId,
                               const QString &sharedBranchId,
                               const QString &localBranchName,
                               QString *errorText);

    QString ensureImportedContainer(KnowTreeModel *model, QString *errorText);
    QString ensureOwnerFolder(KnowTreeModel *model, const QString &importedRootId,
                              const QString &ownerName, QString *errorText);

    QString m_publicationDir;
    bool m_loaded=false;
    PublicationMeta m_meta;
    QString m_branchXmlContent;
    QDomDocument m_branchDoc;
    FragmentData m_headFragment;
    QMap<QString, QString> m_manifestFileShas;
    bool m_pubComplete=false;
    QString m_localTargetBranchId;
};

#endif // _SUBSCRIPTIONIMPORTENGINE_H_

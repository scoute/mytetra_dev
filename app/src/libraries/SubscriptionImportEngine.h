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
// Жизненный цикл (оркестратор — TreeScreen):
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
    // false — данные не доехали полностью (Syncthing) или неверифицируемы;
    // UI показывает «ожидание данных»
    bool isPublicationComplete(void) const;

    // Готовность отдельной записи к импорту: её файлы совпадают с манифестом
    // версии (защита от частичной доставки в момент применения)
    bool isRecordComplete(const QString &sharedRecordId) const;

    // Текущий HEAD репозитория публикации (пусто, если git недоступен
    // или журнал не найден). Для diff/импорта/детекта не обязателен.
    QString headCommit(void) const;

    // Манифест-снимок текущего состояния публикации (JSON): сериализованный
    // branch.xml + карта recordId -> sha256 текстов записей.
    // Сохраняется в подписке (baselineSnapshot) при подписке/просмотре/
    // переносе, когда базовая точка выравнивается к текущему HEAD.
    QString buildBaselineSnapshot(void) const;

    // Список изменений между baselineSnapshot подписчика и текущим HEAD
    // публикации. Пустой snapshot означает «всё новое».
    QList<DiffChange> changesSinceSnapshot(const QString &baselineSnapshot);

    // Остаточный diff: свежие изменения минус уже отражённые в локальной
    // копии минус применённые удаления. Именно этот список показывается
    // в диалоге и срезе: после полного импорта он пуст, после частичного —
    // только остаток. fallbackTable — корневая таблица копии (может быть null)
    QList<DiffChange> pendingChanges(SubscriptionRegistry *registry,
                                     KnowTreeModel *model,
                                     const QString &baselineSnapshot,
                                     RecordTableData *fallbackTable);

    // Таблица-приёмник изменения: где запись реально живёт (чинит наследие,
    // когда всё писалось в корневую таблицу) либо таблица своей ветки-копии.
    // fallbackTable — корневая таблица копии (может быть null)
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
    // ветки импортируются через doCreate с заполнением recordsMap.
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

    // Локальная ветка для shared-ветки: прямое отображение, иначе подъём
    // по родителям HEAD-фрагмента до ближайшей отображённой (корень — фолбэк).
    // Пусто, если не отображено ничего (нет даже корня)
    QString localBranchForShared(SubscriptionRegistry *registry,
                                 const QString &sharedBranchId) const;

    // Сохранение связи sharedBranchId -> localBranchId
    void setBranchMapping(SubscriptionRegistry *registry, const QString &sharedBranchId,
                          const QString &localBranchId);

    // Удаление из карт всех связей поддерева локальной ветки
    // (после удаления ветки у подписчика)
    void purgeLocalSubtreeMappings(SubscriptionRegistry *registry,
                                   TreeItem *localSubtreeRoot);

    // Создание новой локальной записи из shared-записи (новые id/dir)
    AppStatus doCreate(SubscriptionRegistry *registry, RecordTableData *table,
                       const QString &sharedRecordId, QString *errorText,
                       QString *localRecordIdOut);

    // Полная замена локальной записи версией владельца
    AppStatus doReplace(SubscriptionRegistry *registry, RecordTableData *table,
                        const QString &sharedRecordId, const QString &localRecordId,
                        QString *errorText, QString *localRecordIdOut);

    // Сохранить обе: локальная правка переименовывается (_conflict-<дата>),
    // импортируется новая копия
    AppStatus doKeepBoth(SubscriptionRegistry *registry, RecordTableData *table,
                         const DiffChange &change, const QString &localRecordId,
                         QString *errorText, QString *localRecordIdOut);

    // Поиск элемента <record> в кешированном branch.xml
    QDomElement findRecordElement(const QString &sharedRecordId) const;

    // Поиск/создание контейнера "imported" верхнего уровня (ветки с именем
    // imported). Возвращает id ветки-контейнера либо пустую строку.
    QString ensureImportedContainer(KnowTreeModel *model, QString *errorText);

    // Поиск/создание папки владельца внутри контейнера imported.
    // Возвращает id папки либо пустую строку.
    QString ensureOwnerFolder(KnowTreeModel *model,
                              const QString &importedRootId,
                              const QString &ownerName,
                              QString *errorText);

    // Создание локальной копии ветки публикации (ветка + записи + под-ветки).
    //   localParentId  — id локальной ветки-родителя
    //   sharedBranchId — id ветки в публикации (ключ в m_headFragment.branches)
    //   localBranchName— имя создаваемой локальной ветки
    bool importBranchRecursive(SubscriptionRegistry *registry,
                               KnowTreeModel *model,
                               const QString &localParentId,
                               const QString &sharedBranchId,
                               const QString &localBranchName,
                               QString *errorText);

    // Содержимое файла записи в каталоге публикации
    QString readSharedRecordText(const DiffRecordData &recordData) const;

    // Каталог shared-записи в каталоге публикации
    QString sharedRecordDir(const DiffRecordData &recordData) const;

    // Репозиторий журнала для текущей публикации:
    // устаревшая per-branch публикация — сам её каталог (с .git внутри);
    // новый макет — корень каталога обмена shared/.git.
    // Пустая строка, если репозиторий не найден
    QString repositoryRoot(void) const;

    // Копирование файлов записи (кроме текстового файла) из shared-каталога
    // в локальный каталог записи
    bool copyRecordFiles(const DiffRecordData &recordData,
                         const QString &localDirPath,
                         QString *errorText) const;

    // Перенос в корзину файлов локального каталога записи, которых нет
    // в наборе файлов публикации (кроме текстового файла записи)
    void removeStaleFiles(const QString &localDirPath,
                          const DiffRecordData &recordData) const;

    // Карта импортированных записей (sharedRecordId -> (localRecordId, snapshot))
    QMap<QString, QPair<QString, QString>> getRecordsMap(SubscriptionRegistry *registry) const;

    // Сохранение связи sharedRecordId -> localRecordId + snapshot
    void setRecordMapping(SubscriptionRegistry *registry, const QString &sharedRecordId,
                          const QString &localRecordId, const QString &snapshotHash);

    // Удаление связи sharedRecordId
    void removeRecordMapping(SubscriptionRegistry *registry, const QString &sharedRecordId);

    // Запись применённого удаления ("record:<id>" / "branch:<id>")
    void appendAppliedDelete(SubscriptionRegistry *registry, const QString &key);

    // Отражено ли изменение в локальной копии (для остаточного diff)
    bool isChangeReflected(SubscriptionRegistry *registry,
                           KnowTreeModel *model,
                           const DiffChange &change,
                           RecordTableData *fallbackTable);

    // Живой текст локальной записи напрямую из файла, без побочных эффектов
    // (в отличие от getRecordFat не показывает диалогов и ничего не создаёт).
    // false — сверить нельзя (нет файла, шифровано): изменение показывается
    bool readLocalRecordText(RecordTableData *table, int pos,
                             QByteArray *contentOut) const;

    // Запись в локальную БД: текст + файлы + привязка вложений shared-записи
    AppStatus writeLocalRecordContent(RecordTableData *table, int pos,
                                      const DiffRecordData &recordData,
                                      QString *errorText);

    QString m_publicationDir;
    QString m_branchXmlContent;
    QDomDocument m_branchDoc;
    FragmentData m_headFragment;
    PublicationMeta m_meta;
    QString m_localTargetBranchId;

    // Манифест публикации (путь -> sha256 hex) и готовность к диффу/импорту:
    // заполняется в loadPublication, флаг обнуляется при несовпадении записи
    QMap<QString, QString> m_manifestFileShas;
    bool m_pubComplete=false;

    bool m_loaded=false;
};

#endif // _SUBSCRIPTIONIMPORTENGINE_H_
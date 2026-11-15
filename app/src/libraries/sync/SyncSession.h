#ifndef SYNCSESSION_H
#define SYNCSESSION_H

#include <functional>

#include <QString>
#include <QStringList>

#include "SyncManifest.h"
#include "SyncPreview.h"

class KnowTreeModel;
class AppConfig;
class SyncClient;

// Одна сессия синхронизации со стороны спока (клиента).
// Порядок: hello -> манифесты -> сверка -> заливка -> скачивание ->
// удаление -> commit там -> commit тут. Ручной триггер, блокирующие
// вызовы, прогресс наружу через колбэк-лог (UI позже).
struct SyncReport
{
    bool ok=false;
    QString error;
    QString errorCode; // код handshake (foreign-network и т.п.), пусто для прочих
    int uploadedNotes=0;
    int downloadedNotes=0;
    int uploadedBranches=0;
    int downloadedBranches=0;
    int appliedDeletes=0;
    QStringList conflicts; // id с равными mtime при разных хешах

    // Поштучные несмертельные ошибки ("note <id>: <причина>").
    // Сессия идет дальше, а не падает на первой битой записи
    QStringList errors;
};

// Стратегия первого синка, когда данные есть с обеих сторон
// (пока ничего не слилось). Merge — тихое слияние как в MVP
enum class SyncStrategy
{
    Ask,         // спросить оператора (GUI); headless трактует как Merge
    Merge,       // слить как есть: оба дерева остаются
    MergeByName, // объединение по именам веток на хабе (id хаба
                 // побеждает, одноименные записи — дубли), затем
                 // перетирание клиента результатом с хаба
    PullFromHub, // стереть свое в корзину, взять с хаба целиком
    PushToHub,   // стереть на хабе (в корзину), залить свое целиком
    Cancel       // оператор отменил: сессия прерывается, флаг остается
};

class SyncSession
{
public:

    SyncSession(KnowTreeModel *model, AppConfig *config,
                const QString &tetradir);

    // Полный цикл с хабом host:port. isHub=false для спока.
    // Возвращает отчет; при ok==false смотреть error
    SyncReport run(const QString &host, quint16 port,
                   const QString &pairingPin=QString());

    // Решение стратегии первого синка (данные есть с обеих сторон).
    // GUI передает диалог, headless — значение --sync-strategy.
    // По умолчанию — Merge (тихое слияние, поведение MVP)
    using StrategyResolver =
        std::function<SyncStrategy(int localItems, int remoteItems)>;
    void setStrategyResolver(StrategyResolver resolver);

    // Решение по опасному синку. Принцип: безопасное (создание,
    // обновление) синкается молча, опасное (любые удаления) — только
    // с явного подтверждения пользователя
    enum class DeleteDecision
    {
        Proceed,  // подтверждено: удаляем
        Cancel,   // отмена: сессия прерывается
        Restore,  // восстановить с сервера: локальное в корзину,
                  // база перекачивается с хаба целиком
    };
    using DeleteGuard =
        std::function<DeleteDecision(int remoteDeletes, int localDeletes)>;
    void setDeleteGuard(DeleteGuard guard);

    // Окно предпросмотра (GUI): план по категориям с действиями
    // (создание молча, остальное — явный выбор). Возвращает false —
    // пользователь отменил. Без него (headless) действует DeleteGuard
    using PreviewResolver = std::function<bool(const SyncPreview::Preview &,
                                               SyncPreview::Decisions &)>;
    void setPreviewResolver(PreviewResolver resolver);

    // Текстовый лог шагов (для UI/отчета), наращивается по ходу run()
    QStringList log(void) const;

private:

    // Залить на хаб по плану. Возвращает false при первой ошибке сети
    bool pushByPlan(SyncClient &client, const QString &host, quint16 port,
                    const SyncPlan &plan, const SyncManifest &local,
                    SyncReport &report);

    // Скачать с хаба по плану и применить локально
    bool pullByPlan(SyncClient &client, const QString &host, quint16 port,
                    const SyncPlan &plan,
                    const SyncManifest &remote,
                    SyncReport &report);

    // Стратегия "объединение по именам веток на хабе": заливка своего ->
    // слияние на хабе (id хаба побеждает) -> commit -> перетирание
    // клиента результатом с хаба
    SyncReport mergeByNameSession(const QString &host, quint16 port,
                                  const SyncManifest &local,
                                  const SyncManifest &remote,
                                  SyncClient &client);

    // Применить чужие tombstone'ы локально
    bool applyRemoteDeletes(const SyncPlan &plan,
                            const SyncManifest &remote,
                            SyncReport &report);

    // Свои tombstone'ы, которых нет у хаба — отправить ему
    bool pushOwnTombstones(SyncClient &client, const QString &host,
                           quint16 port,
                           const SyncManifest &local,
                           const SyncManifest &remote,
                           const SyncPlan &plan,
                           SyncReport &report);

    // Восстановление с хаба: локальное в корзину, полная загрузка
    SyncReport restoreFromHub(const QString &host, quint16 port,
                              SyncClient &client);

    // Локальная поштучная ошибка (сериализация/применение у себя):
    // в report.errors + лог, сессия идет дальше
    void skipItem(SyncReport &report, const QString &entry);

    // Сетевая поштучная ошибка: транспорт умер или not-paired —
    // фатально (report.error, вернуть false); иначе в report.errors
    // и идем дальше (вернуть true)
    bool noteItem(SyncClient &client, SyncReport &report,
                  const QString &kind, const QString &id,
                  const QString &reason);

    KnowTreeModel *model;
    AppConfig *config;
    QString tetradir;
    QStringList logLines;
    StrategyResolver strategyResolver;
    DeleteGuard deleteGuard;
    PreviewResolver previewResolver;

    void addLog(const QString &line);
};

#endif // SYNCSESSION_H

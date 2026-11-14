#ifndef SYNCSESSION_H
#define SYNCSESSION_H

#include <QString>
#include <QStringList>

#include "SyncManifest.h"

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

    // Применить чужие tombstone'ы локально
    bool applyRemoteDeletes(const SyncPlan &plan,
                            const SyncManifest &remote,
                            SyncReport &report);

    // Свои tombstone'ы, которых нет у хаба — отправить ему
    bool pushOwnTombstones(SyncClient &client, const QString &host,
                           quint16 port,
                           const SyncManifest &local,
                           const SyncManifest &remote,
                           SyncReport &report);

    KnowTreeModel *model;
    AppConfig *config;
    QString tetradir;
    QStringList logLines;

    void addLog(const QString &line);
};

#endif // SYNCSESSION_H

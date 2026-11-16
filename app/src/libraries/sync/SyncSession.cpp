#include "SyncSession.h"
#include "SyncClient.h"
#include "SyncNoteIo.h"
#include "SyncSidecar.h"
#include "SyncIdentity.h"
#include "SyncDebug.h"
#include "models/tree/KnowTreeModel.h"
#include "models/appConfig/AppConfig.h"

#include <QDebug>
#include <QMap>


SyncSession::SyncSession(KnowTreeModel *model, AppConfig *config,
                         const QString &tetradir)
    : model(model),
      config(config),
      tetradir(tetradir)
{    SYNC_TRACE_EXTRA(QStringLiteral("model=%1").arg(syncPtr(model)));

}


QStringList SyncSession::log(void) const
{
    return logLines;
}


void SyncSession::setStrategyResolver(StrategyResolver resolver)
{
    strategyResolver=resolver;
}


void SyncSession::setDeleteGuard(DeleteGuard guard)
{
    deleteGuard=guard;
}


void SyncSession::setPreviewResolver(PreviewResolver resolver)
{
    previewResolver=resolver;
}


namespace
{
// Сколько элементов (ветки со своими записями и поддеревьями) исчезнет
// вместе с id: удаление ветки тащит все поддерево
int subtreeItemCount(const QString &id,
                     const QMap<QString, SyncBranchInfo> &branches,
                     const QMap<QString, SyncNoteInfo> &notes)
{    SYNC_TRACE();

    QMap<QString, QStringList> childBranches;
    QMap<QString, int> noteCounts;

    for(auto it=branches.constBegin(); it!=branches.constEnd(); ++it)
        childBranches[it.value().parentId] << it.key();

    for(auto it=notes.constBegin(); it!=notes.constEnd(); ++it)
        noteCounts[it.value().branchId]++;

    int count=0;
    QStringList queue;
    queue << id;
    QSet<QString> seen;

    while(!queue.isEmpty())
    {
        const QString cur=queue.takeFirst();

        if(cur.isEmpty() || seen.contains(cur))
            continue;

        seen.insert(cur);

        if(notes.contains(cur))
        {
            count++;
            continue;
        }

        if(!branches.contains(cur))
            continue;

        count+=1+noteCounts.value(cur);
        queue << childBranches.value(cur);
    }

    return count;
}
}


void SyncSession::skipItem(SyncReport &report, const QString &entry)
{    SYNC_TRACE();

    report.errors << entry;
    addLog(QStringLiteral("item error, skipping: %1").arg(entry));
    qDebug() << "SyncSession: item error, skipping:" << entry;
}


bool SyncSession::noteItem(SyncClient &client, SyncReport &report,
                           const QString &kind, const QString &id,
                           const QString &reason)
{    SYNC_TRACE();

    // Транспорт умер (не дошло), гейт пары (not-paired) или отказ
    // proof (auth-failed) — это не "битая запись", сессию рвем сразу
    if(!client.lastTransportOk()
       || reason==QStringLiteral("not-paired")
       || reason==QStringLiteral("auth-failed"))
    {
        report.error=reason.isEmpty()
            ? QStringLiteral("%1 %2 failed").arg(kind, id) : reason;
        if(reason==QStringLiteral("not-paired")
           || reason==QStringLiteral("auth-failed"))
            report.errorCode=reason;
        return false;
    }

    skipItem(report, QStringLiteral("%1 %2: %3").arg(kind, id, reason));
    return true;
}


void SyncSession::addLog(const QString &line)
{    SYNC_TRACE();

    logLines << line;

    // Каждый шаг сессии в консоль и журнал синка (включается настройкой
    // "Отладка синка" или MYTETRA_SYNC_DEBUG=1): зависший клиент молчит
    // и непонятно где встал, а журнал можно просто прислать
    syncDbg("session: "+line);
}


SyncReport SyncSession::run(const QString &host, quint16 port,
                              const QString &pairingPin)
{    SYNC_TRACE_EXTRA(QStringLiteral("host=%1 model=%2").arg(host, syncPtr(model)));

    SyncReport report;
    SyncClient client(config);

    // Снапшот пары на входе: данные разрешены только если пара
    // существовала ДО сессии (гейт 089). Сессия, создавшая/починившая
    // пару по PIN, — "попытка установить pair": останавливаемся
    // до манифестов, данные пойдут следующим запуском
    QMap<QString, QString> ringBefore=config->get_syncSecretsRing();
    const QString groupBefore=config->get_syncGroupId();

    // 1. Рукопожатие: версии, сеть, часы, подлинность
    addLog(QStringLiteral("hello %1:%2").arg(host).arg(port));

    SyncHelloResult hello=client.hello(host, port);

    // Починка пары: секрет есть, но proof чужой (сервер сбросили или PIN
    // раньше был другой). Раз PIN предъявлен — перевыводим secret поверх
    // старого и повторяем hello. Сервер с тем же PIN сойдется к тому же
    // secret (деривация симметрична), с чужим PIN — снова откажет внятно.
    // Без PIN: строгий отказ уходит как есть, чинится через --sync-forget
    if(!hello.ok && hello.errorCode==QStringLiteral("auth-failed")
       && !pairingPin.isEmpty()
       && !hello.remoteDeviceId.isEmpty()
       && hello.remoteDeviceId!=config->get_syncDeviceId())
    {
        if(SyncIdentity::adoptPairing(config, hello.remoteDeviceId,
                                      pairingPin))
        {
            addLog(QStringLiteral("secret mismatch, re-paired via PIN, retry"));
            hello=client.hello(host, port);
        }
    }

    if(!hello.ok)
    {
        report.error=hello.errorText.isEmpty() ? hello.errorCode
                                              : hello.errorText;
        report.errorCode=hello.errorCode;
        addLog(QStringLiteral("hello failed: %1").arg(report.error));
        return report;
    }

    addLog(QStringLiteral("hello ok, skew %1 ms").arg(hello.skewMs));

    // Сервер что-то простил (re-pair): сходимся к тому же secret по нашему
    // PIN, иначе в следующий раз чиниться будет снова. Без PIN — просто
    // показываем предупреждение, секрет не трогаем
    if(!hello.warning.isEmpty())
    {
        addLog(QStringLiteral("server warning: %1").arg(hello.warning));

        if(!pairingPin.isEmpty())
            SyncIdentity::adoptPairing(config, hello.remoteDeviceId,
                                       pairingPin);
    }

    // Спаривание: секрета нет, PIN задан — выводим тот же secret что сервер.
    // GUI-вариант с проверкой фингерпринта — отдельно, до вызова run
    if(config->get_syncPairingSecret().isEmpty() && !pairingPin.isEmpty()
       && !hello.remoteDeviceId.isEmpty())
    {
        if(!SyncIdentity::adoptPairing(config, hello.remoteDeviceId,
                                       pairingPin))
        {
            report.error=QStringLiteral("empty PIN is not allowed");
            return report;
        }

        addLog(QStringLiteral("paired via PIN"));
    }

    // Группа: пустая у нас — принимаем удаленную или детерминированную общую.
    // Чужая непустая при общем PIN — переезд между сетями: хаб источник
    // правды, берем его группу с логом (сервер нас уже принял с warning).
    // Без PIN чужую группу не трогаем: строгий отказ уходит как есть
    if(!hello.remoteGroupId.isEmpty()
       && hello.remoteGroupId!=config->get_syncGroupId())
    {
        if(config->get_syncGroupId().isEmpty() || !pairingPin.isEmpty())
        {
            config->set_syncGroupId(hello.remoteGroupId);
            addLog(QStringLiteral("adopted hub group %1")
                   .arg(hello.remoteGroupId));
        }
    }
    else if(config->get_syncGroupId().isEmpty()
            && !hello.remoteDeviceId.isEmpty())
    {
        config->set_syncGroupId(
            SyncIdentity::pairGroupId(config->get_syncDeviceId(),
                                      hello.remoteDeviceId));
    }

    // Гейт пары: без секрета данных нет вообще (только что сброшенная
    // или девственная пара без PIN — дальше не идем, нужен pair по PIN)
    if(!config->hasSyncPairing())
    {
        report.error=QStringLiteral("not paired: pair with the same PIN first");
        report.errorCode=QStringLiteral("not-paired");
        addLog(QStringLiteral("refusing data: not paired"));
        return report;
    }

    // Секрет именно под этот пир (кольцо): hello уже выбрал рабочий,
    // после починок пары перевыставляем явно
    client.setPeerSecret(
        SyncIdentity::secretForPeer(config, hello.remoteDeviceId));

    // Пара создана/пересоздана этой сессией — фиксируем и выходим:
    // это была установка пары, данные следующим запуском
    if(config->get_syncSecretsRing().value(hello.remoteDeviceId)
           !=ringBefore.value(hello.remoteDeviceId)
       || config->get_syncGroupId()!=groupBefore)
    {
        report.error=QStringLiteral("Pair established with %1, "
                                    "run sync again to transfer data")
            .arg(hello.remoteDeviceName.isEmpty()
                 ? hello.remoteDeviceId : hello.remoteDeviceName);
        report.errorCode=QStringLiteral("pair-established");
        addLog(QStringLiteral("pair established, data deferred to next run"));
        // первый следующий синк спросит стратегию (данные есть
        // с обеих сторон — тихий мерж удивлял)
        config->set_syncAskStrategy(true);
        return report;
    }

    // 2. Манифесты: чужой тянем, свой строим локально
    SyncManifest remote=client.fetchManifest(host, port);

    if(!client.lastError().isEmpty())
    {
        report.error=client.lastError();
        addLog(QStringLiteral("manifest failed: %1").arg(report.error));
        return report;
    }

    SyncManifest local=
        SyncManifestBuilder::build(model, SyncSidecar::instance(),
                                   tetradir);

    // Явный деструктивный выбор стратегии уже является подтверждением
    bool destructiveStrategy=false;

    // 2а. Первый синк после спаривания, когда данные есть с обеих
    // сторон: спросить стратегию (тихий мерж удивлял). Флаг снимается
    // после ответа; отмена оставляет флаг и прерывает сессию
    if(config->get_syncAskStrategy())
    {
        // Спрашиваем, когда есть что-то ХОТЬ С ОДНОЙ стороны (ветки
        // тоже считаются): стратегии осмыслены и для одностороннего
        // содержимого. Совсем пусто — флаг не гасим, спросим при
        // первом синке с данными
        const bool localAny=!local.notes.isEmpty() || !local.branches.isEmpty();
        const bool remoteAny=!remote.notes.isEmpty() || !remote.branches.isEmpty();

        if(localAny || remoteAny)
        {
            SyncStrategy strategy=SyncStrategy::Merge;

            if(strategyResolver)
                strategy=strategyResolver(
                    local.notes.size()+local.branches.size(),
                    remote.notes.size()+remote.branches.size());

            if(strategy==SyncStrategy::Cancel)
            {
                addLog(QStringLiteral("strategy: cancelled, will ask again"));
                report.error=QStringLiteral("Sync cancelled");
                return report;
            }

            config->set_syncAskStrategy(false);

            if(strategy==SyncStrategy::MergeByName)
            {
                addLog(QStringLiteral("strategy: merge by branch names "
                                      "on hub (server ids win)"));
                return mergeByNameSession(host, port, local, remote, client);
            }

            if(strategy==SyncStrategy::PullFromHub)
            {
                destructiveStrategy=true;
                addLog(QStringLiteral("strategy: take from hub, local -> trash"));
                const QString error=SyncNoteIo::wipeAll(model);

                if(!error.isEmpty())
                {
                    report.error=error;
                    return report;
                }

                // локальное состояние стерто: план = полная загрузка
                local=SyncManifest();
            }
            else if(strategy==SyncStrategy::PushToHub)
            {
                destructiveStrategy=true;
                addLog(QStringLiteral("strategy: replace hub content with ours"));

                if(!client.reset(host, port))
                {
                    report.error=client.lastError().isEmpty()
                        ? QStringLiteral("hub reset failed") : client.lastError();
                    return report;
                }

                // хаб пуст: план = полная заливка
                remote=SyncManifest();
            }
            else
                addLog(QStringLiteral("strategy: merge as-is"));
        }
        else
        {
            // выбирать нечего (одна из сторон пуста) — флаг гасим,
            // чтобы не спросить позже без причины
            config->set_syncAskStrategy(false);
        }
    }

    // 3. Сверка. Мы спок: равные mtime решает хаб (той же детерминированной
    // логикой: хаб бы скачал, мы — заливаем; итог одинаковый с обеих сторон)
    const SyncPlan plan=SyncManifestBuilder::compare(local, remote, false);

    addLog(QStringLiteral("manifest local notes=%1 branches=%2, "
                          "remote notes=%3 branches=%4")
           .arg(local.notes.size()).arg(local.branches.size())
           .arg(remote.notes.size()).arg(remote.branches.size()));

    addLog(QStringLiteral("plan upNotes=%1 downNotes=%2 upBranches=%3 "
                          "downBranches=%4 deleteLocal=%5 conflicts=%6")
           .arg(plan.uploadNotes.size()).arg(plan.downloadNotes.size())
           .arg(plan.uploadBranches.size()).arg(plan.downloadBranches.size())
           .arg(plan.deleteLocal.size()).arg(plan.conflicts.size()));

    if(plan.empty())
    {
        addLog(QStringLiteral("already in sync"));
        report.ok=true;
        return report;
    }

    // 3а. Опасный синк — только с явного подтверждения. Принцип:
    // безопасное (создание/обновление) едет молча, любые удаления
    // подсвечиваются пользователю (история: случайное удаление базы
    // на клиенте молча обнулило сервер)
    int remoteDeletes=0;

    for(const QString &id : plan.pushTombs)
        remoteDeletes+=subtreeItemCount(id, remote.branches, remote.notes);

    int localDeletes=0;

    for(const QString &id : plan.deleteLocal)
        localDeletes+=subtreeItemCount(id, local.branches, local.notes);

    if(remoteDeletes>0 || localDeletes>0)
        addLog(QStringLiteral("delete impact: %1 item(s) on hub, %2 locally")
               .arg(remoteDeletes).arg(localDeletes));

    // 3б. Предпросмотр (GUI): категории с действиями — безопасное
    // (создание) молча, остальное явным выбором. Заменяет собой
    // и guard удалений: удаления решаются в том же окне
    SyncPlan finalPlan=plan;

    if(previewResolver && !destructiveStrategy)
    {
        const SyncPreview::Preview preview=
            SyncPreview::classify(plan, local, remote);

        if(preview.needsAttention())
        {
            SyncPreview::Decisions decisions=
                SyncPreview::Decisions::defaults();

            if(!previewResolver(preview, decisions))
            {
                addLog(QStringLiteral("preview cancelled"));
                report.error=QStringLiteral("Sync cancelled");
                return report;
            }

            finalPlan=SyncPreview::applyDecisions(plan, preview, decisions,
                                                  local, remote);
            addLog(QStringLiteral("preview: final plan upNotes=%1 "
                                  "downNotes=%2 upBranches=%3 "
                                  "downBranches=%4 deleteLocal=%5 pushTombs=%6")
                   .arg(finalPlan.uploadNotes.size())
                   .arg(finalPlan.downloadNotes.size())
                   .arg(finalPlan.uploadBranches.size())
                   .arg(finalPlan.downloadBranches.size())
                   .arg(finalPlan.deleteLocal.size())
                   .arg(finalPlan.pushTombs.size()));
        }
    }
    else if(remoteDeletes>0 || localDeletes>0)
    {
        // Headless: диалогов нет, удаления — только с явного флага
        if(deleteGuard && !destructiveStrategy)
        {
            const DeleteDecision decision=
                deleteGuard(remoteDeletes, localDeletes);

            if(decision==DeleteDecision::Cancel)
            {
                addLog(QStringLiteral("delete sync cancelled"));
                report.error=QStringLiteral(
                    "Sync cancelled: it would delete data "
                    "(in headless mode use --sync-allow-delete)");
                return report;
            }

            if(decision==DeleteDecision::Restore)
            {
                addLog(QStringLiteral("restore from hub instead of delete"));
                return restoreFromHub(host, port, client);
            }
        }
        else if(!destructiveStrategy)
            addLog(QStringLiteral("warning: deletions proceed without "
                                  "confirmation (no guard)"));
    }

    // 4. Заливка нашего на хаб
    syncDbg(QStringLiteral("session: phase PUSH model=%1 tid=%2")
            .arg(syncPtr(model), syncTid()));
    if(!pushByPlan(client, host, port, finalPlan, local, report))
        return report;

    // 5. Скачивание с хаба и применение у себя
    syncDbg(QStringLiteral("session: phase PULL model=%1")
            .arg(syncPtr(model)));
    if(!pullByPlan(client, host, port, finalPlan, remote, report))
        return report;

    // 6. Чужие tombstone'ы — применить локально
    if(!applyRemoteDeletes(plan, remote, report))
        return report;

    // 7. Свои tombstone'ы, которых нет у хаба — отправить ему
    if(!pushOwnTombstones(client, host, port, local, remote, plan, report))
        return report;

    // 8. Фиксация: сначала хаб (сейв+перезагрузка), потом мы
    syncDbg(QStringLiteral("session: phase COMMIT model=%1")
            .arg(syncPtr(model)));
    addLog(QStringLiteral("commit hub"));

    if(!client.commit(host, port))
    {
        report.error=client.lastError().isEmpty()
                     ? QStringLiteral("hub commit failed")
                     : client.lastError();
        addLog(QStringLiteral("commit failed: %1").arg(report.error));
        return report;
    }

    syncDbg(QStringLiteral("session: BEFORE local save model=%1 tid=%2")
            .arg(syncPtr(model), syncTid()));
    model->save();
    syncDbg(QStringLiteral("session: AFTER local save, BEFORE reload model=%1")
            .arg(syncPtr(model)));
    model->reload();
    syncDbg(QStringLiteral("session: AFTER local reload model=%1 root=%2")
            .arg(syncPtr(model), syncPtr(model->getRootItemPublic())));
    addLog(QStringLiteral("commit local"));

    // 9. Конфликты — в отчет, тихо не теряем ничего:
    // проигравшие версии уже лежат в бэкапах/корзине по ходу apply
    report.conflicts=plan.conflicts;

    if(!report.conflicts.isEmpty())
        addLog(QStringLiteral("conflicts: %1").arg(report.conflicts.join(QStringLiteral(", "))));

    report.ok=true;
    return report;
}


bool SyncSession::pushByPlan(SyncClient &client,
                             const QString &host, quint16 port,
                             const SyncPlan &plan,
                             const SyncManifest &local,
                             SyncReport &report)
{    SYNC_TRACE();

    // Ветки раньше заметок: заметка ссылается на ветку
    for(const QString &id : plan.uploadBranches)
    {
        QJsonObject branch=SyncNoteIo::serializeBranch(model, id);

        if(branch.contains(QStringLiteral("error")))
        {
            skipItem(report, QStringLiteral("branch %1: %2")
                     .arg(id, branch.value(QStringLiteral("error"))
                     .toString()));
            continue;
        }

        // mtime/hash для adopt на хабе — из локального манифеста
        const SyncBranchInfo &info=local.branches.value(id);
        branch[QStringLiteral("mtime")]=info.mtime;
        branch[QStringLiteral("hash")]=info.hash;

        addLog(QStringLiteral("push branch %1").arg(id));

        if(!client.pushBranch(host, port, branch))
        {
            if(!noteItem(client, report, QStringLiteral("branch"), id,
                         client.lastError()))
                return false;

            continue;
        }

        report.uploadedBranches++;
    }

    for(const QString &id : plan.uploadNotes)
    {
        QJsonObject note=SyncNoteIo::serializeNote(model, tetradir, id);

        if(note.contains(QStringLiteral("error")))
        {
            skipItem(report, QStringLiteral("note %1: %2")
                     .arg(id, note.value(QStringLiteral("error")).toString()));
            continue;
        }

        const SyncNoteInfo &info=local.notes.value(id);
        note[QStringLiteral("mtime")]=info.mtime;
        note[QStringLiteral("hash")]=info.hash;
        note[QStringLiteral("branch")]=info.branchId;

        addLog(QStringLiteral("push note %1").arg(id));

        if(!client.pushNote(host, port, note))
        {
            if(!noteItem(client, report, QStringLiteral("note"), id,
                         client.lastError()))
                return false;

            continue;
        }

        report.uploadedNotes++;
    }

    return true;
}


bool SyncSession::pullByPlan(SyncClient &client,
                             const QString &host, quint16 port,
                             const SyncPlan &plan,
                             const SyncManifest &remote,
                             SyncReport &report)
{    SYNC_TRACE();

    for(const QString &id : plan.downloadBranches)
    {
        addLog(QStringLiteral("pull branch %1").arg(id));

        const QJsonObject branch=client.pullBranch(host, port, id);

        if(branch.contains(QStringLiteral("error")) ||
           !client.lastError().isEmpty())
        {
            if(!noteItem(client, report, QStringLiteral("branch"), id,
                         client.lastError().isEmpty()
                         ? QStringLiteral("pull branch failed")
                         : client.lastError()))
                return false;

            continue;
        }

        QJsonObject full=branch;
        full[QStringLiteral("id")]=id;

        const SyncBranchInfo &info=remote.branches.value(id);
        full[QStringLiteral("mtime")]=info.mtime;
        full[QStringLiteral("hash")]=info.hash;

        const QString error=SyncNoteIo::applyBranch(model, full);

        if(!error.isEmpty())
        {
            skipItem(report, QStringLiteral("branch %1: %2").arg(id, error));
            continue;
        }

        report.downloadedBranches++;
    }

    for(const QString &id : plan.downloadNotes)
    {
        addLog(QStringLiteral("pull note %1").arg(id));

        const QJsonObject note=client.pullNote(host, port, id);

        if(note.contains(QStringLiteral("error")) ||
           !client.lastError().isEmpty())
        {
            if(!noteItem(client, report, QStringLiteral("note"), id,
                         client.lastError().isEmpty()
                         ? QStringLiteral("pull note failed")
                         : client.lastError()))
                return false;

            continue;
        }

        QJsonObject full=note;
        full[QStringLiteral("id")]=id;

        const SyncNoteInfo &info=remote.notes.value(id);
        full[QStringLiteral("branch")]=info.branchId;
        full[QStringLiteral("mtime")]=info.mtime;
        full[QStringLiteral("hash")]=info.hash;

        const QString error=SyncNoteIo::applyNote(model, tetradir, full);

        if(!error.isEmpty())
        {
            skipItem(report, QStringLiteral("note %1: %2").arg(id, error));
            continue;
        }

        report.downloadedNotes++;
    }

    return true;
}


bool SyncSession::applyRemoteDeletes(const SyncPlan &plan,
                                     const SyncManifest &remote,
                                     SyncReport &report)
{    SYNC_TRACE();

    for(const QString &id : plan.deleteLocal)
    {
        const qint64 deletedAt=remote.tombstones.value(id, 0);

        addLog(QStringLiteral("delete local %1").arg(id));

        const QString error=SyncNoteIo::applyDelete(model, id, deletedAt);

        if(!error.isEmpty())
        {
            skipItem(report, QStringLiteral("delete %1: %2")
                     .arg(id, error));
            continue;
        }

        report.appliedDeletes++;
    }

    return true;
}


bool SyncSession::pushOwnTombstones(SyncClient &client,
                                    const QString &host, quint16 port,
                                    const SyncManifest &local,
                                    const SyncManifest &remote,
                                    const SyncPlan &plan,
                                    SyncReport &report)
{    SYNC_TRACE();

    // Шлем только tombstone'ы из плана: которых нет у хаба и которые
    // не проиграны их более новой правке (там уже стоит download)
    for(const QString &id : plan.pushTombs)
    {
        const qint64 deletedAt=local.tombstones.value(id, 0);

        // Перепроверка edit-wins: их правка новее — tomb не пушим
        if(remote.notes.contains(id)
           && remote.notes.value(id).mtime>deletedAt)
            continue;

        if(remote.branches.contains(id)
           && remote.branches.value(id).mtime>deletedAt)
            continue;

        addLog(QStringLiteral("push delete %1").arg(id));

        if(!client.pushDelete(host, port, id, deletedAt))
        {
            if(!noteItem(client, report, QStringLiteral("delete"),
                         id, client.lastError().isEmpty()
                         ? QStringLiteral("push delete failed")
                         : client.lastError()))
                return false;

            continue;
        }
    }

    return true;
}

// Стратегия "объединение по именам веток на хабе" (114):
// 1) заливаем свое (недостающее создается, разное решает LWW);
// 2) хаб сливает НАШИ ветки в одноименные свои (id хаба побеждает,
//    записи не трогаются — одноименные остаются дублями);
// 3) commit хаба;
// 4) перетирание клиента результатом (как стратегия "взять с сервера").
// При любой ошибке на шагах 1-3 сессия прерывается БЕЗ перетирания:
// терять локальное можно только когда результат слияния уже готов
SyncReport SyncSession::mergeByNameSession(const QString &host, quint16 port,
                                           const SyncManifest &local,
                                           const SyncManifest &remote,
                                           SyncClient &client)
{    SYNC_TRACE_EXTRA(QStringLiteral("model=%1").arg(syncPtr(model)));

    SyncReport report;

    // 1. Заливка своего
    const SyncPlan prePlan=SyncManifestBuilder::compare(local, remote, false);
    addLog(QStringLiteral("merge-by-name: push own (upNotes=%1 upBranches=%2)")
           .arg(prePlan.uploadNotes.size()).arg(prePlan.uploadBranches.size()));

    if(!pushByPlan(client, host, port, prePlan, local, report))
        return report;

    if(!report.errors.isEmpty())
    {
        // сливать можно только полное дерево: потерянная ветка исчезла бы
        // безвозвратно при перетирании клиента
        report.error=QStringLiteral("merge by name aborted: "
                                    "item errors on upload");
        addLog(report.error);
        return report;
    }

    // 2. Наши ветки = локальные минус бывшие у хаба до заливки
    QStringList clientOnly;

    for(auto it=local.branches.constBegin();
        it!=local.branches.constEnd(); ++it)
        if(!remote.branches.contains(it.key()))
            clientOnly << it.key();

    addLog(QStringLiteral("merge-by-name: %1 own branches to merge")
           .arg(clientOnly.size()));

    if(!client.mergeByName(host, port, clientOnly))
    {
        report.error=client.lastError().isEmpty()
            ? QStringLiteral("merge by name failed") : client.lastError();
        addLog(report.error);
        return report;
    }

    // 3. Фиксация слияния на хабе
    if(!client.commit(host, port))
    {
        report.error=client.lastError().isEmpty()
            ? QStringLiteral("hub commit failed") : client.lastError();
        addLog(report.error);
        return report;
    }

    // 4. Перетирание клиента результатом слияния
    syncDbg(QStringLiteral("session: merge-by-name BEFORE wipe model=%1")
            .arg(syncPtr(model)));
    addLog(QStringLiteral("merge-by-name: wipe local, pull merged result"));

    const QString error=SyncNoteIo::wipeAll(model);

    if(!error.isEmpty())
    {
        report.error=error;
        addLog(report.error);
        return report;
    }

    const SyncManifest freshRemote=client.fetchManifest(host, port);

    if(!client.lastError().isEmpty())
    {
        report.error=client.lastError();
        addLog(report.error);
        return report;
    }

    SyncManifest emptyLocal;
    const SyncPlan fullPlan=
        SyncManifestBuilder::compare(emptyLocal, freshRemote, false);

    if(!pullByPlan(client, host, port, fullPlan, freshRemote, report))
        return report;

    if(!client.commit(host, port))
    {
        report.error=client.lastError().isEmpty()
            ? QStringLiteral("hub commit failed") : client.lastError();
        addLog(report.error);
        return report;
    }

    syncDbg(QStringLiteral("session: merge-by-name BEFORE save model=%1")
            .arg(syncPtr(model)));
    model->save();
    syncDbg(QStringLiteral("session: merge-by-name AFTER save, BEFORE reload"));
    model->reload();
    syncDbg(QStringLiteral("session: merge-by-name AFTER reload root=%1")
            .arg(syncPtr(model->getRootItemPublic())));
    addLog(QStringLiteral("merge-by-name: done"));

    report.ok=true;
    return report;
}

// Восстановление с хаба: локальное уходит в корзину, база
// перекачивается с хаба целиком. Ответ на "случайно удалил, хочу
// восстановиться" прямо из диалога подтверждения удаления
SyncReport SyncSession::restoreFromHub(const QString &host, quint16 port,
                                      SyncClient &client)
{    SYNC_TRACE_EXTRA(QStringLiteral("model=%1").arg(syncPtr(model)));

    SyncReport report;
    addLog(QStringLiteral("restore: local -> trash, pull all from hub"));

    const QString error=SyncNoteIo::wipeAll(model);

    if(!error.isEmpty())
    {
        report.error=error;
        addLog(report.error);
        return report;
    }

    const SyncManifest freshRemote=client.fetchManifest(host, port);

    if(!client.lastError().isEmpty())
    {
        report.error=client.lastError();
        addLog(report.error);
        return report;
    }

    SyncManifest emptyLocal;
    const SyncPlan fullPlan=
        SyncManifestBuilder::compare(emptyLocal, freshRemote, false);

    if(!pullByPlan(client, host, port, fullPlan, freshRemote, report))
        return report;

    if(!client.commit(host, port))
    {
        report.error=client.lastError().isEmpty()
            ? QStringLiteral("hub commit failed") : client.lastError();
        addLog(report.error);
        return report;
    }

    syncDbg(QStringLiteral("session: restore BEFORE save model=%1")
            .arg(syncPtr(model)));
    model->save();
    syncDbg(QStringLiteral("session: restore AFTER save, BEFORE reload"));
    model->reload();
    syncDbg(QStringLiteral("session: restore AFTER reload root=%1")
            .arg(syncPtr(model->getRootItemPublic())));
    addLog(QStringLiteral("restore from hub: done"));

    report.ok=true;
    return report;
}

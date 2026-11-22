#include "SyncSession.h"
#include "SyncClient.h"
#include "SyncNoteIo.h"
#include "SyncSidecar.h"
#include "SyncScope.h"
#include "SyncCounts.h"
#include "SyncIdentity.h"
#include "SyncPairFlow.h"
#include "SyncPairCrypto.h"
#include "SyncDebug.h"
#include "models/tree/KnowTreeModel.h"
#include "models/appConfig/AppConfig.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QMap>
#include <QSet>


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


void SyncSession::setProgressCallback(ProgressCallback callback)
{
    progressCallback=callback;
}


void SyncSession::setTlsTrustResolver(TlsTrustResolver resolver)
{
    tlsTrustResolver=resolver;
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
                              const QString &pairingPin,
                              const QString &scopeRootId)
{    SYNC_TRACE_EXTRA(QStringLiteral("host=%1 model=%2").arg(host, syncPtr(model)));

    SyncReport report;
    SyncClient client(config);
    client.setTlsMode(static_cast<SyncClient::TlsMode>(
        config->get_syncTlsClientMode()));
    client.setTlsPin(config->get_syncTlsPin(host, port));

    if(tlsTrustResolver)
        client.setTlsTrustCallback(tlsTrustResolver);

    // Scope поддерева: корень псевдо-элемента "0" — тот же весь синк
    QString scope=scopeRootId;

    if(scope==QStringLiteral("0"))
        scope.clear();

    if(!scope.isEmpty())
        addLog(QStringLiteral("scope branch %1").arg(scope));

    // Снапшот пары на входе: данные разрешены только если пара
    // существовала ДО сессии (гейт 089). Сессия, создавшая/починившая
    // пару по коду/PIN, — "попытка установить pair": останавливаемся
    // до манифестов, данные пойдут следующим запуском
    QMap<QString, QString> ringBefore=config->get_syncSecretsRing();
    const QString groupBefore=config->get_syncGroupId();

    // 1. Рукопожатие: версии, сеть, часы, подлинность
    addLog(QStringLiteral("hello %1:%2").arg(host).arg(port));

    SyncHelloResult hello=client.hello(host, port);

    // Код генерируется на сервере по pairRequest; GUI-пути уже сделали
    // этот запрос до диалога, но сессию могут позвать и напрямую
    if(SyncPairCrypto::isValidCode(
           SyncPairCrypto::normalizeCode(pairingPin))
       && !hello.remoteDeviceId.isEmpty()
       && SyncIdentity::secretForPeer(config,
                                      hello.remoteDeviceId).isEmpty())
        client.hello(host, port, true);

    // Пара: либо уже есть рабочий секрет, либо input — одноразовый код
    // sync/2 (или легаси-PIN для headless). Общий путь с GUI
    const SyncPairFlow::Result pair=SyncPairFlow::ensurePairing(
        config, client, host, port, hello, pairingPin);

    if(!pair.ok)
    {
        report.error=pair.error;
        report.errorCode=pair.errorCode;
        addLog(QStringLiteral("pairing failed: %1").arg(report.error));
        return report;
    }

    if(pair.pairedNow)
    {
        if(!hello.warning.isEmpty())
            addLog(QStringLiteral("server warning: %1").arg(hello.warning));

        addLog(QStringLiteral("paired, secret stored"));
    }

    addLog(QStringLiteral("hello ok, skew %1 ms").arg(hello.skewMs));

    // Группа: пустая у нас — принимаем удаленную или детерминированную общую.
    // Чужая непустая при коде/PIN — переезд между сетями: хаб источник
    // правды, берем его группу с логом (сервер нас уже принял с warning).
    // Без кода/PIN чужую группу не трогаем: строгий отказ уходит как есть
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
    // или девственная пара без кода — дальше не идем, нужен pair)
    if(!config->hasSyncPairing())
    {
        report.error=QStringLiteral("not paired: pair with the one-time code first");
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

    if(progressCallback)
        progressCallback(QStringLiteral("Получаю манифест с хаба"), 0, 0);

    // 2. Манифесты: чужой тянем, свой строим локально.
    // Манифест на десятках тысяч записей — большой кусок: ему отдельный
    // таймаут (дефолтные 15с рвут даже быстрый линк на 100k)
    QElapsedTimer mt; mt.start();
    client.setTimeoutMs(120000);
    SyncManifest remote=client.fetchManifest(host, port, scope);
    client.setTimeoutMs(15000);
    addLog(QStringLiteral("manifest fetch: notes=%1 branches=%2 in %3 ms")
           .arg(remote.notes.size()).arg(remote.branches.size())
           .arg(mt.restart()));

    if(!client.lastError().isEmpty())
    {
        report.error=client.lastError();
        addLog(QStringLiteral("manifest failed: %1").arg(report.error));
        return report;
    }

    if(progressCallback)
        progressCallback(QStringLiteral("Строю локальный манифест"), 0, 0);

    // Новый сервер вернул только scope и эхом подтвердил это.
    // Старый сервер поле ignored — тогда локальный манифест полный,
    // а план режется старым SyncScope::slice (совместимость)
    const bool remoteScoped=!scope.isEmpty()
                            && client.lastManifestScope()==scope;

    SyncManifest local=
        SyncManifestBuilder::build(model, SyncSidecar::instance(),
                                   tetradir,
                                   remoteScoped ? scope : QString());
    addLog(QStringLiteral("manifest local build: notes=%1 branches=%2 in %3 ms")
           .arg(local.notes.size()).arg(local.branches.size())
           .arg(mt.elapsed()));

    // Явный деструктивный выбор стратегии уже является подтверждением

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

            // Деструктивная стратегия рассчитана на всю базу (wipe/reset).
            // Для синка ветки её молчаливое применение стирало бы всё —
            // отказываем, флаг «спросить» оставляем на полный синк.
            if(!scope.isEmpty()
               && (strategy==SyncStrategy::MergeByName
                   || strategy==SyncStrategy::PullFromHub
                   || strategy==SyncStrategy::PushToHub))
            {
                addLog(QStringLiteral("scope %1: destructive strategy "
                                       "refused, full base required").arg(scope));
                report.error=QStringLiteral("Branch sync cannot replace the whole base. "
                                            "Run a full sync to use this strategy.");
                report.errorCode=QStringLiteral("scope-strategy");
                return report;
            }

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

    if(progressCallback)
        progressCallback(QStringLiteral("Сверяю изменения"), 0, 0);

    // 3. Сверка. Мы спок: равные mtime решает хаб (той же детерминированной
    // логикой: хаб бы скачал, мы — заливаем; итог одинаковый с обеих сторон)
    SyncPlan plan=SyncManifestBuilder::compare(local, remote, false);

    addLog(QStringLiteral("manifest local notes=%1 branches=%2, "
                          "remote notes=%3 branches=%4")
           .arg(local.notes.size()).arg(local.branches.size())
           .arg(remote.notes.size()).arg(remote.branches.size()));

    addLog(QStringLiteral("plan upNotes=%1 downNotes=%2 upBranches=%3 "
                          "downBranches=%4 deleteLocal=%5 conflicts=%6")
           .arg(plan.uploadNotes.size()).arg(plan.downloadNotes.size())
           .arg(plan.uploadBranches.size()).arg(plan.downloadBranches.size())
           .arg(plan.deleteLocal.size()).arg(plan.conflicts.size()));

    // Срез по поддереву ветки: манифесты уже полные (обход один),
    // экономия — превью/передача только по scope
    if(!scope.isEmpty())
    {
        plan=SyncScope::slice(plan, local, remote, scope);

        addLog(QStringLiteral("scope %1 plan upNotes=%2 downNotes=%3 "
                               "upBranches=%4 downBranches=%5 "
                               "deleteLocal=%6 pushTombs=%7")
               .arg(scope, QString::number(plan.uploadNotes.size()),
                    QString::number(plan.downloadNotes.size()),
                    QString::number(plan.uploadBranches.size()),
                    QString::number(plan.downloadBranches.size()),
                    QString::number(plan.deleteLocal.size()),
                    QString::number(plan.pushTombs.size())));
    }

    if(plan.empty())
    {
        addLog(QStringLiteral("already in sync"));
        report.ok=true;
        report.scopeEmpty=!scope.isEmpty();
        return report;
    }

    // 3а. Опасный синк — только с явного подтверждения. Принцип:
    // безопасное (создание/обновление) едет молча, любые удаления
    // подсвечиваются пользователю (история: случайное удаление базы
    // на клиенте молча обнулило сервер)
    // Индексы поддеревьев строятся один раз: раньше каждый tombstone
    // заново сканировал весь манифест.
    const SyncSubtreeCounter remoteCounter(remote.branches, remote.notes);
    const SyncSubtreeCounter localCounter(local.branches, local.notes);

    int remoteDeletes=0;

    for(const QString &id : plan.pushTombs)
        remoteDeletes+=remoteCounter.count(id);

    int localDeletes=0;

    for(const QString &id : plan.deleteLocal)
        localDeletes+=localCounter.count(id);

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

            // Контекст частичного «Применить»: диалог модален внутри run,
            // колбэк режет этот план по id тем же выполнением ниже
            partialPlan=plan;
            partialLocal=local;
            partialRemote=remote;
            partialClient=&client;
            partialHost=host;
            partialPort=port;
            partialReport=&report;

            const bool previewOk=previewResolver(preview, decisions);

            partialClient=nullptr;
            partialReport=nullptr;

            if(!previewOk)
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
    if(!pullByPlan(client, host, port, finalPlan, local, remote, report))
        return report;

    // 6. Чужие tombstone'ы — применить локально
    if(!applyRemoteDeletes(finalPlan, remote, report))
        return report;

    // 7. Свои tombstone'ы, которых нет у хаба — отправить ему
    if(!pushOwnTombstones(client, host, port, local, remote, finalPlan, report))
        return report;

    // 8. Фиксация: сначала хаб (сейв+перезагрузка), потом мы
    syncDbg(QStringLiteral("session: phase COMMIT model=%1")
            .arg(syncPtr(model)));
    addLog(QStringLiteral("commit hub"));

    if(progressCallback)
        progressCallback(QStringLiteral("Фиксирую на хабе"), 0, 0);

    if(!client.commit(host, port))
    {
        report.error=client.lastError().isEmpty()
                     ? QStringLiteral("hub commit failed")
                     : client.lastError();
        addLog(QStringLiteral("commit failed: %1").arg(report.error));
        return report;
    }

    if(progressCallback)
        progressCallback(QStringLiteral("Сохраняю локально"), 0, 0);

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
    if(progressCallback)
        progressCallback(QStringLiteral("Отправляю ветки"), 0,
                         plan.uploadBranches.size());

    int branchDone=0;

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
        ++branchDone;

        if(progressCallback && branchDone%25==0)
            progressCallback(QStringLiteral("Отправляю ветки"), branchDone,
                             plan.uploadBranches.size());
    }

    if(progressCallback)
        progressCallback(QStringLiteral("Отправляю заметки"), 0,
                         plan.uploadNotes.size());

    int noteDone=0;

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
        ++noteDone;

        if(progressCallback && noteDone%25==0)
            progressCallback(QStringLiteral("Отправляю заметки"), noteDone,
                             plan.uploadNotes.size());
    }

    return true;
}


bool SyncSession::pullByPlan(SyncClient &client,
                             const QString &host, quint16 port,
                             const SyncPlan &plan,
                             const SyncManifest &local,
                             const SyncManifest &remote,
                             SyncReport &report)
{    SYNC_TRACE();

    // Индекс id -> ветка из локального манифеста: для новых заметок
    // не надо обходить всё дерево, чтобы понять, что их там нет
    QHash<QString, QString> localBranches;

    for(auto it=local.notes.constBegin(); it!=local.notes.constEnd(); ++it)
        localBranches.insert(it.key(), it.value().branchId);

    // Каждый applyNote/applyBranch делает SyncSidecar::adopt, а adopt
    // сохраняет весь журнал целиком. Без пачки это O(n²) по числу
    // скачанных записей: к концу синка файл растёт, и запись на диск
    // превращается в узкое место. Сохраняем один раз на пачку
    const SyncSidecar::BulkGuard bulkGuard;

    if(progressCallback)
        progressCallback(QStringLiteral("Скачиваю ветки"), 0,
                         plan.downloadBranches.size());

    int branchDone=0;

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
        ++branchDone;

        if(progressCallback && branchDone%25==0)
            progressCallback(QStringLiteral("Скачиваю ветки"), branchDone,
                             plan.downloadBranches.size());
    }

    if(progressCallback)
        progressCallback(QStringLiteral("Скачиваю заметки"), 0,
                         plan.downloadNotes.size());

    int noteDone=0;

    // Заметки едут пачками: один HTTP-запрос на 100 штук. При старом хабе
    // (нет /pull-notes) — тихий откат на поштучный pullNote
    bool batchSupported=true;
    const int batchSize=100;

    for(int base=0; base<plan.downloadNotes.size(); )
    {
        const int count=qMin(batchSize, plan.downloadNotes.size()-base);
        const bool useBatch=(batchSupported && count>1);
        bool haveBatchAnswer=false;
        QMap<QString, QJsonObject> batchNotes;

        if(useBatch)
        {
            QJsonArray items;

            for(int i=base; i<base+count; ++i)
            {
                const QString &id=plan.downloadNotes.at(i);
                QJsonObject item;
                item[QStringLiteral("id")]=id;
                item[QStringLiteral("branch")]=
                    remote.notes.value(id).branchId;
                items.append(item);
            }

            const QJsonObject answer=client.pullNotes(host, port, items);

            if(answer.contains(QStringLiteral("notes")))
            {
                haveBatchAnswer=true;

                for(const QJsonValue &value
                    : answer.value(QStringLiteral("notes")).toArray())
                {
                    const QJsonObject note=value.toObject();
                    const QString id=note.value(QStringLiteral("id")).toString();

                    if(!id.isEmpty())
                        batchNotes.insert(id, note);
                }
            }
            else
                batchSupported=false;
        }

        for(int i=base; i<base+count; ++i)
        {
            const QString &id=plan.downloadNotes.at(i);
            const SyncNoteInfo &info=remote.notes.value(id);

            addLog(QStringLiteral("pull note %1").arg(id));

            QJsonObject note;
            QString pullError;

            if(haveBatchAnswer)
            {
                note=batchNotes.value(id);

                if(note.isEmpty())
                    pullError=QStringLiteral("pull note failed");
                else if(note.contains(QStringLiteral("error")))
                    pullError=note.value(QStringLiteral("error")).toString();
            }
            else
            {
                note=client.pullNote(host, port, id, info.branchId);

                if(note.contains(QStringLiteral("error")) ||
                   !client.lastError().isEmpty())
                    pullError=client.lastError().isEmpty()
                        ? QStringLiteral("pull note failed")
                        : client.lastError();
            }

            if(!pullError.isEmpty())
            {
                if(!haveBatchAnswer)
                {
                    if(!noteItem(client, report, QStringLiteral("note"), id,
                                 pullError))
                        return false;
                }
                else
                    skipItem(report,
                             QStringLiteral("note %1: %2").arg(id, pullError));

                continue;
            }

            QJsonObject full=note;
            full[QStringLiteral("id")]=id;
            full[QStringLiteral("branch")]=info.branchId;
            full[QStringLiteral("mtime")]=info.mtime;
            full[QStringLiteral("hash")]=info.hash;

            const QString error=SyncNoteIo::applyNote(model, tetradir, full,
                                                      &localBranches);

            if(!error.isEmpty())
            {
                skipItem(report,
                         QStringLiteral("note %1: %2").arg(id, error));
                continue;
            }

            report.downloadedNotes++;
            ++noteDone;

            if(progressCallback && noteDone%25==0)
                progressCallback(QStringLiteral("Скачиваю заметки"), noteDone,
                                 plan.downloadNotes.size());
        }

        base+=count;
    }

    return true;
}


bool SyncSession::applyRemoteDeletes(const SyncPlan &plan,
                                     const SyncManifest &remote,
                                     SyncReport &report)
{    SYNC_TRACE();

    // Приём 50k tombstone'ов: сайдкар сейвится один раз в конце пачки
    const SyncSidecar::BulkGuard bulkGuard;
    Q_UNUSED(bulkGuard);

    if(progressCallback)
        progressCallback(QStringLiteral("Применяю удаления"), 0,
                         plan.deleteLocal.size());

    int deleteDone=0;

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
        ++deleteDone;

        if(progressCallback && deleteDone%25==0)
            progressCallback(QStringLiteral("Применяю удаления"), deleteDone,
                             plan.deleteLocal.size());
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

    if(progressCallback)
        progressCallback(QStringLiteral("Отправляю удаления"), 0,
                         plan.pushTombs.size());

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

    if(!pullByPlan(client, host, port, fullPlan, emptyLocal, freshRemote,
                   report))
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

// Частичный синк из окна предпросмотра (аналог syncPartial мобилы):
// срез плана по id + то же выполнение (push/pull/удаления/commit).
// Поштучные ошибки оставляют строки в дереве (false), фатальные рвут
// так же. Итоговые счетчики — в финальном проходе run(), здесь только
// факт успеха (как у мобилы: отчет пачки отбрасывается)
bool SyncSession::syncPartialNow(
    const QMap<QString, SyncPreview::Action> &actions)
{
    if(partialClient==nullptr || partialReport==nullptr)
        return false;

    const QList<QString> keys=actions.keys();
    const QSet<QString> ids(keys.begin(), keys.end());

    // Срез плана по id (аналог slicePlan мобилы)
    SyncPlan sub;
    auto keep=[&](const QStringList &list) {
        QStringList out;
        for(const QString &id : list)
            if(ids.contains(id))
                out << id;
        return out;
    };
    sub.uploadNotes=keep(partialPlan.uploadNotes);
    sub.downloadNotes=keep(partialPlan.downloadNotes);
    sub.uploadBranches=keep(partialPlan.uploadBranches);
    sub.downloadBranches=keep(partialPlan.downloadBranches);
    sub.deleteLocal=keep(partialPlan.deleteLocal);
    sub.pushTombs=keep(partialPlan.pushTombs);

    if(sub.empty())
        return true;

    const SyncPreview::Preview subPreview=
        SyncPreview::classify(sub, partialLocal, partialRemote);
    SyncPreview::Decisions subDecisions;
    subDecisions.byId=actions;
    const SyncPlan finalSub=SyncPreview::applyDecisions(
        sub, subPreview, subDecisions, partialLocal, partialRemote);

    addLog(QStringLiteral("partial: %1 item(s)").arg(ids.size()));

    SyncReport tmp;
    if(!pushByPlan(*partialClient, partialHost, partialPort,
                   finalSub, partialLocal, tmp))
        return false;
    if(!pullByPlan(*partialClient, partialHost, partialPort,
                   finalSub, partialLocal, partialRemote, tmp))
        return false;
    if(!applyRemoteDeletes(finalSub, partialRemote, tmp))
        return false;
    if(!pushOwnTombstones(*partialClient, partialHost, partialPort,
                           partialLocal, partialRemote, finalSub, tmp))
        return false;

    if(!partialClient->commit(partialHost, partialPort))
        return false;

    model->save();
    model->reload();
    addLog(QStringLiteral("partial: committed"));

    Q_UNUSED(partialReport);
    return tmp.errors.isEmpty();
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

    if(!pullByPlan(client, host, port, fullPlan, emptyLocal, freshRemote,
                   report))
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

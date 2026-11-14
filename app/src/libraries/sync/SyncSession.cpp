#include "SyncSession.h"
#include "SyncClient.h"
#include "SyncNoteIo.h"
#include "SyncSidecar.h"
#include "SyncIdentity.h"
#include "SyncDebug.h"
#include "models/tree/KnowTreeModel.h"
#include "models/appConfig/AppConfig.h"

#include <QDebug>


SyncSession::SyncSession(KnowTreeModel *model, AppConfig *config,
                         const QString &tetradir)
    : model(model),
      config(config),
      tetradir(tetradir)
{
}


QStringList SyncSession::log(void) const
{
    return logLines;
}


void SyncSession::addLog(const QString &line)
{
    logLines << line;

    // Живой прогон (ПК + VM): каждый шаг сразу в stderr, иначе зависший
    // клиент молчит и непонятно где встал. Только под MYTETRA_SYNC_DEBUG=1
    if(syncDebugEnabled())
        qDebug() << "SyncStep:" << line;
}


SyncReport SyncSession::run(const QString &host, quint16 port,
                              const QString &pairingPin)
{
    SyncReport report;
    SyncClient client(config);

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

    // 2. Манифесты: чужой тянем, свой строим локально
    const SyncManifest remote=client.fetchManifest(host, port);

    if(!client.lastError().isEmpty())
    {
        report.error=client.lastError();
        addLog(QStringLiteral("manifest failed: %1").arg(report.error));
        return report;
    }

    const SyncManifest local=
        SyncManifestBuilder::build(model, SyncSidecar::instance(),
                                   tetradir);

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

    // 4. Заливка нашего на хаб
    if(!pushByPlan(client, host, port, plan, local, report))
        return report;

    // 5. Скачивание с хаба и применение у себя
    if(!pullByPlan(client, host, port, plan, remote, report))
        return report;

    // 6. Чужие tombstone'ы — применить локально
    if(!applyRemoteDeletes(plan, remote, report))
        return report;

    // 7. Свои tombstone'ы, которых нет у хаба — отправить ему
    if(!pushOwnTombstones(client, host, port, local, remote, report))
        return report;

    // 8. Фиксация: сначала хаб (сейв+перезагрузка), потом мы
    addLog(QStringLiteral("commit hub"));

    if(!client.commit(host, port))
    {
        report.error=client.lastError().isEmpty()
                     ? QStringLiteral("hub commit failed")
                     : client.lastError();
        addLog(QStringLiteral("commit failed: %1").arg(report.error));
        return report;
    }

    model->save();
    model->reload();
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
{
    // Ветки раньше заметок: заметка ссылается на ветку
    for(const QString &id : plan.uploadBranches)
    {
        QJsonObject branch=SyncNoteIo::serializeBranch(model, id);

        if(branch.contains(QStringLiteral("error")))
        {
            report.error=QStringLiteral("serialize branch failed");
            return false;
        }

        // mtime/hash для adopt на хабе — из локального манифеста
        const SyncBranchInfo &info=local.branches.value(id);
        branch[QStringLiteral("mtime")]=info.mtime;
        branch[QStringLiteral("hash")]=info.hash;

        addLog(QStringLiteral("push branch %1").arg(id));

        if(!client.pushBranch(host, port, branch))
        {
            report.error=client.lastError();
            return false;
        }

        report.uploadedBranches++;
    }

    for(const QString &id : plan.uploadNotes)
    {
        QJsonObject note=SyncNoteIo::serializeNote(model, tetradir, id);

        if(note.contains(QStringLiteral("error")))
        {
            report.error=QStringLiteral("serialize note failed");
            return false;
        }

        const SyncNoteInfo &info=local.notes.value(id);
        note[QStringLiteral("mtime")]=info.mtime;
        note[QStringLiteral("hash")]=info.hash;
        note[QStringLiteral("branch")]=info.branchId;

        addLog(QStringLiteral("push note %1").arg(id));

        if(!client.pushNote(host, port, note))
        {
            report.error=client.lastError();
            return false;
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
{
    for(const QString &id : plan.downloadBranches)
    {
        addLog(QStringLiteral("pull branch %1").arg(id));

        const QJsonObject branch=client.pullBranch(host, port, id);

        if(branch.contains(QStringLiteral("error")) ||
           !client.lastError().isEmpty())
        {
            report.error=client.lastError().isEmpty()
                         ? QStringLiteral("pull branch failed")
                         : client.lastError();
            return false;
        }

        QJsonObject full=branch;
        full[QStringLiteral("id")]=id;

        const SyncBranchInfo &info=remote.branches.value(id);
        full[QStringLiteral("mtime")]=info.mtime;
        full[QStringLiteral("hash")]=info.hash;

        const QString error=SyncNoteIo::applyBranch(model, full);

        if(!error.isEmpty())
        {
            report.error=error;
            return false;
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
            report.error=client.lastError().isEmpty()
                         ? QStringLiteral("pull note failed")
                         : client.lastError();
            return false;
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
            report.error=error;
            return false;
        }

        report.downloadedNotes++;
    }

    return true;
}


bool SyncSession::applyRemoteDeletes(const SyncPlan &plan,
                                     const SyncManifest &remote,
                                     SyncReport &report)
{
    for(const QString &id : plan.deleteLocal)
    {
        const qint64 deletedAt=remote.tombstones.value(id, 0);

        addLog(QStringLiteral("delete local %1").arg(id));

        const QString error=SyncNoteIo::applyDelete(model, id, deletedAt);

        if(!error.isEmpty())
        {
            report.error=error;
            return false;
        }

        report.appliedDeletes++;
    }

    return true;
}


bool SyncSession::pushOwnTombstones(SyncClient &client,
                                    const QString &host, quint16 port,
                                    const SyncManifest &local,
                                    const SyncManifest &remote,
                                    SyncReport &report)
{
    for(auto it=local.tombstones.constBegin();
        it!=local.tombstones.constEnd(); ++it)
    {
        // У хаба уже есть — пересылать нечего
        if(remote.tombstones.contains(it.key()))
            continue;

        addLog(QStringLiteral("push delete %1").arg(it.key()));

        if(!client.pushDelete(host, port, it.key(), it.value()))
        {
            report.error=client.lastError().isEmpty()
                         ? QStringLiteral("push delete failed")
                         : client.lastError();
            return false;
        }
    }

    return true;
}

#include <QJsonArray>

#include "SyncHub.h"
#include "SyncServer.h"
#include "SyncHandshake.h"
#include "SyncManifest.h"
#include "SyncSidecar.h"
#include "SyncNoteIo.h"
#include "SyncDebug.h"
#include "models/tree/KnowTreeModel.h"
#include "models/appConfig/AppConfig.h"

#include <QDateTime>

#include <QDebug>


bool SyncHub::dataAllowed(void) const
{    SYNC_TRACE();

    // Кольцо: данные разрешены при хоть одной паре (4-5 устройств),
    // привязка к конкретному пиру больше не требуется
    return config!=nullptr && config->hasSyncPairing();
}


static QJsonObject notPairedAnswer(void)
{    SYNC_TRACE();

    QJsonObject answer;
    answer[QStringLiteral("error")]=QStringLiteral("not-paired");
    return answer;
}


// Проверка proof пары на data-маршруте. Пустая строка — ок,
// иначе текст ошибки для поля error (auth-failed)
static QString checkAuth(AppConfig *config, const QString &route,
                         const QJsonObject &request)
{    SYNC_TRACE();

    QString detail;

    if(!SyncHandshake::checkDataAuth(config, route, request,
                                     QDateTime::currentMSecsSinceEpoch(),
                                     detail))
    {
        const QString error=detail.isEmpty() ? QStringLiteral("auth failed")
                                             : detail;
        syncDbg("hub: "+route+" rejected: "+error);
        return error;
    }

    return QString();
}


static QJsonObject authFailedAnswer(const QString &detail)
{
    QJsonObject answer;
    answer[QStringLiteral("error")]=QStringLiteral("auth-failed");
    answer[QStringLiteral("detail")]=detail;
    return answer;
}


SyncHub::SyncHub(KnowTreeModel *model, AppConfig *config,
                 SyncSidecar &sidecar, const QString &tetradir,
                 QObject *parent, const QString &pairingPin)
    : QObject(parent),
      model(model),
      config(config),
      sidecar(&sidecar),
      tetradir(tetradir)
{    SYNC_TRACE_EXTRA(QStringLiteral("model=%1").arg(syncPtr(model)));

    server=new SyncServer(this);
    SyncHandshake::registerRoutes(server, this->config, pairingPin);

    // LAN-discovery: отвечаем на пробы скана своей идентичностью
    server->setDiscoveryIdentity([config]() -> QJsonObject {
        QJsonObject identity;
        identity[QStringLiteral("deviceId")]=config->get_syncDeviceId();
        identity[QStringLiteral("deviceName")]=config->get_syncDeviceName();
        return identity;
    });

    // Манифест локальной базы для спока
    server->addRoute(QStringLiteral("POST /manifest"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /manifest"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        const SyncManifest manifest=
            SyncManifestBuilder::build(this->model, *this->sidecar,
                                       this->tetradir);
        return manifest.toJson();
    });

    // Заметка целиком: поля, блоб текста как лежит, аттачи байтами.
    // Шифрованные отдаются шифртекстом, ключ не нужен и не отдается
    server->addRoute(QStringLiteral("POST /pull-note"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /pull-note"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        return pullNote(request.value(QStringLiteral("id")).toString());
    });

    // Ветка целиком: поля + parentId
    server->addRoute(QStringLiteral("POST /pull-branch"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /pull-branch"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        return pullBranch(request.value(QStringLiteral("id")).toString());
    });

    // Прием заметки целиком (создание или обновление, id сохраняются).
    // Ветка должна уже существовать: клиент шлет ветки раньше заметок
    server->addRoute(QStringLiteral("POST /push-note"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /push-note"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        return applyNote(request);
    });
    // Прием ветки целиком (создание или обновление полей, id сохраняется)
    server->addRoute(QStringLiteral("POST /push-branch"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /push-branch"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        return applyBranch(request);
    });

    // Применить удаление по tombstone: ветка с поддеревом или заметка.
    // Время tombstone'а — удаленное, не локальное (иначе пинг-понг)
    server->addRoute(QStringLiteral("POST /push-delete"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /push-delete"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        return applyDelete(request.value(QStringLiteral("id")).toString(),
                           request.value(QStringLiteral("deletedAt")).toVariant().toLongLong());
    });

    // Фиксация сессии: сейв базы и перезагрузка моделей,
    // чтобы виды показали примененное. Вызывает клиент в конце плана
    server->addRoute(QStringLiteral("POST /commit"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /commit"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        syncDbg(QStringLiteral("hub: BEFORE save model=%1 tid=%2")
                .arg(syncPtr(this->model), syncTid()));
        this->model->save();
        syncDbg(QStringLiteral("hub: AFTER save, BEFORE reload"));
        this->model->reload();
        syncDbg(QStringLiteral("hub: AFTER reload root=%1")
                .arg(syncPtr(this->model->getRootItemPublic())));

        QJsonObject answer;
        answer[QStringLiteral("ok")]=true;
        return answer;
    });

    // Полная замена содержимого хаба (стратегия "заменить серверные
    // моими"): база уходит в корзину без tombstone'ов, журнал стирается.
    // Дальше клиент заливает свое. Только для стратегий первого синка!
    server->addRoute(QStringLiteral("POST /reset"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /reset"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        syncDbg(QStringLiteral("hub: reset BEFORE wipe model=%1")
                .arg(syncPtr(this->model)));
        const QString error=SyncNoteIo::wipeAll(this->model);
        syncDbg(QStringLiteral("hub: reset AFTER wipe err=%1").arg(error));

        QJsonObject answer;

        if(!error.isEmpty())
            answer[QStringLiteral("error")]=error;
        else
            answer[QStringLiteral("ok")]=true;

        return answer;
    });

    // Слияние веток по именам (стратегия "объединение"): клиент шлет id
    // своих веток, хаб вливает их в одноименные свои (id хаба побеждает)
    server->addRoute(QStringLiteral("POST /merge-by-name"),
                     [this](const QJsonObject &request) -> QJsonObject {
        if(!this->dataAllowed())
            return notPairedAnswer();
        const QString authErr=checkAuth(this->config, QStringLiteral("POST /merge-by-name"), request);
        if(!authErr.isEmpty())
                return authFailedAnswer(authErr);

        QStringList ids;
        const QJsonArray arr=request.value(QStringLiteral("branchIds")).toArray();

        for(const QJsonValue &value : arr)
            ids << value.toString();

        syncDbg(QStringLiteral("hub: merge-by-name BEFORE model=%1 n=%2")
                .arg(syncPtr(this->model)).arg(ids.size()));
        QJsonObject answer;
        const QString error=SyncNoteIo::mergeByName(this->model, ids);

        if(!error.isEmpty())
            answer[QStringLiteral("error")]=error;
        else
            answer[QStringLiteral("ok")]=true;

        return answer;
    });
}


SyncHub::~SyncHub(void)
{    SYNC_TRACE();

    stop();
}


bool SyncHub::start(quint16 port)
{    SYNC_TRACE();

    return server->start(port);
}


void SyncHub::stop(void)
{    SYNC_TRACE();

    server->stop();
}


bool SyncHub::isRunning(void) const
{
    return server->isRunning();
}


quint16 SyncHub::port(void) const
{
    return server->port();
}


void SyncHub::setHub(bool hub)
{
    this->hub=hub;
}


bool SyncHub::isHub(void) const
{
    return hub;
}


QJsonObject SyncHub::pullNote(const QString &id)
{    SYNC_TRACE_EXTRA(QStringLiteral("id=%1").arg(id));

    return SyncNoteIo::serializeNote(model, tetradir, id);
}


QJsonObject SyncHub::pullBranch(const QString &id)
{    SYNC_TRACE_EXTRA(QStringLiteral("id=%1").arg(id));

    return SyncNoteIo::serializeBranch(model, id);
}


QJsonObject SyncHub::applyNote(const QJsonObject &request)
{    SYNC_TRACE();

    const QString id=request.value(QStringLiteral("id")).toString();
    const QString error=SyncNoteIo::applyNote(model, tetradir, request);

    if(error.isEmpty())
        syncDbg("hub: push-note "+id+" applied");
    else
        syncDbg("hub: push-note "+id+" rejected: "+error);

    QJsonObject answer;

    if(!error.isEmpty())
        answer[QStringLiteral("error")]=error;
    else
        answer[QStringLiteral("ok")]=true;

    return answer;
}


QJsonObject SyncHub::applyBranch(const QJsonObject &request)
{    SYNC_TRACE();

    const QString id=request.value(QStringLiteral("id")).toString();
    const QString error=SyncNoteIo::applyBranch(model, request);

    if(error.isEmpty())
        syncDbg("hub: push-branch "+id+" applied");
    else
        syncDbg("hub: push-branch "+id+" rejected: "+error);

    QJsonObject answer;

    if(!error.isEmpty())
        answer[QStringLiteral("error")]=error;
    else
        answer[QStringLiteral("ok")]=true;

    return answer;
}


QJsonObject SyncHub::applyDelete(const QString &id, qint64 deletedAt)
{    SYNC_TRACE_EXTRA(QStringLiteral("id=%1").arg(id));

    const QString error=SyncNoteIo::applyDelete(model, id, deletedAt);

    if(error.isEmpty())
        syncDbg("hub: push-delete "+id+" applied");
    else
        syncDbg("hub: push-delete "+id+" rejected: "+error);

    QJsonObject answer;

    if(!error.isEmpty())
        answer[QStringLiteral("error")]=error;
    else
        answer[QStringLiteral("ok")]=true;

    return answer;
}

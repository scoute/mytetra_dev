#include <QJsonArray>

#include "SyncHub.h"
#include "SyncServer.h"
#include "SyncHandshake.h"
#include "SyncManifest.h"
#include "SyncSidecar.h"
#include "SyncNoteIo.h"
#include "SyncDebug.h"
#include "SyncDataCrypto.h"
#include "models/tree/KnowTreeModel.h"
#include "models/appConfig/AppConfig.h"

#include <QElapsedTimer>
#include <QDateTime>

#include <QDebug>


bool SyncHub::dataAllowed(void) const
{    SYNC_TRACE();

    // Кольцо: данные разрешены при хоть одной паре (4-5 устройств),
    // привязка к конкретному пиру больше не требуется
    return config!=nullptr && config->hasSyncPairing();
}


struct PreparedData
{
    bool ok=false;
    bool encrypted=false;
    SyncDataCrypto::Context ctx;
    QJsonObject body;
    QJsonObject error;
};


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

    // Общий вход data-маршрута: либо старый HMAC-proof (plain), либо
    // enc-конверт (AEAD ключом пары). На выходе — либо ответ как есть,
    // либо зашифрованный тем же ключом
    auto prepare=[this](const QString &route,
                        const QJsonObject &request) -> PreparedData {
        PreparedData p;

        if(!this->dataAllowed())
        {
            p.error=notPairedAnswer();
            return p;
        }

        if(request.contains(QStringLiteral("_enc")))
        {
            QString error;

            if(!SyncDataCrypto::openRequest(
                   this->config, this->config->get_syncDeviceId(), route,
                   request,
                   QDateTime::currentMSecsSinceEpoch(),
                   p.body, p.ctx, error))
            {
                p.error=authFailedAnswer(error);
                return p;
            }

            p.encrypted=true;
        }
        else
        {
            const QString authErr=checkAuth(this->config, route, request);

            if(!authErr.isEmpty())
            {
                p.error=authFailedAnswer(authErr);
                return p;
            }

            p.body=request;
        }

        p.ok=true;
        return p;
    };

    auto finish=[this](const PreparedData &p,
                       const QJsonObject &answer) -> QJsonObject {
        if(p.encrypted)
            return SyncDataCrypto::sealResponse(p.ctx, answer);

        return answer;
    };

    // Манифест локальной базы для спока
    server->addRoute(QStringLiteral("POST /manifest"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /manifest"), request);
        if(!p.ok)
            return p.error;

        QString scope=p.body.value(QStringLiteral("scope")).toString();

        if(scope==QStringLiteral("0"))
            scope.clear();

        QElapsedTimer mt; mt.start();
        const SyncManifest manifest=
            SyncManifestBuilder::build(this->model, *this->sidecar,
                                       this->tetradir, scope);
        const qint64 buildMs=mt.restart();
        QJsonObject mjson=manifest.toJson();

        // Эхо scope: клиент понимает, что старый сервер поле ignored
        // и нужно строить полный локальный манифест/резать план
        if(!scope.isEmpty())
            mjson[QStringLiteral("scope")]=scope;

        qDebug() << "SyncHub: manifest built notes="
                 << manifest.notes.size() << "branches="
                 << manifest.branches.size() << "tombs="
                 << manifest.tombstones.size() << "scope=" << scope
                 << "in" << buildMs
                 << "ms + toJson" << mt.elapsed() << "ms";
        return finish(p, mjson);
    });

    // Заметка целиком: поля, блоб текста как лежит, аттачи байтами.
    // Шифрованные отдаются шифртекстом, ключ не нужен и не отдается
    server->addRoute(QStringLiteral("POST /pull-note"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /pull-note"), request);
        if(!p.ok)
            return p.error;

        return finish(p, pullNote(
            p.body.value(QStringLiteral("id")).toString(),
            p.body.value(QStringLiteral("branch")).toString()));
    });

    // Ветка целиком: поля + parentId
    server->addRoute(QStringLiteral("POST /pull-branch"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /pull-branch"), request);
        if(!p.ok)
            return p.error;

        return finish(p, pullBranch(p.body.value(QStringLiteral("id")).toString()));
    });

    // Прием заметки целиком (создание или обновление, id сохраняются).
    // Ветка должна уже существовать: клиент шлет ветки раньше заметок
    server->addRoute(QStringLiteral("POST /push-note"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /push-note"), request);
        if(!p.ok)
            return p.error;

        return finish(p, applyNote(p.body));
    });
    // Прием ветки целиком (создание или обновление полей, id сохраняется)
    server->addRoute(QStringLiteral("POST /push-branch"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /push-branch"), request);
        if(!p.ok)
            return p.error;

        return finish(p, applyBranch(p.body));
    });

    // Применить удаление по tombstone: ветка с поддеревом или заметка.
    // Время tombstone'а — удаленное, не локальное (иначе пинг-понг)
    server->addRoute(QStringLiteral("POST /push-delete"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /push-delete"), request);
        if(!p.ok)
            return p.error;

        return finish(p, applyDelete(
            p.body.value(QStringLiteral("id")).toString(),
            p.body.value(QStringLiteral("deletedAt")).toVariant().toLongLong()));
    });

    // Фиксация сессии: сейв базы и перезагрузка моделей,
    // чтобы виды показали примененное. Вызывает клиент в конце плана
    server->addRoute(QStringLiteral("POST /commit"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /commit"), request);
        if(!p.ok)
            return p.error;

        syncDbg(QStringLiteral("hub: BEFORE save model=%1 tid=%2")
                .arg(syncPtr(this->model), syncTid()));
        this->model->save();
        syncDbg(QStringLiteral("hub: AFTER save, BEFORE reload"));
        this->model->reload();
        syncDbg(QStringLiteral("hub: AFTER reload root=%1")
                .arg(syncPtr(this->model->getRootItemPublic())));

        QJsonObject answer;
        answer[QStringLiteral("ok")]=true;
        return finish(p, answer);
    });

    // Полная замена содержимого хаба (стратегия "заменить серверные
    // моими"): база уходит в корзину без tombstone'ов, журнал стирается.
    // Дальше клиент заливает свое. Только для стратегий первого синка!
    server->addRoute(QStringLiteral("POST /reset"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /reset"), request);
        if(!p.ok)
            return p.error;

        syncDbg(QStringLiteral("hub: reset BEFORE wipe model=%1")
                .arg(syncPtr(this->model)));
        const QString error=SyncNoteIo::wipeAll(this->model);
        syncDbg(QStringLiteral("hub: reset AFTER wipe err=%1").arg(error));

        QJsonObject answer;

        if(!error.isEmpty())
            answer[QStringLiteral("error")]=error;
        else
            answer[QStringLiteral("ok")]=true;

        return finish(p, answer);
    });

    // Слияние веток по именам (стратегия "объединение"): клиент шлет id
    // своих веток, хаб вливает их в одноименные свои (id хаба побеждает)
    server->addRoute(QStringLiteral("POST /merge-by-name"),
                     [this, prepare, finish](const QJsonObject &request) -> QJsonObject {
        const PreparedData p=prepare(QStringLiteral("POST /merge-by-name"), request);
        if(!p.ok)
            return p.error;

        QStringList ids;
        const QJsonArray arr=p.body.value(QStringLiteral("branchIds")).toArray();

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

        return finish(p, answer);
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


bool SyncHub::setTls(const QString &certPath, const QString &keyPath,
                     QString *error)
{
    return server->setTls(certPath, keyPath, error);
}


void SyncHub::clearTls(void)
{
    server->clearTls();
}


bool SyncHub::tlsEnabled(void) const
{
    return server->tlsEnabled();
}


QString SyncHub::tlsFingerprint(void) const
{
    return server->tlsFingerprint();
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


QJsonObject SyncHub::pullNote(const QString &id, const QString &branchId)
{    SYNC_TRACE_EXTRA(QStringLiteral("id=%1").arg(id));

    return SyncNoteIo::serializeNote(model, tetradir, id, branchId);
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

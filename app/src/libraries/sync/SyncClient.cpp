#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QMap>
#include <QTimer>
#include <QDateTime>
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>

#include "SyncClient.h"
#include "SyncHandshake.h"
#include "SyncDebug.h"
#include "models/appConfig/AppConfig.h"

#include <QDebug>


SyncClient::SyncClient(AppConfig *config) : config(config)
{    SYNC_TRACE();

}


void SyncClient::setTimeoutMs(int ms)
{
    timeoutMs=ms;
}


QJsonObject SyncClient::withAuth(const QString &route,
                                  const QJsonObject &body)
{    SYNC_TRACE();

    return SyncHandshake::buildDataAuth(config, route, body,
                                        peerSecretText);
}


void SyncClient::setPeerSecret(const QString &secret)
{
    peerSecretText=secret;
}


QString SyncClient::peerSecret(void) const
{
    return peerSecretText;
}


QString SyncClient::lastError(void) const
{
    return lastErrorText;
}


bool SyncClient::lastTransportOk(void) const
{
    return lastTransportOkFlag;
}


QJsonObject SyncClient::postJson(const QString &host, quint16 port,
                                 const QString &path, const QJsonObject &body,
                                 bool &ok)
{    SYNC_TRACE_EXTRA(host);

    ok=false;
    lastTransportOkFlag=false;
    lastErrorText.clear();

    QNetworkAccessManager manager;
    QNetworkRequest request;
    request.setUrl(QUrl(QStringLiteral("http://%1:%2%3")
                        .arg(host).arg(port).arg(path)));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=utf-8"));

    const QByteArray payload=QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply *reply=manager.post(request, payload);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished,
                     &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,
                     &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    if(!timer.isActive())
    {
        // Сработал таймаут, а не ответ
        reply->abort();
        reply->deleteLater();
        lastErrorText=QStringLiteral("Sync request timed out.");
        return QJsonObject();
    }

    timer.stop();

    if(reply->error()!=QNetworkReply::NoError)
    {
        lastErrorText=reply->errorString();

        if(syncDebugEnabled())
            qDebug() << "SyncClient:" << path << "transport error:"
                     << lastErrorText;

        reply->deleteLater();
        return QJsonObject();
    }

    QJsonParseError parseError;
    const QJsonDocument doc=QJsonDocument::fromJson(reply->readAll(),
                                                    &parseError);
    reply->deleteLater();

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
    {
        lastErrorText=QStringLiteral("Bad sync reply.");
        return QJsonObject();
    }

    ok=true;
    lastTransportOkFlag=true;

    // Ответ сервера с его ошибкой (не транспорт): тоже в лог,
    // иначе по таймингам видно "ok", а сессия падает дальше молча
    if(syncDebugEnabled())
    {
        const QJsonObject obj=doc.object();
        const QString err=obj.value(QStringLiteral("error")).toString();
        const QString warn=obj.value(QStringLiteral("warning")).toString();

        if(!err.isEmpty() || !warn.isEmpty())
            qDebug() << "SyncClient:" << path << "server says:"
                     << (err.isEmpty() ? QString() : QStringLiteral("error=")+err)
                     << (warn.isEmpty() ? QString() : QStringLiteral("warning=")+warn);
    }

    return doc.object();
}


QJsonObject SyncClient::getJson(const QString &host, quint16 port,
                                const QString &path, bool &ok)
{    SYNC_TRACE_EXTRA(host);

    ok=false;
    lastTransportOkFlag=false;
    lastErrorText.clear();

    QNetworkAccessManager manager;
    QNetworkRequest request;
    request.setUrl(QUrl(QStringLiteral("http://%1:%2%3")
                        .arg(host).arg(port).arg(path)));

    QNetworkReply *reply=manager.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished,
                     &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,
                     &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    if(!timer.isActive())
    {
        reply->abort();
        reply->deleteLater();
        lastErrorText=QStringLiteral("Sync request timed out.");
        return QJsonObject();
    }

    timer.stop();

    if(reply->error()!=QNetworkReply::NoError)
    {
        lastErrorText=reply->errorString();

        if(syncDebugEnabled())
            qDebug() << "SyncClient:" << path << "transport error:"
                     << lastErrorText;

        reply->deleteLater();
        return QJsonObject();
    }

    QJsonParseError parseError;
    const QJsonDocument doc=QJsonDocument::fromJson(reply->readAll(),
                                                    &parseError);
    reply->deleteLater();

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
    {
        lastErrorText=QStringLiteral("Bad sync reply.");
        return QJsonObject();
    }

    ok=true;
    lastTransportOkFlag=true;

    // Ответ сервера с его ошибкой (не транспорт): тоже в лог,
    // иначе по таймингам видно "ok", а сессия падает дальше молча
    if(syncDebugEnabled())
    {
        const QJsonObject obj=doc.object();
        const QString err=obj.value(QStringLiteral("error")).toString();
        const QString warn=obj.value(QStringLiteral("warning")).toString();

        if(!err.isEmpty() || !warn.isEmpty())
            qDebug() << "SyncClient:" << path << "server says:"
                     << (err.isEmpty() ? QString() : QStringLiteral("error=")+err)
                     << (warn.isEmpty() ? QString() : QStringLiteral("warning=")+warn);
    }

    return doc.object();
}


SyncHelloResult SyncClient::hello(const QString &host, quint16 port,
                                    bool pairRequest)
{    SYNC_TRACE_EXTRA(host);

    SyncHelloResult result;
    result.serverTime=QDateTime::currentMSecsSinceEpoch();

    if(syncDebugEnabled())
        qDebug() << "SyncClient: hello" << host << port
                 << (pairRequest ? QStringLiteral("pairRequest")
                                 : QString());

    // Кандидаты секрета: сначала легаси-одиночка, затем кольцо
    // (у человека 4-5 устройств — hello пробует каждый, рабочий
    // запоминается для data-маршрутов). Дедупликация: одно устройство —
    // один secret
    QStringList candidates;
    const QString primary=config->get_syncPairingSecret();

    if(!primary.isEmpty())
        candidates << primary;

    const QMap<QString, QString> ring=config->get_syncSecretsRing();

    for(auto it=ring.constBegin(); it!=ring.constEnd(); ++it)
        if(!it.value().isEmpty() && !candidates.contains(it.value()))
            candidates << it.value();

    // Девственное устройство (кольцо пусто): hello без proof,
    // сервер это различает (первое знакомство / pairRequest)
    if(candidates.isEmpty())
        candidates << QString();

    QJsonObject answer;
    QString usedSecret;
    bool transported=false;

    for(const QString &candidate : candidates)
    {
        bool ok=false;
        answer=postJson(
            host, port, QStringLiteral("/hello"),
            SyncHandshake::buildHello(config, result.serverTime,
                                     pairRequest, candidate), ok);

        if(!ok)
        {
            result.errorCode=QStringLiteral("network");
            result.errorText=lastErrorText;
            return result;
        }

        transported=true;
        usedSecret=candidate;

        const QString errPreview=answer.value(QStringLiteral("error")).toString();

        // auth-failed и кандидаты еще есть — пробуем следующий секрет.
        // Любой другой исход (ok, not-paired, foreign-network, версии,
        // часы, pairPending) — окончательный, дальше не перебираем
        if(!answer.value(QStringLiteral("ok")).toBool(false)
           && errPreview==QStringLiteral("auth-failed")
           && candidate!=candidates.last())
        {
            syncDbg(QStringLiteral("hello: secret #%1 rejected, trying next")
                    .arg(candidates.indexOf(candidate)+1));
            continue;
        }

        break;
    }

    if(!transported)
    {
        result.errorCode=QStringLiteral("network");
        result.errorText=lastErrorText;
        return result;
    }

    // Рабочий секрет — для всех data-маршрутов сессии
    peerSecretText=usedSecret;

    // Hello от самого себя (синк на свой же адрес): дальше не идем.
    // Единая точка отказа для GUI, headless и перепроверок пары
    const QString answerDeviceId=answer.value(QStringLiteral("deviceId")).toString();

    if(!answerDeviceId.isEmpty() && answerDeviceId==config->get_syncDeviceId())
    {
        result.ok=false;
        result.errorCode=QStringLiteral("self-hello");
        result.errorText=QStringLiteral("Sync with itself is not allowed: %1 answered as this device (%2). Check the address.")
            .arg(host, config->get_syncDeviceName().isEmpty()
                 ? answerDeviceId : config->get_syncDeviceName());
        result.remoteDeviceId=answerDeviceId;
        return result;
    }

    // Сервер уже все проверил; клиент только фиксирует вердикт
    result.ok=answer.value(QStringLiteral("ok")).toBool(false);
    result.serverTime=answer.value(QStringLiteral("serverTime"))
                           .toVariant().toLongLong();
    result.skewMs=answer.value(QStringLiteral("skewMs"))
                       .toVariant().toLongLong();
    result.paired=answer.value(QStringLiteral("paired")).toBool(false);
    result.remoteDeviceId=answer.value(QStringLiteral("deviceId")).toString();
    result.remoteDeviceName=answer.value(QStringLiteral("deviceName")).toString();
    result.remoteGroupId=answer.value(QStringLiteral("groupId")).toString();
    result.warning=answer.value(QStringLiteral("warning")).toString();
    result.pairPending=answer.value(QStringLiteral("pairPending"))
                            .toBool(false);

    if(!result.ok)
    {
        result.errorCode=answer.value(QStringLiteral("error")).toString();
        result.errorText=answer.value(QStringLiteral("detail")).toString();
    }

    return result;
}


SyncManifest SyncClient::fetchManifest(const QString &host, quint16 port)
{    SYNC_TRACE_EXTRA(host);

    bool ok=false;
    const QJsonObject answer=postJson(
        host, port, QStringLiteral("/manifest"),
        withAuth(QStringLiteral("POST /manifest"), QJsonObject()), ok);

    if(!ok)
        return SyncManifest();

    // Сервер отклонил (not-paired/auth-failed): пустой манифест без ошибки
    // в lastError звал бы заливку всего локального — фиксируем отказ
    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return SyncManifest();
    }

    return SyncManifest::fromJson(answer);
}


QJsonObject SyncClient::pullNote(const QString &host, quint16 port,
                                 const QString &id)
{    SYNC_TRACE_EXTRA(host);

    QJsonObject request;
    request[QStringLiteral("id")]=id;

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/pull-note"),
                                      withAuth(QStringLiteral("POST /pull-note"), request), ok);

    if(!ok)
        return QJsonObject();

    // Сервер отклонил применение (bad id, no text и т.п.): текст лежит
    // в answer, а не в lastError. Без этого сессия падает с пустым
    // "pull note failed" (та же ловушка что была в push, юнит 072)
    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return QJsonObject();
    }

    return answer;
}


QJsonObject SyncClient::pullBranch(const QString &host, quint16 port,
                                   const QString &id)
{    SYNC_TRACE_EXTRA(host);

    QJsonObject request;
    request[QStringLiteral("id")]=id;

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/pull-branch"),
                                      withAuth(QStringLiteral("POST /pull-branch"), request), ok);

    if(!ok)
        return QJsonObject();

    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return QJsonObject();
    }

    return answer;
}


bool SyncClient::pushNote(const QString &host, quint16 port,
                          const QJsonObject &note)
{    SYNC_TRACE_EXTRA(host+QStringLiteral(" ")+note.value(QStringLiteral("id")).toString());

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/push-note"),
                                      withAuth(QStringLiteral("POST /push-note"), note), ok);

    if(!ok)
        return false;

    // Сервер принял транспорт, но отклонил применение (bad request,
    // no branch и т.п.): текст ошибки лежит в answer, а не в lastError.
    // Без этого сессия падает с пустым "Sync failed: "
    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return false;
    }

    return true;
}


bool SyncClient::pushBranch(const QString &host, quint16 port,
                            const QJsonObject &branch)
{    SYNC_TRACE_EXTRA(host+QStringLiteral(" ")+branch.value(QStringLiteral("id")).toString());

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/push-branch"),
                                      withAuth(QStringLiteral("POST /push-branch"), branch), ok);

    if(!ok)
        return false;

    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return false;
    }

    return true;
}


bool SyncClient::pushDelete(const QString &host, quint16 port,
                            const QString &id, qint64 deletedAt)
{    SYNC_TRACE_EXTRA(host+QStringLiteral(" ")+id);

    QJsonObject request;
    request[QStringLiteral("id")]=id;
    request[QStringLiteral("deletedAt")]=deletedAt;

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/push-delete"),
                                      withAuth(QStringLiteral("POST /push-delete"), request), ok);

    if(!ok)
        return false;

    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return false;
    }

    return true;
}


bool SyncClient::commit(const QString &host, quint16 port)
{    SYNC_TRACE_EXTRA(host);

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/commit"),
                                      withAuth(QStringLiteral("POST /commit"), QJsonObject()), ok);

    return ok && answer.value(QStringLiteral("ok")).toBool(false);
}


bool SyncClient::mergeByName(const QString &host, quint16 port,
                             const QStringList &branchIds)
{    SYNC_TRACE_EXTRA(host);

    QJsonArray arr;

    for(const QString &id : branchIds)
        arr.append(id);

    QJsonObject request;
    request[QStringLiteral("branchIds")]=arr;

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/merge-by-name"),
                                      withAuth(QStringLiteral("POST /merge-by-name"), request), ok);

    if(!ok)
        return false;

    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return false;
    }

    return true;
}


bool SyncClient::reset(const QString &host, quint16 port)
{    SYNC_TRACE_EXTRA(host);

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/reset"),
                                      withAuth(QStringLiteral("POST /reset"), QJsonObject()), ok);

    return ok && answer.value(QStringLiteral("ok")).toBool(false);
}

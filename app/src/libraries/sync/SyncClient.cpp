#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>
#include <QUrl>
#include <QJsonDocument>

#include "SyncClient.h"
#include "SyncHandshake.h"
#include "SyncDebug.h"
#include "models/appConfig/AppConfig.h"

#include <QDebug>


SyncClient::SyncClient(AppConfig *config) : config(config)
{
}


void SyncClient::setTimeoutMs(int ms)
{
    timeoutMs=ms;
}


QString SyncClient::lastError(void) const
{
    return lastErrorText;
}


QJsonObject SyncClient::postJson(const QString &host, quint16 port,
                                 const QString &path, const QJsonObject &body,
                                 bool &ok)
{
    ok=false;
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
{
    ok=false;
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


SyncHelloResult SyncClient::hello(const QString &host, quint16 port)
{
    SyncHelloResult result;
    result.serverTime=QDateTime::currentMSecsSinceEpoch();

    bool ok=false;
    const QJsonObject answer=postJson(
        host, port, QStringLiteral("/hello"),
        SyncHandshake::buildHello(config, result.serverTime), ok);

    if(!ok)
    {
        result.errorCode=QStringLiteral("network");
        result.errorText=lastErrorText;
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

    if(!result.ok)
    {
        result.errorCode=answer.value(QStringLiteral("error")).toString();
        result.errorText=answer.value(QStringLiteral("detail")).toString();
    }

    return result;
}


SyncManifest SyncClient::fetchManifest(const QString &host, quint16 port)
{
    bool ok=false;
    const QJsonObject answer=getJson(host, port,
                                     QStringLiteral("/manifest"), ok);

    if(!ok)
        return SyncManifest();

    return SyncManifest::fromJson(answer);
}


QJsonObject SyncClient::pullNote(const QString &host, quint16 port,
                                 const QString &id)
{
    QJsonObject request;
    request[QStringLiteral("id")]=id;

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/pull-note"),
                                      request, ok);

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
{
    QJsonObject request;
    request[QStringLiteral("id")]=id;

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/pull-branch"),
                                      request, ok);

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
{
    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/push-note"),
                                      note, ok);

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
{
    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/push-branch"),
                                      branch, ok);

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
{
    QJsonObject request;
    request[QStringLiteral("id")]=id;
    request[QStringLiteral("deletedAt")]=deletedAt;

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/push-delete"),
                                      request, ok);

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
{
    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/commit"),
                                      QJsonObject(), ok);

    return ok && answer.value(QStringLiteral("ok")).toBool(false);
}

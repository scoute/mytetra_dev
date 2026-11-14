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
#include "models/appConfig/AppConfig.h"


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

    return answer;
}

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
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>

#include "SyncClient.h"
#include "SyncHandshake.h"
#include "SyncPairing.h"
#include "SyncPairCrypto.h"
#include "SyncTls.h"
#include "SyncDataCrypto.h"
#include "SyncDebug.h"
#include "models/appConfig/AppConfig.h"

#include <QDebug>


SyncClient::SyncClient(AppConfig *config) : config(config)
{    SYNC_TRACE();

}


SyncClient::~SyncClient()
{
    delete network;
}


void SyncClient::setTimeoutMs(int ms)
{
    timeoutMs=ms;
}


void SyncClient::setTlsMode(TlsMode mode)
{
    tlsModeValue=static_cast<int>(mode);
}


SyncClient::TlsMode SyncClient::tlsMode(void) const
{
    return static_cast<TlsMode>(tlsModeValue);
}


void SyncClient::setTlsPin(const QString &fingerprint)
{
    tlsPinText=fingerprint.trimmed();
}


QString SyncClient::tlsPin(void) const
{
    return tlsPinText;
}


void SyncClient::setTlsTrustCallback(TlsTrustCallback callback)
{
    tlsTrustCallback=callback;
}


bool SyncClient::lastUsedTls(void) const
{
    return lastUsedTlsFlag;
}


QString SyncClient::lastTlsFingerprint(void) const
{
    return lastTlsFingerprintText;
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
{
    SYNC_TRACE_EXTRA(host);

    ok=false;
    lastTransportOkFlag=false;
    lastErrorText.clear();
    lastUsedTlsFlag=false;
    lastTlsFingerprintText.clear();

    if(tlsModeValue==0)
    {
        bool certError=false;
        return requestOnce(QStringLiteral("POST"), host, port, path, body,
                           QStringLiteral("http"), ok, certError);
    }

    bool certError=false;
    QJsonObject answer=requestOnce(QStringLiteral("POST"), host, port, path,
                                   body, QStringLiteral("https"), ok,
                                   certError);

    if(ok)
    {
        lastUsedTlsFlag=true;
        return answer;
    }

    // Сертификат предъявлен, но не доверен — откат на HTTP запрещён:
    // иначе MITM/self-signed молча понижал бы транспорт
    if(tlsModeValue==2 || certError)
    {
        if(tlsModeValue==2)
            lastErrorText=QStringLiteral("TLS is required: ")+lastErrorText;

        return QJsonObject();
    }

    // Prefer: HTTPS не поднялся и сертификат не предъявлялся —
    // вероятно, старый сервер без TLS. Осознанный откат с логом
    syncDbg(QStringLiteral("SyncClient: HTTPS failed (%1), trying HTTP")
                .arg(lastErrorText));
    lastErrorText.clear();
    lastTlsFingerprintText.clear();

    answer=requestOnce(QStringLiteral("POST"), host, port, path, body,
                       QStringLiteral("http"), ok, certError);
    lastUsedTlsFlag=false;
    return answer;
}


QJsonObject SyncClient::getJson(const QString &host, quint16 port,
                                const QString &path, bool &ok)
{
    SYNC_TRACE_EXTRA(host);

    ok=false;
    lastTransportOkFlag=false;
    lastErrorText.clear();
    lastUsedTlsFlag=false;
    lastTlsFingerprintText.clear();

    if(tlsModeValue==0)
    {
        bool certError=false;
        return requestOnce(QStringLiteral("GET"), host, port, path,
                           QJsonObject(), QStringLiteral("http"), ok,
                           certError);
    }

    bool certError=false;
    QJsonObject answer=requestOnce(QStringLiteral("GET"), host, port, path,
                                   QJsonObject(), QStringLiteral("https"),
                                   ok, certError);

    if(ok)
    {
        lastUsedTlsFlag=true;
        return answer;
    }

    if(tlsModeValue==2 || certError)
    {
        if(tlsModeValue==2)
            lastErrorText=QStringLiteral("TLS is required: ")+lastErrorText;

        return QJsonObject();
    }

    syncDbg(QStringLiteral("SyncClient: HTTPS failed (%1), trying HTTP")
                .arg(lastErrorText));
    lastErrorText.clear();
    lastTlsFingerprintText.clear();

    answer=requestOnce(QStringLiteral("GET"), host, port, path, QJsonObject(),
                       QStringLiteral("http"), ok, certError);
    lastUsedTlsFlag=false;
    return answer;
}


QJsonObject SyncClient::requestOnce(const QString &method, const QString &host,
                                    quint16 port, const QString &path,
                                    const QJsonObject &body,
                                    const QString &scheme,
                                    bool &ok, bool &certError)
{
    ok=false;
    certError=false;
    lastTransportOkFlag=false;

    // Один менеджер на сессию: он держит пул соединений и переиспользует
    // TCP/TLS с keep-alive сервером. Раньше на каждый pull-note создавался
    // новый QNetworkAccessManager и новый сокет (с TLS — новый handshake).
    if(network==nullptr)
        network=new QNetworkAccessManager();

    QNetworkRequest request;
    request.setUrl(QUrl(QStringLiteral("%1://%2:%3%4")
                        .arg(scheme, host).arg(port).arg(path)));

    QByteArray payload;

    if(method!=QStringLiteral("GET"))
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json; charset=utf-8"));
        payload=QJsonDocument(body).toJson(QJsonDocument::Compact);
    }

    QNetworkReply *reply=method==QStringLiteral("GET")
        ? network->get(request) : network->post(request, payload);

    QObject::connect(reply,
        QOverload<const QList<QSslError> &>::of(&QNetworkReply::sslErrors),
        reply,
        [this, reply, host, port, &certError](const QList<QSslError> &errors) {
        certError=true;

        const QSslCertificate cert=reply->sslConfiguration().peerCertificate();
        const QString fingerprint=SyncTls::certificateFingerprint(cert);
        lastTlsFingerprintText=fingerprint;

        if(cert.isNull())
        {
            lastErrorText=QStringLiteral("TLS: server did not present a "
                                         "certificate");
            return;
        }

        bool hardError=false;
        bool selfSigned=false;
        bool hostMismatch=false;

        for(const QSslError &error : errors)
        {
            switch(error.error())
            {
            case QSslError::CertificateExpired:
            case QSslError::CertificateNotYetValid:
            case QSslError::CertificateRevoked:
            case QSslError::CertificateSignatureFailed:
            case QSslError::UnspecifiedError:
                hardError=true;
                break;

            case QSslError::SelfSignedCertificate:
            case QSslError::SelfSignedCertificateInChain:
                selfSigned=true;
                break;

            case QSslError::HostNameMismatch:
                hostMismatch=true;
                break;

            default:
                break;
            }
        }

        if(hardError)
        {
            lastErrorText=QStringLiteral("TLS certificate error: %1")
                .arg(errors.isEmpty() ? QString()
                                      : errors.first().errorString());
            return;
        }

        QString pinned=tlsPinText;

        if(pinned.isEmpty() && config!=nullptr)
            pinned=config->get_syncTlsPin(host, port);

        if(!pinned.isEmpty())
        {
            if(pinned.compare(fingerprint, Qt::CaseInsensitive)==0)
            {
                reply->ignoreSslErrors();
                return;
            }

            lastErrorText=QStringLiteral(
                "TLS certificate does not match the pinned fingerprint. "
                "Got: %1").arg(fingerprint);
            return;
        }

        // CA-подписанный сертификат с чужим именем не даём обойти через
        // TOFU: это уже не «свой self-signed», а подозрительный хост
        if(hostMismatch && !selfSigned)
        {
            lastErrorText=QStringLiteral(
                "TLS host name mismatch for %1").arg(host);
            return;
        }

        if(tlsTrustCallback
           && tlsTrustCallback(host, port, cert, fingerprint))
        {
            if(config!=nullptr)
                config->set_syncTlsPin(host, port, fingerprint);

            tlsPinText=fingerprint;
            reply->ignoreSslErrors();
            return;
        }

        lastErrorText=QStringLiteral(
            "TLS certificate is not trusted. Fingerprint: %1")
            .arg(fingerprint);
    });

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished,
                     &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,
                     &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    if(!timer.isActive() && !reply->isFinished())
    {
        reply->abort();
        reply->deleteLater();

        if(lastErrorText.isEmpty())
            lastErrorText=QStringLiteral("Sync request timed out.");

        return QJsonObject();
    }

    timer.stop();

    if(reply->error()!=QNetworkReply::NoError)
    {
        if(lastErrorText.isEmpty())
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

    if(syncDebugEnabled())
    {
        const QJsonObject obj=doc.object();
        const QString err=obj.value(QStringLiteral("error")).toString();
        const QString warn=obj.value(QStringLiteral("warning")).toString();

        if(!err.isEmpty() || !warn.isEmpty())
            qDebug() << "SyncClient:" << path << "server says:"
                     << (err.isEmpty() ? QString()
                                       : QStringLiteral("error=")+err)
                     << (warn.isEmpty() ? QString()
                                        : QStringLiteral("warning=")+warn);
    }

    return doc.object();
}



QJsonObject SyncClient::postData(const QString &host, quint16 port,
                                 const QString &route, const QString &path,
                                 const QJsonObject &body, bool &ok)
{
    SyncDataCrypto::Context ctx;
    QJsonObject request;

    if(peerEnc && !peerSessionText.isEmpty() && !peerSecretText.isEmpty()
       && !peerDeviceIdText.isEmpty())
    {
        ctx.encrypted=true;
        ctx.clientId=config->get_syncDeviceId();
        ctx.serverId=peerDeviceIdText;
        ctx.session=peerSessionText;
        ctx.ts=QDateTime::currentMSecsSinceEpoch();
        ctx.route=route;

        request=SyncDataCrypto::sealRequest(
            peerSecretText, ctx.clientId, ctx.serverId, ctx.session,
            route, ctx.ts, body);

        if(request.isEmpty())
        {
            lastErrorText=QStringLiteral("Cannot encrypt sync request");
            ok=false;
            lastTransportOkFlag=false;
            return QJsonObject();
        }
    }
    else
        request=withAuth(route, body);

    QJsonObject answer=postJson(host, port, path, request, ok);

    if(!ok)
        return QJsonObject();

    if(!ctx.encrypted)
        return answer;

    if(answer.contains(QStringLiteral("_enc")))
    {
        QJsonObject inner;
        QString error;

        if(!SyncDataCrypto::openResponse(peerSecretText, ctx, answer,
                                         inner, error))
        {
            lastErrorText=error;
            ok=false;
            lastTransportOkFlag=false;
            return QJsonObject();
        }

        return inner;
    }

    // Ошибку сервер может вернуть plain (auth-failed/not-paired):
    // секрета для шифрования ошибки у него нет/не нужно
    if(answer.contains(QStringLiteral("error")))
        return answer;

    lastErrorText=QStringLiteral(
        "Server returned an unencrypted sync response");
    ok=false;
    lastTransportOkFlag=false;
    return QJsonObject();
}


SyncHelloResult SyncClient::hello(const QString &host, quint16 port,
                                    bool pairRequest)
{    SYNC_TRACE_EXTRA(host);

    SyncHelloResult result;
    result.serverTime=QDateTime::currentMSecsSinceEpoch();
    const qint64 clientTime=result.serverTime;
    const QString sessionB64=SyncPairCrypto::toBase64(
        SyncPairCrypto::randomBytes(16));

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
            SyncHandshake::buildHello(config, clientTime,
                                     pairRequest, candidate, sessionB64), ok);

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
    result.pair2=answer.value(QStringLiteral("pair2")).toInt(0)!=0;
    result.pairPending=answer.value(QStringLiteral("pairPending"))
                            .toBool(false);
    result.pairCodeRequired=answer.value(QStringLiteral("pairCodeRequired"))
                                  .toBool(false);
    result.pairCodeLocked=answer.value(QStringLiteral("pairCodeLocked"))
                                .toBool(false);
    result.pairCodeExpiresAt=answer.value(QStringLiteral("pairCodeExpiresAt"))
                                   .toVariant().toLongLong();
    result.pairCodeUntil=answer.value(QStringLiteral("pairCodeUntil"))
                               .toVariant().toLongLong();
    result.enc=answer.value(QStringLiteral("enc")).toInt(0)==1;
    result.session=answer.value(QStringLiteral("session")).toString();
    result.helloMac=answer.value(QStringLiteral("helloMac")).toString();

    if(!result.ok)
    {
        result.errorCode=answer.value(QStringLiteral("error")).toString();
        result.errorText=answer.value(QStringLiteral("detail")).toString();
        return result;
    }

    // --- end-to-end enc (sync/3) ---
    const bool wasEnc=config!=nullptr
        && config->get_syncDeviceEnc(result.remoteDeviceId);

    if(result.enc)
    {
        if(result.session!=sessionB64)
        {
            result.ok=false;
            result.errorCode=QStringLiteral("auth-failed");
            result.errorText=QStringLiteral(
                "Server hello session mismatch");
            return result;
        }

        if(result.helloMac.isEmpty() || usedSecret.isEmpty())
        {
            result.ok=false;
            result.errorCode=QStringLiteral("auth-failed");
            result.errorText=QStringLiteral(
                "Server did not authenticate the hello response");
            return result;
        }

        const QByteArray macInput=SyncDataCrypto::helloResponseMacInput(
            config->get_syncDeviceId(), result.remoteDeviceId,
            result.session, clientTime, result.serverTime,
            result.ok, result.paired, result.remoteGroupId,
            result.remoteDeviceName, result.pairCodeRequired,
            result.pairCodeLocked, result.pairCodeExpiresAt,
            result.warning,
            answer.value(QStringLiteral("error")).toString(),
            answer.value(QStringLiteral("detail")).toString());

        const QString expected=SyncDataCrypto::helloMac(usedSecret,
                                                        macInput);

        if(!SyncPairCrypto::constantTimeEquals(result.helloMac.toLatin1(),
                                               expected.toLatin1()))
        {
            result.ok=false;
            result.errorCode=QStringLiteral("auth-failed");
            result.errorText=QStringLiteral(
                "Server hello MAC mismatch");
            return result;
        }

        peerEnc=true;
        peerSessionText=sessionB64;
        peerDeviceIdText=result.remoteDeviceId;

        if(config!=nullptr)
            config->set_syncDeviceEnc(result.remoteDeviceId, true);
    }
    else
    {
        peerEnc=false;
        peerSessionText.clear();
        peerDeviceIdText=result.remoteDeviceId;

        // Липкий enc: уже шифровались с этим устройством, а сервер
        // вдруг отвечает plain — не откатываемся молча
        if(wasEnc)
        {
            result.ok=false;
            result.errorCode=QStringLiteral("enc-downgrade");
            result.errorText=QStringLiteral(
                "This device previously used encrypted sync, but the "
                "server stopped offering it. Check the server or re-pair.");
            return result;
        }
    }

    return result;
}


QString SyncClient::pairWithCode(const QString &host, quint16 port,
                                 const QString &serverId, const QString &code,
                                 QString *errorCode)
{
    const auto fail=[&](const QString &error) -> QString {
        if(errorCode!=nullptr)
            *errorCode=error;

        return QString();
    };

    if(config==nullptr || serverId.isEmpty())
        return fail(QStringLiteral("pair-bad-request"));

    const QString normalized=SyncPairCrypto::normalizeCode(code);

    if(!SyncPairCrypto::isValidCode(normalized))
        return fail(QStringLiteral("pair-code-invalid"));

    QByteArray clientPriv;
    QByteArray clientPub;

    if(!SyncPairCrypto::generateX25519(clientPriv, clientPub))
        return fail(QStringLiteral("pair-crypto"));

    const QByteArray clientNonce=SyncPairCrypto::randomBytes(16);

    const QJsonObject request=SyncPairing::buildBegin(
        config->get_syncDeviceId(), config->get_syncDeviceName(),
        serverId, normalized, clientPub, clientNonce);

    if(request.contains(QStringLiteral("error")))
        return fail(request.value(QStringLiteral("error")).toString());

    bool ok=false;
    const QJsonObject answer=postJson(host, port,
                                      QStringLiteral("/pair-begin"),
                                      request, ok);

    if(!ok)
        return fail(QStringLiteral("network"));

    QString error;
    const QString secret=SyncPairing::finishBegin(
        config->get_syncDeviceId(), serverId, normalized,
        clientPriv, clientPub, clientNonce, answer, &error);

    if(secret.isEmpty())
        return fail(error.isEmpty() ? QStringLiteral("pair-bad-response")
                                    : error);

    if(errorCode!=nullptr)
        errorCode->clear();

    return secret;
}


QString SyncClient::lastManifestScope(void) const
{
    return manifestScopeText;
}


SyncManifest SyncClient::fetchManifest(const QString &host, quint16 port,
                                       const QString &scopeRootId)
{    SYNC_TRACE_EXTRA(host);

    manifestScopeText.clear();

    QJsonObject request;

    if(!scopeRootId.isEmpty() && scopeRootId!=QStringLiteral("0"))
        request[QStringLiteral("scope")]=scopeRootId;

    bool ok=false;
    const QJsonObject answer=postData(
        host, port, QStringLiteral("POST /manifest"),
        QStringLiteral("/manifest"), request, ok);

    if(!ok)
        return SyncManifest();

    manifestScopeText=answer.value(QStringLiteral("scope")).toString();

    // Сервер отклонил (not-paired/auth-failed): пустой манифест без ошибки
    // в lastError звал бы заливку всего локального — фиксируем отказ
    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return SyncManifest();
    }

    return SyncManifest::fromJson(answer);
}


bool SyncClient::fetchTreeManifest(const QString &host, quint16 port,
                                   const SyncManifest &local,
                                   SyncManifest &remote)
{
    SYNC_TRACE_EXTRA(host);

    QJsonObject hashes;

    for(auto it=local.branches.constBegin(); it!=local.branches.constEnd(); ++it)
        if(!it.value().directHash.isEmpty())
            hashes.insert(it.key(), it.value().directHash);

    QJsonObject request;
    request[QStringLiteral("directHashes")]=hashes;

    bool ok=false;
    const QJsonObject answer=postData(
        host, port, QStringLiteral("POST /tree"),
        QStringLiteral("/tree"), request, ok);

    if(!ok)
        return false;

    if(answer.contains(QStringLiteral("error")))
    {
        lastErrorText=answer.value(QStringLiteral("error")).toString();
        return false;
    }

    SyncManifest incoming=SyncManifest::fromJson(answer);

    // Пустой ответ не должен выглядеть как «на сервере всё удалили»
    if(incoming.branches.isEmpty() && !local.branches.isEmpty())
        return false;

    QMap<QString, QList<SyncNoteInfo>> localByBranch;

    for(auto it=local.notes.constBegin(); it!=local.notes.constEnd(); ++it)
        localByBranch[it.value().branchId] << it.value();

    QMap<QString, SyncNoteInfo> mergedNotes;
    QSet<QString> equalSubtrees;

    for(auto it=incoming.branches.constBegin();
        it!=incoming.branches.constEnd(); ++it)
    {
        const QString id=it.key();
        const SyncBranchInfo &branch=it.value();
        auto localBranch=local.branches.constFind(id);

        if(localBranch!=local.branches.constEnd()
           && !branch.subtreeHash.isEmpty()
           && localBranch.value().subtreeHash==branch.subtreeHash)
        {
            // Всё поддерево идентично — не тянем и не сравниваем
            equalSubtrees.insert(id);
            continue;
        }

        const bool sameDirect=localBranch!=local.branches.constEnd()
            && !branch.directHash.isEmpty()
            && localBranch.value().directHash==branch.directHash;

        if(sameDirect)
        {
            for(const SyncNoteInfo &note : localByBranch.value(id))
                mergedNotes.insert(note.id, note);
        }
        else
        {
            for(auto nit=incoming.notes.constBegin();
                nit!=incoming.notes.constEnd(); ++nit)
                if(nit.value().branchId==id)
                    mergedNotes.insert(nit.key(), nit.value());
        }
    }

    incoming.notes=mergedNotes;
    incoming.skipSubtreeIds=equalSubtrees;
    remote=incoming;
    return true;
}


QJsonObject SyncClient::pullNotes(const QString &host, quint16 port,
                                  const QJsonArray &items)
{    SYNC_TRACE_EXTRA(host);

    QJsonObject request;
    request[QStringLiteral("items")]=items;

    bool ok=false;
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /pull-notes"),
                                      QStringLiteral("/pull-notes"),
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


QJsonObject SyncClient::pullNote(const QString &host, quint16 port,
                                 const QString &id, const QString &branchId)
{    SYNC_TRACE_EXTRA(host);

    QJsonObject request;
    request[QStringLiteral("id")]=id;

    if(!branchId.isEmpty())
        request[QStringLiteral("branch")]=branchId;

    bool ok=false;
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /pull-note"),
                                      QStringLiteral("/pull-note"), request, ok);

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
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /pull-branch"),
                                      QStringLiteral("/pull-branch"), request, ok);

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
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /push-note"),
                                      QStringLiteral("/push-note"), note, ok);

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
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /push-branch"),
                                      QStringLiteral("/push-branch"), branch, ok);

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
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /push-delete"),
                                      QStringLiteral("/push-delete"), request, ok);

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
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /commit"),
                                      QStringLiteral("/commit"), QJsonObject(), ok);

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
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /merge-by-name"),
                                      QStringLiteral("/merge-by-name"), request, ok);

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
    const QJsonObject answer=postData(host, port,
                                      QStringLiteral("POST /reset"),
                                      QStringLiteral("/reset"), QJsonObject(), ok);

    return ok && answer.value(QStringLiteral("ok")).toBool(false);
}

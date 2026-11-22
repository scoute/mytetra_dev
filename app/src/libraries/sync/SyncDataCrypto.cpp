#include <QJsonDocument>
#include <QMap>

#include "SyncDataCrypto.h"
#include "SyncPairCrypto.h"
#include "SyncIdentity.h"
#include "models/appConfig/AppConfig.h"


namespace SyncDataCrypto
{

namespace
{
constexpr qint64 kMaxTsSkewMs=120000;

QMap<QString, QString> &activeSessions(void)
{
    static QMap<QString, QString> sessions;
    return sessions;
}


QByteArray joinFields(const QByteArray &prefix,
                      const QList<QByteArray> &fields)
{
    QByteArray out=prefix;

    for(const QByteArray &field : fields)
    {
        out.append('\0');
        out.append(field);
    }

    return out;
}


QByteArray num(qint64 value)
{
    return QByteArray::number(value);
}


QByteArray flag(bool value)
{
    return value ? QByteArrayLiteral("1") : QByteArrayLiteral("0");
}


QByteArray canonical(const QJsonObject &obj)
{
    // QJsonObject хранит ключи отсортированными — сериализация
    // детерминирована. Для направленных ключей/AAD одинаково на обеих
    // сторонах (Kotlin строит тот же фиксированный набор полей,
    // а тело шифруется как есть и только парсится на приёме).
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}


QByteArray deriveKey(const QString &secretHex, const QByteArray &salt,
                     const QString &direction, const QString &sessionB64,
                     const QString &clientId, const QString &serverId)
{
    const QByteArray secret=QByteArray::fromHex(secretHex.toLatin1());

    if(secret.size()!=SyncPairCrypto::KeyBytes)
        return QByteArray();

    const QByteArray info=joinFields(QByteArrayLiteral("mytetra-data-key-v1"),
        {direction.toUtf8(), sessionB64.toUtf8(),
         clientId.toUtf8(), serverId.toUtf8()});

    return SyncPairCrypto::hkdfSha256(secret, salt, info,
                                      SyncPairCrypto::KeyBytes);
}


QByteArray requestAad(const Context &ctx)
{
    return joinFields(QByteArrayLiteral("mytetra-data-req-v1"),
        {ctx.route.toUtf8(), ctx.clientId.toUtf8(), ctx.serverId.toUtf8(),
         ctx.session.toUtf8(), num(ctx.ts)});
}


QByteArray responseAad(const Context &ctx)
{
    return joinFields(QByteArrayLiteral("mytetra-data-resp-v1"),
        {ctx.route.toUtf8(), ctx.clientId.toUtf8(), ctx.serverId.toUtf8(),
         ctx.session.toUtf8(), num(ctx.ts)});
}


QString secretForDevice(AppConfig *config, const QString &deviceId,
                        QStringList *all)
{
    if(config==nullptr || deviceId.isEmpty())
        return QString();

    const QStringList candidates=SyncIdentity::candidateSecrets(config, deviceId);

    if(all!=nullptr)
        *all=candidates;

    return candidates.isEmpty() ? QString() : candidates.first();
}
}


QByteArray helloRequestChallenge(const QString &clientId, qint64 clientTime,
                                 const QString &sessionB64,
                                 const QString &groupId,
                                 const QString &deviceName,
                                 bool pairRequest, bool enc)
{
    return joinFields(QByteArrayLiteral("mytetra-hello-req-v1"),
        {clientId.toUtf8(), num(clientTime), sessionB64.toUtf8(),
         groupId.toUtf8(), deviceName.toUtf8(),
         flag(pairRequest), flag(enc)});
}


QByteArray helloResponseMacInput(const QString &clientId,
                                 const QString &serverId,
                                 const QString &sessionB64,
                                 qint64 clientTime, qint64 serverTime,
                                 bool ok, bool paired,
                                 const QString &groupId,
                                 const QString &deviceName,
                                 bool pairCodeRequired,
                                 bool pairCodeLocked,
                                 qint64 pairCodeExpiresAt,
                                 const QString &warning,
                                 const QString &error,
                                 const QString &detail)
{
    return joinFields(QByteArrayLiteral("mytetra-hello-resp-v1"),
        {clientId.toUtf8(), serverId.toUtf8(), sessionB64.toUtf8(),
         num(clientTime), num(serverTime), flag(ok), flag(paired),
         groupId.toUtf8(), deviceName.toUtf8(),
         flag(pairCodeRequired), flag(pairCodeLocked),
         num(pairCodeExpiresAt), warning.toUtf8(),
         error.toUtf8(), detail.toUtf8()});
}


QString helloMac(const QString &secretHex, const QByteArray &input)
{
    const QByteArray secret=QByteArray::fromHex(secretHex.toLatin1());

    if(secret.size()!=SyncPairCrypto::KeyBytes || input.isEmpty())
        return QString();

    return SyncPairCrypto::toHex(
        SyncPairCrypto::hmacSha256(secret, input));
}


QByteArray dataKeyForTest(const QString &secretHex, const QString &direction,
                          const QString &sessionB64,
                          const QString &clientId,
                          const QString &serverId)
{
    return deriveKey(secretHex, QByteArrayLiteral("mytetra-data-v1"),
                     direction, sessionB64, clientId, serverId);
}


QJsonObject sealRequest(const QString &secretHex, const QString &clientId,
                        const QString &serverId, const QString &sessionB64,
                        const QString &route, qint64 ts,
                        const QJsonObject &body)
{
    const QByteArray key=deriveKey(secretHex,
        QByteArrayLiteral("mytetra-data-v1"),
        QStringLiteral("c2s"), sessionB64, clientId, serverId);

    if(key.isEmpty() || sessionB64.isEmpty())
        return QJsonObject();

    Context ctx;
    ctx.encrypted=true;
    ctx.clientId=clientId;
    ctx.serverId=serverId;
    ctx.session=sessionB64;
    ctx.ts=ts;
    ctx.route=route;

    const QByteArray plain=canonical(body);
    const QByteArray blob=SyncPairCrypto::aeadEncrypt(key, plain,
                                                      requestAad(ctx));

    if(blob.isEmpty())
        return QJsonObject();

    QJsonObject envelope;
    envelope[QStringLiteral("_enc")]=1;
    envelope[QStringLiteral("_dev")]=clientId;
    envelope[QStringLiteral("_ts")]=ts;
    envelope[QStringLiteral("_s")]=sessionB64;
    envelope[QStringLiteral("_ct")]=SyncPairCrypto::toBase64(blob);
    return envelope;
}


bool openRequestWithSecret(const QString &secretHex, const QString &serverId,
                           const QString &route, const QJsonObject &envelope,
                           qint64 now, QJsonObject &body, Context &ctx,
                           QString &error)
{
    const auto fail=[&](const QString &text) -> bool {
        error=text;
        return false;
    };

    if(envelope.value(QStringLiteral("_enc")).toInt(0)!=1)
        return fail(QStringLiteral("not encrypted"));

    const QString clientId=envelope.value(QStringLiteral("_dev")).toString();
    const qint64 ts=envelope.value(QStringLiteral("_ts"))
                        .toVariant().toLongLong();
    const QString session=envelope.value(QStringLiteral("_s")).toString();
    const QByteArray blob=SyncPairCrypto::fromBase64(
        envelope.value(QStringLiteral("_ct")).toString());

    if(clientId.isEmpty() || session.isEmpty() || blob.isEmpty())
        return fail(QStringLiteral("missing encrypted fields"));

    if(qAbs(now-ts)>kMaxTsSkewMs)
        return fail(QStringLiteral("stale request"));

    Context candidate;
    candidate.encrypted=true;
    candidate.secret=secretHex;
    candidate.clientId=clientId;
    candidate.serverId=serverId;
    candidate.session=session;
    candidate.ts=ts;
    candidate.route=route;

    const QByteArray key=deriveKey(secretHex,
        QByteArrayLiteral("mytetra-data-v1"),
        QStringLiteral("c2s"), session, clientId, serverId);

    if(key.isEmpty())
        return fail(QStringLiteral("bad secret"));

    QByteArray plain;

    if(!SyncPairCrypto::aeadDecrypt(key, blob, requestAad(candidate), plain))
        return fail(QStringLiteral("bad encrypted body"));

    QJsonParseError parseError;
    const QJsonDocument doc=QJsonDocument::fromJson(plain, &parseError);

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
        return fail(QStringLiteral("bad encrypted json"));

    body=doc.object();
    ctx=candidate;
    error.clear();
    return true;
}


bool openRequest(AppConfig *config, const QString &serverId,
                 const QString &route, const QJsonObject &envelope,
                 qint64 now, QJsonObject &body, Context &ctx, QString &error)
{
    const QString clientId=envelope.value(QStringLiteral("_dev")).toString();
    const QString session=envelope.value(QStringLiteral("_s")).toString();

    if(clientId.isEmpty() || session.isEmpty())
    {
        error=QStringLiteral("missing encrypted fields");
        return false;
    }

    if(!sessionActive(clientId, session))
    {
        error=QStringLiteral("stale session");
        return false;
    }

    QStringList secrets;
    secretForDevice(config, clientId, &secrets);

    if(secrets.isEmpty())
    {
        error=QStringLiteral("unknown device, pair it first");
        return false;
    }

    QString lastError;

    for(const QString &secret : secrets)
    {
        Context candidate;
        QString candidateError;

        if(openRequestWithSecret(secret, serverId, route, envelope, now,
                                 body, candidate, candidateError))
        {
            ctx=candidate;
            ctx.secret=secret;
            error.clear();
            return true;
        }

        lastError=candidateError;
    }

    error=lastError.isEmpty() ? QStringLiteral("bad proof") : lastError;
    return false;
}


QJsonObject sealResponse(const Context &ctx, const QJsonObject &body)
{
    if(!ctx.encrypted || ctx.secret.isEmpty())
        return body;

    const QByteArray key=deriveKey(ctx.secret,
        QByteArrayLiteral("mytetra-data-v1"),
        QStringLiteral("s2c"), ctx.session, ctx.clientId, ctx.serverId);

    if(key.isEmpty())
        return QJsonObject();

    const QByteArray plain=canonical(body);
    const QByteArray blob=SyncPairCrypto::aeadEncrypt(key, plain,
                                                      responseAad(ctx));

    if(blob.isEmpty())
        return QJsonObject();

    QJsonObject envelope;
    envelope[QStringLiteral("_enc")]=1;
    envelope[QStringLiteral("_dev")]=ctx.serverId;
    envelope[QStringLiteral("_ts")]=ctx.ts;
    envelope[QStringLiteral("_s")]=ctx.session;
    envelope[QStringLiteral("_ct")]=SyncPairCrypto::toBase64(blob);
    return envelope;
}


bool openResponse(const QString &secretHex, const Context &ctx,
                  const QJsonObject &envelope, QJsonObject &body,
                  QString &error)
{
    const auto fail=[&](const QString &text) -> bool {
        error=text;
        return false;
    };

    if(envelope.value(QStringLiteral("_enc")).toInt(0)!=1)
        return fail(QStringLiteral("response is not encrypted"));

    if(envelope.value(QStringLiteral("_dev")).toString()!=ctx.serverId)
        return fail(QStringLiteral("wrong server id in response"));

    if(envelope.value(QStringLiteral("_s")).toString()!=ctx.session)
        return fail(QStringLiteral("wrong session in response"));

    if(envelope.value(QStringLiteral("_ts")).toVariant().toLongLong()!=ctx.ts)
        return fail(QStringLiteral("wrong ts in response"));

    const QByteArray blob=SyncPairCrypto::fromBase64(
        envelope.value(QStringLiteral("_ct")).toString());

    if(blob.isEmpty())
        return fail(QStringLiteral("empty encrypted response"));

    const QByteArray key=deriveKey(secretHex,
        QByteArrayLiteral("mytetra-data-v1"),
        QStringLiteral("s2c"), ctx.session, ctx.clientId, ctx.serverId);

    if(key.isEmpty())
        return fail(QStringLiteral("bad secret"));

    QByteArray plain;

    if(!SyncPairCrypto::aeadDecrypt(key, blob, responseAad(ctx), plain))
        return fail(QStringLiteral("bad encrypted response"));

    QJsonParseError parseError;
    const QJsonDocument doc=QJsonDocument::fromJson(plain, &parseError);

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
        return fail(QStringLiteral("bad encrypted json"));

    body=doc.object();
    error.clear();
    return true;
}


void setActiveSession(const QString &deviceId, const QString &sessionB64)
{
    if(deviceId.isEmpty() || sessionB64.isEmpty())
        return;

    activeSessions().insert(deviceId, sessionB64);
}


void clearActiveSession(const QString &deviceId)
{
    activeSessions().remove(deviceId);
}


void clearAllSessions(void)
{
    activeSessions().clear();
}


bool sessionActive(const QString &deviceId, const QString &sessionB64)
{
    return !deviceId.isEmpty() && !sessionB64.isEmpty()
           && activeSessions().value(deviceId)==sessionB64;
}

} // namespace SyncDataCrypto

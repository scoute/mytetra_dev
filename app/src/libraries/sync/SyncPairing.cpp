#include <QDebug>
#include <QMap>

#include "SyncPairing.h"
#include "SyncPairCrypto.h"


namespace SyncPairing
{

namespace
{

struct CodeState
{
    QString code;
    QString peerIp;
    qint64 createdAt=0;
    qint64 expiresAt=0;
    int failed=0;
    qint64 lockedUntil=0;
};

QMap<QString, CodeState> &codes(void)
{
    static QMap<QString, CodeState> state;
    return state;
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


QByteArray beginMacInput(const QString &serverId, const QString &clientId,
                         const QByteArray &clientEph,
                         const QByteArray &clientNonce)
{
    return joinFields(QByteArrayLiteral("mytetra-pair-begin-v2"),
                      {serverId.toUtf8(), clientId.toUtf8(),
                       clientEph.toBase64(), clientNonce.toBase64()});
}


QByteArray transcript(const QString &serverId, const QString &clientId,
                      const QByteArray &clientEph,
                      const QByteArray &serverEph,
                      const QByteArray &clientNonce,
                      const QByteArray &serverNonce)
{
    return joinFields(QByteArrayLiteral("mytetra-pair-v2"),
                      {serverId.toUtf8(), clientId.toUtf8(),
                       clientEph.toBase64(), serverEph.toBase64(),
                       clientNonce.toBase64(), serverNonce.toBase64()});
}


QByteArray finishMacInput(const QByteArray &info,
                          const QByteArray &cipher)
{
    return joinFields(QByteArrayLiteral("mytetra-pair-finish-v2"),
                      {info, cipher.toBase64()});
}


bool isHex64(const QByteArray &text)
{
    if(text.size()!=64)
        return false;

    for(const char c : text)
        if(!((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F')))
            return false;

    return true;
}


QJsonObject errorAnswer(const QString &code, const QString &detail=QString())
{
    QJsonObject answer;
    answer[QStringLiteral("ok")]=false;
    answer[QStringLiteral("error")]=code;

    if(!detail.isEmpty())
        answer[QStringLiteral("detail")]=detail;

    return answer;
}


void wipe(QByteArray &data)
{
    data.fill(0);
}

} // namespace


QString ensureCode(const QString &peerId, qint64 now,
                   const QString &peerIp,
                   bool bindIp,
                   qint64 *expiresAt, qint64 *lockedUntil)
{
    if(peerId.isEmpty())
        return QString();

    // Ограничение памяти: поток случайных deviceId с pairRequest не
    // должен копить коды бесконечно. Самые старые вытесняем
    if(!codes().contains(peerId) && codes().size()>=32)
    {
        QString oldest;
        qint64 oldestAt=0;

        for(auto it=codes().constBegin(); it!=codes().constEnd(); ++it)
            if(oldest.isEmpty() || it.value().createdAt<oldestAt)
            {
                oldest=it.key();
                oldestAt=it.value().createdAt;
            }

        if(!oldest.isEmpty())
            codes().remove(oldest);
    }

    CodeState &state=codes()[peerId];

    // Заблокирован после серии неверных кодов: новый код не обходит лимит
    if(state.lockedUntil>now)
    {
        if(expiresAt!=nullptr)
            *expiresAt=state.lockedUntil;

        if(lockedUntil!=nullptr)
            *lockedUntil=state.lockedUntil;

        return QString();
    }

    if(!state.code.isEmpty() && now<state.expiresAt)
    {
        // Настройка привязки могла включиться/выключиться на живом коде:
        // включаем привязку к IP текущего pairRequest, выключение снимает
        if(bindIp && state.peerIp.isEmpty())
            state.peerIp=peerIp;

        if(!bindIp)
            state.peerIp.clear();

        if(expiresAt!=nullptr)
            *expiresAt=state.expiresAt;

        if(lockedUntil!=nullptr)
            *lockedUntil=0;

        return state.code;
    }

    const QString code=SyncPairCrypto::generateCode();

    if(code.isEmpty())
        return QString();

    state.code=code;
    state.peerIp=bindIp ? peerIp : QString();
    state.createdAt=now;
    state.expiresAt=now+CodeTtlMs;
    state.failed=0;
    state.lockedUntil=0;

    if(expiresAt!=nullptr)
        *expiresAt=state.expiresAt;

    if(lockedUntil!=nullptr)
        *lockedUntil=0;

    qDebug() << "SyncPairing: code generated for" << peerId
             << "ttl" << CodeTtlMs << "ms";
    return code;
}


bool currentCode(const QString &peerId, qint64 now,
                 QString *code, qint64 *expiresAt)
{
    if(peerId.isEmpty() || !codes().contains(peerId))
        return false;

    const CodeState &state=codes().value(peerId);

    if(now>=state.expiresAt || state.lockedUntil>now)
    {
        if(now>=state.expiresAt)
            codes().remove(peerId);

        return false;
    }

    if(code!=nullptr)
        *code=state.code;

    if(expiresAt!=nullptr)
        *expiresAt=state.expiresAt;

    return true;
}


void consumeCode(const QString &peerId)
{
    codes().remove(peerId);
}


void clearCode(const QString &peerId)
{
    codes().remove(peerId);
}


void clearAll(void)
{
    codes().clear();
}


QJsonObject buildBegin(const QString &clientId, const QString &clientName,
                       const QString &serverId, const QString &code,
                       const QByteArray &clientEphPub,
                       const QByteArray &clientNonce)
{
    if(clientId.isEmpty() || clientName.isEmpty() || serverId.isEmpty()
       || clientEphPub.size()!=SyncPairCrypto::EphBytes
       || clientNonce.size()!=16)
        return errorAnswer(QStringLiteral("pair-bad-request"));

    const QString normalized=SyncPairCrypto::normalizeCode(code);

    if(!SyncPairCrypto::isValidCode(normalized))
        return errorAnswer(QStringLiteral("pair-code-invalid"));

    const QByteArray key=SyncPairCrypto::codeKey(normalized);

    if(key.isEmpty())
        return errorAnswer(QStringLiteral("pair-crypto"));

    const QByteArray mac=SyncPairCrypto::hmacSha256(
        key,
        beginMacInput(serverId, clientId, clientEphPub, clientNonce));

    QJsonObject request;
    request[QStringLiteral("proto")]=QStringLiteral("sync/1");
    request[QStringLiteral("pair2")]=1;
    request[QStringLiteral("deviceId")]=clientId;
    request[QStringLiteral("deviceName")]=clientName;
    request[QStringLiteral("serverId")]=serverId;
    request[QStringLiteral("clientEph")]=SyncPairCrypto::toBase64(clientEphPub);
    request[QStringLiteral("clientNonce")]=SyncPairCrypto::toBase64(clientNonce);
    request[QStringLiteral("codeMac")]=SyncPairCrypto::toHex(mac);
    return request;
}


QJsonObject handleBegin(const QString &serverId, const QJsonObject &request,
                        qint64 now, const QString &peerIp, bool bindIp,
                        const PendingGetter &getPending,
                        const PendingSetter &setPending)
{
    if(serverId.isEmpty() || !getPending || !setPending)
        return errorAnswer(QStringLiteral("pair-crypto"));

    const QString proto=request.value(QStringLiteral("proto")).toString();
    const QString requestedServer=request.value(QStringLiteral("serverId")).toString();
    const QString peerId=request.value(QStringLiteral("deviceId")).toString();

    if(proto!=QStringLiteral("sync/1")
       || request.value(QStringLiteral("pair2")).toInt(0)!=1)
        return errorAnswer(QStringLiteral("pair-unsupported"));

    if(requestedServer!=serverId)
        return errorAnswer(QStringLiteral("pair-wrong-server"));

    if(peerId.isEmpty() || peerId==serverId)
        return errorAnswer(QStringLiteral("pair-bad-request"));

    const QByteArray clientEph=SyncPairCrypto::fromBase64(
        request.value(QStringLiteral("clientEph")).toString());
    const QByteArray clientNonce=SyncPairCrypto::fromBase64(
        request.value(QStringLiteral("clientNonce")).toString());
    const QByteArray codeMac=SyncPairCrypto::fromHex(
        request.value(QStringLiteral("codeMac")).toString());

    if(clientEph.size()!=SyncPairCrypto::EphBytes
       || clientNonce.size()!=16
       || codeMac.size()!=SyncPairCrypto::MacBytes)
        return errorAnswer(QStringLiteral("pair-bad-request"));

    if(!codes().contains(peerId))
        return errorAnswer(QStringLiteral("pair-code-missing"));

    CodeState &state=codes()[peerId];

    if(state.lockedUntil>now)
        return errorAnswer(QStringLiteral("pair-rate-limited"));

    if(now>=state.expiresAt)
    {
        codes().remove(peerId);
        return errorAnswer(QStringLiteral("pair-code-expired"));
    }

    // Привязка к IP: код, выданный одному адресу, не принимаем с другого.
    // Считаем это неудачной попыткой, чтобы не давать бесплатный перебор
    if(bindIp && !state.peerIp.isEmpty() && peerIp!=state.peerIp)
    {
        ++state.failed;

        if(state.failed>=MaxFailedAttempts)
            state.lockedUntil=now+LockoutMs;

        qDebug() << "SyncPairing: code IP mismatch for" << peerId
                 << "expected" << state.peerIp << "got" << peerIp;
        return errorAnswer(QStringLiteral("pair-ip-mismatch"));
    }

    const QByteArray key=SyncPairCrypto::codeKey(state.code);

    if(key.isEmpty())
        return errorAnswer(QStringLiteral("pair-crypto"));

    const QByteArray expected=SyncPairCrypto::hmacSha256(
        key, beginMacInput(serverId, peerId, clientEph, clientNonce));

    if(!SyncPairCrypto::constantTimeEquals(codeMac, expected))
    {
        ++state.failed;

        if(state.failed>=MaxFailedAttempts)
        {
            state.lockedUntil=now+LockoutMs;
            qDebug() << "SyncPairing: peer" << peerId
                     << "locked after" << state.failed << "bad codes";
            return errorAnswer(QStringLiteral("pair-rate-limited"));
        }

        qDebug() << "SyncPairing: bad code from" << peerId
                 << "attempt" << state.failed;
        return errorAnswer(QStringLiteral("pair-code-invalid"));
    }

    state.failed=0;
    state.lockedUntil=0;

    // Долговременный секрет: при повторе (потеряли ответ) отдаём тот же
    // pending, а не новый, иначе клиент сохранит один, а сервер ждёт другой
    QString pendingSecret=getPending(peerId);

    if(pendingSecret.isEmpty())
    {
        pendingSecret=SyncPairCrypto::randomSecretHex();
        setPending(peerId, pendingSecret);
    }

    QByteArray serverPriv;
    QByteArray serverPub;

    if(!SyncPairCrypto::generateX25519(serverPriv, serverPub))
        return errorAnswer(QStringLiteral("pair-crypto"));

    const QByteArray serverNonce=SyncPairCrypto::randomBytes(16);

    QByteArray shared;

    if(!SyncPairCrypto::x25519(serverPriv, clientEph, shared))
        return errorAnswer(QStringLiteral("pair-crypto"));

    const QByteArray info=transcript(serverId, peerId, clientEph, serverPub,
                                     clientNonce, serverNonce);

    QByteArray okm=SyncPairCrypto::hkdfSha256(
        shared, key, info, 64);

    if(okm.size()!=64)
        return errorAnswer(QStringLiteral("pair-crypto"));

    const QByteArray kEnc=okm.left(32);
    const QByteArray kMac=okm.mid(32, 32);

    const QByteArray cipher=SyncPairCrypto::aeadEncrypt(
        kEnc, pendingSecret.toLatin1(), info);

    if(cipher.isEmpty())
        return errorAnswer(QStringLiteral("pair-crypto"));

    const QByteArray serverMac=SyncPairCrypto::hmacSha256(
        kMac, finishMacInput(info, cipher));

    QJsonObject answer;
    answer[QStringLiteral("ok")]=true;
    answer[QStringLiteral("serverTime")]=now;
    answer[QStringLiteral("serverEph")]=SyncPairCrypto::toBase64(serverPub);
    answer[QStringLiteral("serverNonce")]=SyncPairCrypto::toBase64(serverNonce);
    answer[QStringLiteral("cipher")]=SyncPairCrypto::toBase64(cipher);
    answer[QStringLiteral("serverMac")]=SyncPairCrypto::toHex(serverMac);
    answer[QStringLiteral("codeExpiresAt")]=state.expiresAt;

    wipe(shared);
    wipe(okm);
    wipe(serverPriv);

    qDebug() << "SyncPairing: pair secret issued to" << peerId
             << "(pending, code expires in" << (state.expiresAt-now) << "ms)";
    return answer;
}


QString finishBegin(const QString &clientId, const QString &serverId,
                    const QString &code,
                    const QByteArray &clientEphPriv,
                    const QByteArray &clientEphPub,
                    const QByteArray &clientNonce,
                    const QJsonObject &answer,
                    QString *errorCode)
{
    const auto fail=[&](const QString &error) -> QString {
        if(errorCode!=nullptr)
            *errorCode=error;

        return QString();
    };

    if(clientId.isEmpty() || serverId.isEmpty())
        return fail(QStringLiteral("pair-bad-request"));

    if(!answer.value(QStringLiteral("ok")).toBool(false))
    {
        const QString error=answer.value(QStringLiteral("error")).toString();
        return fail(error.isEmpty() ? QStringLiteral("pair-bad-response")
                                    : error);
    }

    const QByteArray serverEph=SyncPairCrypto::fromBase64(
        answer.value(QStringLiteral("serverEph")).toString());
    const QByteArray serverNonce=SyncPairCrypto::fromBase64(
        answer.value(QStringLiteral("serverNonce")).toString());
    const QByteArray cipher=SyncPairCrypto::fromBase64(
        answer.value(QStringLiteral("cipher")).toString());
    const QByteArray serverMac=SyncPairCrypto::fromHex(
        answer.value(QStringLiteral("serverMac")).toString());

    if(serverEph.size()!=SyncPairCrypto::EphBytes
       || serverNonce.size()!=16
       || cipher.size()<SyncPairCrypto::NonceBytes+SyncPairCrypto::TagBytes
       || serverMac.size()!=SyncPairCrypto::MacBytes)
        return fail(QStringLiteral("pair-bad-response"));

    const QString normalized=SyncPairCrypto::normalizeCode(code);

    if(!SyncPairCrypto::isValidCode(normalized))
        return fail(QStringLiteral("pair-code-invalid"));

    const QByteArray key=SyncPairCrypto::codeKey(normalized);

    if(key.isEmpty())
        return fail(QStringLiteral("pair-crypto"));

    QByteArray shared;

    if(!SyncPairCrypto::x25519(clientEphPriv, serverEph, shared))
        return fail(QStringLiteral("pair-crypto"));

    const QByteArray info=transcript(serverId, clientId, clientEphPub,
                                     serverEph, clientNonce, serverNonce);
    QByteArray okm=SyncPairCrypto::hkdfSha256(shared, key, info, 64);

    if(okm.size()!=64)
        return fail(QStringLiteral("pair-crypto"));

    const QByteArray kEnc=okm.left(32);
    const QByteArray kMac=okm.mid(32, 32);

    const QByteArray expectedMac=SyncPairCrypto::hmacSha256(
        kMac, finishMacInput(info, cipher));

    if(!SyncPairCrypto::constantTimeEquals(serverMac, expectedMac))
        return fail(QStringLiteral("pair-bad-response"));

    QByteArray plain;

    if(!SyncPairCrypto::aeadDecrypt(kEnc, cipher, info, plain))
        return fail(QStringLiteral("pair-bad-response"));

    if(!isHex64(plain))
        return fail(QStringLiteral("pair-bad-response"));

    if(errorCode!=nullptr)
        errorCode->clear();

    const QString secret=QString::fromLatin1(plain).toLower();

    wipe(shared);
    wipe(okm);
    wipe(plain);
    return secret;
}


QJsonObject disabledAnswer(void)
{
    return errorAnswer(QStringLiteral("pairing-disabled"),
        QStringLiteral("New pairings are blocked on this server. "
                       "Enable pairing on the server first."));
}


QString errorText(const QString &errorCode)
{
    if(errorCode==QStringLiteral("pair-code-invalid"))
        return QStringLiteral("Неверный код. Проверьте код на другом устройстве "
                              "и попробуйте ещё раз.");

    if(errorCode==QStringLiteral("pair-code-expired"))
        return QStringLiteral("Код истёк. Нажмите «Pair» на другом устройстве, "
                              "чтобы получить новый.");

    if(errorCode==QStringLiteral("pair-code-missing"))
        return QStringLiteral("На другом устройстве нет активного кода. "
                              "Нажмите там «Pair» и покажите код.");

    if(errorCode==QStringLiteral("pair-rate-limited"))
        return QStringLiteral("Слишком много неверных попыток. Подождите "
                              "несколько минут и запросите новый код.");

    if(errorCode==QStringLiteral("pair-unsupported"))
        return QStringLiteral("Другое устройство не поддерживает спаривание "
                              "по одноразовому коду. Обновите его.");

    if(errorCode==QStringLiteral("pair-wrong-server"))
        return QStringLiteral("Ответило не то устройство. Проверьте адрес "
                              "и повторите.");

    if(errorCode==QStringLiteral("pair-crypto"))
        return QStringLiteral("Ошибка криптографии спаривания. "
                              "Повторите попытку.");

    if(errorCode==QStringLiteral("pair-bad-response"))
        return QStringLiteral("Некорректный ответ при спаривании. "
                              "Проверьте, что оба устройства обновлены.");

    if(errorCode==QStringLiteral("pair-bad-request"))
        return QStringLiteral("Некорректный запрос спаривания.");

    if(errorCode==QStringLiteral("pairing-disabled"))
        return QStringLiteral("Сервер запретил новые спаривания. "
                              "Включите разрешение на нём и повторите.");

    if(errorCode==QStringLiteral("pair-ip-mismatch"))
        return QStringLiteral("Код выдан для другого IP. Спаривайтесь "
                              "с того же адреса или запросите новый код.");

    return errorCode;
}


void resetForTests(void)
{
    codes().clear();
}

} // namespace SyncPairing

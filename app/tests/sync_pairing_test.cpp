#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <cstdio>

#include "SyncPairCrypto.h"
#include "SyncPairing.h"

// Полный протокол спаривания: фиксированный вектор (X25519 + HKDF +
// ChaCha20-Poly1305) и живой round-trip сервер/клиент.
//
//   make -C app/tests run

static int failures=0;

#define CHECK(cond) do { \
    if(!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while(0)


static QByteArray hex(const char *text)
{
    return SyncPairCrypto::fromHex(QString::fromLatin1(text));
}

static QByteArray b64(const char *text)
{
    return SyncPairCrypto::fromBase64(QString::fromLatin1(text));
}


int main(void)
{
    const QString serverId=QStringLiteral("server-1");
    const QString clientId=QStringLiteral("client-1");
    const QString clientName=QStringLiteral("Client");
    const QString code=QStringLiteral("ABCDEFGHIJKLMNOP");

    // --- фиксированный вектор, посчитанный независимо (libsodium+Python) ---
    const QByteArray clientPriv=hex(
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f");
    const QByteArray clientPub=hex(
        "8f40c5adb68f25624ae5b214ea767a6e"
        "c94d829d3d7b5e1ad1ba6f3e2138285f");
    const QByteArray serverPub=hex(
        "358072d6365880d1aeea329adf912138"
        "3851ed21a28e3b75e965d0d2cd166254");
    const QByteArray clientNonce=b64("QEFCQ0RFRkdISUpLTE1OTw==");
    const QByteArray serverNonce=b64("UFFSU1RVVldYWVpbXF1eXw==");
    const QByteArray cipher=b64(
        "YGFiY2RlZmdoaWpr0saFIV44GlpMBIPoXRYpEQSZMLu6QVOwb5dO2fdRnnwLnzsmAIEzlmLkzaj64qJw"
        "5g1wGEbSez0uRhqtI2fvKFy2C0Sbri6mizY1DyMWm4Y=");
    const QString serverMac=QStringLiteral(
        "02b1c7b8a5898c306667b2455cbfc414"
        "6eabcb19058e375f58a75ebed81098da");
    const QString expectedCodeMac=QStringLiteral(
        "7031abe2c0207970f8e01ae2a3b27214"
        "0969bb11e7e024124a63a112650baed8");
    const QString expectedSecret=QStringLiteral(
        "ffeeddccbbaa99887766554433221100"
        "ffeeddccbbaa99887766554433221100");

    // buildBegin должен дать ровно тот же codeMac
    {
        const QJsonObject begin=SyncPairing::buildBegin(
            clientId, clientName, serverId, code, clientPub, clientNonce);

        CHECK(begin.value(QStringLiteral("deviceId")).toString()==clientId);
        CHECK(begin.value(QStringLiteral("serverId")).toString()==serverId);
        CHECK(begin.value(QStringLiteral("pair2")).toInt()==1);
        CHECK(begin.value(QStringLiteral("codeMac")).toString()
              ==expectedCodeMac);
    }

    // finishBegin должен расшифровать фиксированный ответ
    {
        QJsonObject answer;
        answer[QStringLiteral("ok")]=true;
        answer[QStringLiteral("serverEph")]=SyncPairCrypto::toBase64(serverPub);
        answer[QStringLiteral("serverNonce")]=SyncPairCrypto::toBase64(serverNonce);
        answer[QStringLiteral("cipher")]=SyncPairCrypto::toBase64(cipher);
        answer[QStringLiteral("serverMac")]=serverMac;

        QString error;
        const QString secret=SyncPairing::finishBegin(
            clientId, serverId, code, clientPriv, clientPub, clientNonce,
            answer, &error);

        CHECK(error.isEmpty());
        CHECK(secret==expectedSecret);

        // Подмена serverMac — отказ
        QJsonObject forged=answer;
        forged[QStringLiteral("serverMac")]=QStringLiteral("00")+serverMac.mid(2);
        CHECK(SyncPairing::finishBegin(clientId, serverId, code, clientPriv,
              clientPub, clientNonce, forged, &error).isEmpty());
        CHECK(error==QStringLiteral("pair-bad-response"));

        // Подмена шифртекста — отказ по тегу AEAD
        QByteArray broken=cipher;
        broken[0]=static_cast<char>(static_cast<quint8>(broken.at(0)) ^ 0x01);
        forged=answer;
        forged[QStringLiteral("cipher")]=SyncPairCrypto::toBase64(broken);
        CHECK(SyncPairing::finishBegin(clientId, serverId, code, clientPriv,
              clientPub, clientNonce, forged, &error).isEmpty());
        CHECK(error==QStringLiteral("pair-bad-response"));
    }

    // --- живой round-trip сервер/клиент ---
    {
        SyncPairing::resetForTests();

        QMap<QString, QString> pending;
        const auto getPending=[&](const QString &id) {
            return pending.value(id);
        };
        const auto setPending=[&](const QString &id, const QString &secret) {
            pending.insert(id, secret);
        };

        const QString peerId=QStringLiteral("client-live");
        const qint64 now=1000000000;
        qint64 expiresAt=0;
        const QString liveCode=SyncPairing::ensureCode(peerId, now, QStringLiteral("127.0.0.1"),
                                false, &expiresAt);

        CHECK(SyncPairCrypto::isValidCode(liveCode));
        CHECK(expiresAt>now);

        QByteArray ephPriv, ephPub;
        CHECK(SyncPairCrypto::generateX25519(ephPriv, ephPub));
        const QByteArray nonce=SyncPairCrypto::randomBytes(16);

        const QJsonObject begin=SyncPairing::buildBegin(
            peerId, QStringLiteral("Live client"), serverId, liveCode,
            ephPub, nonce);

        const QJsonObject answer=SyncPairing::handleBegin(
            serverId, begin, now, QStringLiteral("127.0.0.1"), false,
            getPending, setPending);

        CHECK(answer.value(QStringLiteral("ok")).toBool(false));

        QString error;
        const QString secret=SyncPairing::finishBegin(
            peerId, serverId, liveCode, ephPriv, ephPub, nonce, answer,
            &error);

        CHECK(error.isEmpty());
        CHECK(secret.size()==64);
        CHECK(secret==pending.value(peerId));
        CHECK(secret!=expectedSecret);

        // Повтор begin (потеряли ответ) — тот же pending-секрет
        {
            QByteArray eph2Priv, eph2Pub;
            CHECK(SyncPairCrypto::generateX25519(eph2Priv, eph2Pub));
            const QByteArray nonce2=SyncPairCrypto::randomBytes(16);

            const QJsonObject begin2=SyncPairing::buildBegin(
                peerId, QStringLiteral("Live client"), serverId, liveCode,
                eph2Pub, nonce2);
            const QJsonObject answer2=SyncPairing::handleBegin(
                serverId, begin2, now+1000, QStringLiteral("127.0.0.1"),
                false, getPending, setPending);

            CHECK(answer2.value(QStringLiteral("ok")).toBool(false));

            const QString secret2=SyncPairing::finishBegin(
                peerId, serverId, liveCode, eph2Priv, eph2Pub, nonce2,
                answer2, &error);

            CHECK(secret2==secret);
        }

        // Одноразовость: погасили код — повтор отклонён
        SyncPairing::consumeCode(peerId);
        CHECK(SyncPairing::handleBegin(serverId, begin, now+2000,
              QStringLiteral("127.0.0.1"), false, getPending, setPending)
              .value(QStringLiteral("error")).toString()
              ==QStringLiteral("pair-code-missing"));
    }

    // --- истёкший код ---
    {
        SyncPairing::resetForTests();
        QMap<QString, QString> pending;
        const auto getPending=[&](const QString &id) { return pending.value(id); };
        const auto setPending=[&](const QString &id, const QString &s) {
            pending.insert(id, s);
        };

        const QString peerId=QStringLiteral("client-expire");
        const qint64 now=2000000000;
        const QString liveCode=SyncPairing::ensureCode(peerId, now, QStringLiteral("127.0.0.1"), false);

        QByteArray priv, pub;
        CHECK(SyncPairCrypto::generateX25519(priv, pub));
        const QByteArray nonce=SyncPairCrypto::randomBytes(16);
        const QJsonObject begin=SyncPairing::buildBegin(
            peerId, QStringLiteral("Expire"), serverId, liveCode, pub, nonce);

        const QJsonObject answer=SyncPairing::handleBegin(
            serverId, begin, now+SyncPairing::CodeTtlMs+1,
            QStringLiteral("127.0.0.1"), false, getPending, setPending);

        CHECK(answer.value(QStringLiteral("error")).toString()
              ==QStringLiteral("pair-code-expired"));
    }

    // --- неверный код и блокировка ---
    {
        SyncPairing::resetForTests();
        QMap<QString, QString> pending;
        const auto getPending=[&](const QString &id) { return pending.value(id); };
        const auto setPending=[&](const QString &id, const QString &s) {
            pending.insert(id, s);
        };

        const QString peerId=QStringLiteral("client-bruteforce");
        const qint64 now=3000000000;
        CHECK(!SyncPairing::ensureCode(peerId, now,
              QStringLiteral("127.0.0.1"), false).isEmpty());

        QByteArray priv, pub;
        CHECK(SyncPairCrypto::generateX25519(priv, pub));
        const QByteArray nonce=SyncPairCrypto::randomBytes(16);

        // Валидный по форме, но другой код
        const QJsonObject wrong=SyncPairing::buildBegin(
            peerId, QStringLiteral("Brute"), serverId,
            QStringLiteral("ZZZZZZZZZZZZZZZZ"), pub, nonce);

        QString lastError;

        for(int i=0; i<SyncPairing::MaxFailedAttempts; ++i)
        {
            const QJsonObject answer=SyncPairing::handleBegin(
                serverId, wrong, now+i, QStringLiteral("127.0.0.1"),
                false, getPending, setPending);
            lastError=answer.value(QStringLiteral("error")).toString();
        }

        CHECK(lastError==QStringLiteral("pair-rate-limited"));

        // Заблокирован: новый код не выдаётся, пока не истечёт lockout
        qint64 lockedUntil=0;
        CHECK(SyncPairing::ensureCode(peerId, now+10, QStringLiteral("127.0.0.1"),
                                    false, nullptr, &lockedUntil)
              .isEmpty());
        CHECK(lockedUntil>now+10);
    }

    // --- привязка кода к IP ---
    {
        SyncPairing::resetForTests();
        QMap<QString, QString> pending;
        const auto getPending=[&](const QString &id) { return pending.value(id); };
        const auto setPending=[&](const QString &id, const QString &s) {
            pending.insert(id, s);
        };

        const QString peerId=QStringLiteral("client-ipbind");
        const qint64 now=4000000000;
        const QString liveCode=SyncPairing::ensureCode(
            peerId, now, QStringLiteral("10.0.0.5"), true);
        CHECK(!liveCode.isEmpty());

        QByteArray priv, pub;
        CHECK(SyncPairCrypto::generateX25519(priv, pub));
        const QByteArray nonce=SyncPairCrypto::randomBytes(16);
        const QJsonObject begin=SyncPairing::buildBegin(
            peerId, QStringLiteral("IP bind"), serverId, liveCode, pub, nonce);

        // Другой IP — отказ, код не выдаёт секрет
        const QJsonObject wrongIp=SyncPairing::handleBegin(
            serverId, begin, now, QStringLiteral("10.0.0.6"), true,
            getPending, setPending);
        CHECK(wrongIp.value(QStringLiteral("error")).toString()
              ==QStringLiteral("pair-ip-mismatch"));

        // Тот же IP — ок
        const QJsonObject okIp=SyncPairing::handleBegin(
            serverId, begin, now, QStringLiteral("10.0.0.5"), true,
            getPending, setPending);
        CHECK(okIp.value(QStringLiteral("ok")).toBool(false));
    }

    if(failures==0)
        std::printf("sync_pairing_test OK\n");

    return failures==0 ? 0 : 1;
}

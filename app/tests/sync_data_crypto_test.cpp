#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <cstdio>

#include "SyncDataCrypto.h"
#include "SyncPairCrypto.h"

// End-to-end шифрование data-канала: envelope, AAD, ключи, hello MAC.
// Часть векторов дублируется в Kotlin (SyncDataCryptoTest), чтобы
// клиент и сервер сходились байт-в-байт.
//
//   make -C app/tests run

static int failures=0;

#define CHECK(cond) do { \
    if(!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while(0)


int main(void)
{
    const QString secret=QStringLiteral(
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff");
    const QString clientId=QStringLiteral("client-1");
    const QString serverId=QStringLiteral("server-1");
    const QString route=QStringLiteral("POST /push-note");
    const qint64 ts=1700000000000LL;
    const QString session=SyncPairCrypto::toBase64(
        SyncPairCrypto::fromHex(QStringLiteral(
            "000102030405060708090a0b0c0d0e0f")));

    QJsonObject body;
    body[QStringLiteral("id")]=QStringLiteral("note-1");
    QJsonObject fields;
    fields[QStringLiteral("name")]=QStringLiteral("Тест");
    body[QStringLiteral("fields")]=fields;
    body[QStringLiteral("text")]=QStringLiteral("secret note text");

    // --- request seal/open ---
    const QJsonObject envelope=SyncDataCrypto::sealRequest(
        secret, clientId, serverId, session, route, ts, body);
    CHECK(!envelope.isEmpty());
    CHECK(envelope.value(QStringLiteral("_enc")).toInt()==1);
    CHECK(envelope.value(QStringLiteral("_dev")).toString()==clientId);
    CHECK(envelope.value(QStringLiteral("_ts")).toVariant().toLongLong()==ts);
    CHECK(envelope.value(QStringLiteral("_s")).toString()==session);

    {
        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(SyncDataCrypto::openRequestWithSecret(
            secret, serverId, route, envelope, ts+100, opened, ctx, error));
        CHECK(opened==body);
        CHECK(ctx.encrypted);
        CHECK(ctx.clientId==clientId);
        CHECK(ctx.serverId==serverId);
        CHECK(ctx.session==session);
    }

    // --- tampering: ct / session / ts ---
    {
        QJsonObject bad=envelope;
        QByteArray ct=SyncPairCrypto::fromBase64(
            bad.value(QStringLiteral("_ct")).toString());
        ct[ct.size()-1]=static_cast<char>(
            static_cast<quint8>(ct.at(ct.size()-1)) ^ 0x01);
        bad[QStringLiteral("_ct")]=SyncPairCrypto::toBase64(ct);

        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(!SyncDataCrypto::openRequestWithSecret(
            secret, serverId, route, bad, ts+100, opened, ctx, error));
    }

    {
        QJsonObject bad=envelope;
        bad[QStringLiteral("_s")]=SyncPairCrypto::toBase64(
            SyncPairCrypto::randomBytes(16));

        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(!SyncDataCrypto::openRequestWithSecret(
            secret, serverId, route, bad, ts+100, opened, ctx, error));
    }

    {
        QJsonObject bad=envelope;
        bad[QStringLiteral("_ts")]=ts+1000;

        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(!SyncDataCrypto::openRequestWithSecret(
            secret, serverId, route, bad, ts+100, opened, ctx, error));
    }

    // Запрос мог подвиснуть в фоне Android: минута опоздания — не stale
    {
        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(SyncDataCrypto::openRequestWithSecret(
            secret, serverId, route, envelope, ts+60000, opened, ctx, error));
    }

    {
        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(!SyncDataCrypto::openRequestWithSecret(
            secret, serverId, route, envelope, ts+180000, opened, ctx, error));
    }

    // --- response seal/open ---
    {
        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(SyncDataCrypto::openRequestWithSecret(
            secret, serverId, route, envelope, ts+100, opened, ctx, error));

        QJsonObject resp;
        resp[QStringLiteral("ok")]=true;
        resp[QStringLiteral("id")]=QStringLiteral("note-1");

        const QJsonObject respEnv=SyncDataCrypto::sealResponse(ctx, resp);
        CHECK(!respEnv.isEmpty());
        CHECK(respEnv.value(QStringLiteral("_dev")).toString()==serverId);

        QJsonObject respOpened;
        CHECK(SyncDataCrypto::openResponse(secret, ctx, respEnv,
                                           respOpened, error));
        CHECK(respOpened==resp);

        QJsonObject bad=respEnv;
        bad[QStringLiteral("_ts")]=ts+1;
        CHECK(!SyncDataCrypto::openResponse(secret, ctx, bad,
                                            respOpened, error));
    }

    // --- hello MAC стабилен и зависит от полей ---
    {
        const QByteArray challenge=SyncDataCrypto::helloRequestChallenge(
            clientId, 1700000000123LL, session,
            QStringLiteral("group-1"), QStringLiteral("Laptop"),
            false, true);
        const QString mac=SyncDataCrypto::helloMac(secret, challenge);
        CHECK(!mac.isEmpty());
        CHECK(mac==SyncDataCrypto::helloMac(secret, challenge));
        CHECK(mac!=SyncDataCrypto::helloMac(secret,
            SyncDataCrypto::helloRequestChallenge(
                clientId, 1700000000124LL, session,
                QStringLiteral("group-1"), QStringLiteral("Laptop"),
                false, true)));

        const QByteArray respInput=SyncDataCrypto::helloResponseMacInput(
            clientId, serverId, session, 1700000000123LL, 1700000000124LL,
            true, true, QStringLiteral("group-1"), QStringLiteral("Server"),
            false, false, 0, QString(), QString(), QString());
        CHECK(!SyncDataCrypto::helloMac(secret, respInput).isEmpty());
    }

    // --- фиксированный interop-вектор (совпадает с Kotlin) ---
    {
        const QString vectorSecret=QStringLiteral(
            "00112233445566778899aabbccddeeff"
            "00112233445566778899aabbccddeeff");
        const QString vectorSession=QStringLiteral("AAECAwQFBgcICQoLDA0ODw==");
        const QString vectorEnvelope=QStringLiteral(
            "{\"_ct\":\"AAECAwQFBgcICQoLq7mODr+56HRlo8QfIzL16lTUCJLsKcFTBpEMvTtJPKY8UUSKh+8oY2yQqd693gj1kud2byqgRPWA\","
            "\"_dev\":\"client-1\",\"_enc\":1,"
            "\"_s\":\"AAECAwQFBgcICQoLDA0ODw==\","
            "\"_ts\":1700000000000}");

        QJsonParseError parseError;
        const QJsonDocument doc=QJsonDocument::fromJson(
            vectorEnvelope.toUtf8(), &parseError);
        CHECK(parseError.error==QJsonParseError::NoError);

        QJsonObject opened;
        SyncDataCrypto::Context ctx;
        QString error;
        CHECK(SyncDataCrypto::openRequestWithSecret(
            vectorSecret, QStringLiteral("server-1"),
            QStringLiteral("POST /push-note"), doc.object(),
            1700000000100LL, opened, ctx, error));
        CHECK(opened.value(QStringLiteral("id")).toString()
              ==QStringLiteral("note-1"));
        CHECK(opened.value(QStringLiteral("text")).toString()
              ==QStringLiteral("secret note text"));

        const QByteArray challenge=SyncDataCrypto::helloRequestChallenge(
            QStringLiteral("client-1"), 1700000000123LL, vectorSession,
            QStringLiteral("group-1"), QStringLiteral("Laptop"), false, true);
        CHECK(SyncPairCrypto::toHex(challenge)
              ==QStringLiteral("6d7974657472612d68656c6c6f2d7265712d7631"
                               "00636c69656e742d31003137303030303030303031323300"
                               "41414543417751464267634943516f4c4441304f44773d3d00"
                               "67726f75702d31004c6170746f7000300031"));
        CHECK(SyncDataCrypto::helloMac(vectorSecret, challenge)
              ==QStringLiteral("b0fde28f3204fd19cb453573891b69798"
                               "95460b43d14e7c40441d654e591dd29"));

        const QByteArray key=SyncDataCrypto::dataKeyForTest(
            vectorSecret, QStringLiteral("c2s"), vectorSession,
            QStringLiteral("client-1"), QStringLiteral("server-1"));
        CHECK(SyncPairCrypto::toHex(key)
              ==QStringLiteral("7f01b44c36aa2283837e5abd62f4a18d"
                               "3ee70c75acdf6e43d9b121e84448c253"));
    }

    // --- active session ---
    {
        SyncDataCrypto::clearAllSessions();
        CHECK(!SyncDataCrypto::sessionActive(clientId, session));
        SyncDataCrypto::setActiveSession(clientId, session);
        CHECK(SyncDataCrypto::sessionActive(clientId, session));
        CHECK(!SyncDataCrypto::sessionActive(clientId, QStringLiteral("other")));
        SyncDataCrypto::clearActiveSession(clientId);
        CHECK(!SyncDataCrypto::sessionActive(clientId, session));
    }

    if(failures==0)
        std::printf("sync_data_crypto_test OK\n");

    return failures==0 ? 0 : 1;
}

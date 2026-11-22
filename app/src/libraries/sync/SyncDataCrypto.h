#ifndef SYNCDATACRYPTO_H
#define SYNCDATACRYPTO_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class AppConfig;

// End-to-end шифрование sync-данных ключом пары (sync/3, capability enc:1).
//
// Долговременный секрет S (32 байта) уже есть у обеих сторон после
// спаривания. Из него + случайной сессии выводим направленные ключи
// K_c2s/K_s2c (HKDF-SHA256). Тело каждого запроса/ответа шифруется
// ChaCha20-Poly1305; AEAD-тег заменяет старый _proof и покрывает тело.
//
// Снаружи остаются только _dev/_ts/_s/_ct: маршрутизация, anti-replay
// и выбор ключа. Метаданные (id, размеры, тайминги, route) видны в
// plain HTTP; TLS скрывает и их.
namespace SyncDataCrypto
{

struct Context
{
    bool encrypted=false;
    QString secret;     // hex; сервер: подобранный current/pending
    QString clientId;
    QString serverId;
    QString session;    // base64
    qint64 ts=0;
    QString route;
};

// --- hello: proof2 и MAC ответа (фиксированный порядок полей) ---
QByteArray helloRequestChallenge(const QString &clientId, qint64 clientTime,
                                 const QString &sessionB64,
                                 const QString &groupId,
                                 const QString &deviceName,
                                 bool pairRequest, bool enc);

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
                                 const QString &detail);

// HMAC-SHA256(secret, input) hex, нижний регистр
QString helloMac(const QString &secretHex, const QByteArray &input);

// Тестовый/диагностический доступ к направленному ключу
QByteArray dataKeyForTest(const QString &secretHex, const QString &direction,
                          const QString &sessionB64,
                          const QString &clientId,
                          const QString &serverId);

// --- data envelope ---

// Клиент: зашифровать тело запроса. Пусто — ошибка/нет секрета.
QJsonObject sealRequest(const QString &secretHex, const QString &clientId,
                        const QString &serverId, const QString &sessionB64,
                        const QString &route, qint64 ts,
                        const QJsonObject &body);

// Чистая проверка с уже известным секретом (для тестов и сервера).
bool openRequestWithSecret(const QString &secretHex, const QString &serverId,
                           const QString &route, const QJsonObject &envelope,
                           qint64 now, QJsonObject &body, Context &ctx,
                           QString &error);

// Сервер: найти секрет по _dev (current/pending), проверить активную
// сессию, расшифровать. ctx.secret заполняется для ответа.
bool openRequest(AppConfig *config, const QString &serverId,
                 const QString &route, const QJsonObject &envelope,
                 qint64 now, QJsonObject &body, Context &ctx, QString &error);

// Сервер: зашифровать ответ (ctx.secret обязателен).
QJsonObject sealResponse(const Context &ctx, const QJsonObject &body);

// Клиент: расшифровать ответ.
bool openResponse(const QString &secretHex, const Context &ctx,
                  const QJsonObject &envelope, QJsonObject &body,
                  QString &error);

// Активная сессия устройства (ставится на hello, проверяется на data).
void setActiveSession(const QString &deviceId, const QString &sessionB64);
void clearActiveSession(const QString &deviceId);
void clearAllSessions(void);
bool sessionActive(const QString &deviceId, const QString &sessionB64);

} // namespace SyncDataCrypto

#endif // SYNCDATACRYPTO_H

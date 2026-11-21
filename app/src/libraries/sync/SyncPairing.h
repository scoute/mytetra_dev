#ifndef SYNCPAIRING_H
#define SYNCPAIRING_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <functional>

// Спаривание sync/2: одноразовый код + X25519.
//
// Схема (подробности — в docs/sync-pairing-v2.md):
//   1. Клиент стучится /hello с pairRequest. Сервер генерирует
//      16-символьный base32-код (80 бит), показывает его на своём
//      экране. По сети код не ходит.
//   2. Клиент вводит код, генерирует эфемерную пару X25519 и шлёт
//      /pair-begin с clientEph, clientNonce и codeMac (HMAC от кода).
//   3. Сервер проверяет codeMac, генерирует свою эфемерную пару,
//      выводит HKDF-SHA256 из общего секрета и кода, шифрует
//      ChaCha20-Poly1305 случайный 32-байтный долговременный секрет
//      и отдаёт его вместе с serverMac.
//   4. Клиент проверяет serverMac, расшифровывает секрет, запоминает.
//      Code одноразовый: после первого hello с новым секретом сервер
//      повышает pending -> current и гасит код.
//
// 80 бит кода достаточно, чтобы офлайн-перебор захваченного рукопожатия
// был нереальным; PAKE здесь не нужен (он нужен для коротких PIN).
namespace SyncPairing
{

constexpr qint64 CodeTtlMs=5*60*1000;
constexpr int MaxFailedAttempts=5;
constexpr qint64 LockoutMs=5*60*1000;

// --- серверная сторона -------------------------------------------------

// Гарантировать живой код для пира. Если код истёк — новый.
// Пусто — пир заблокирован после неудачных попыток (until — до когда).
QString ensureCode(const QString &peerId, qint64 now,
                   qint64 *expiresAt=nullptr,
                   qint64 *lockedUntil=nullptr);

// Текущий живой код. false — нет/истёк/заблокирован
bool currentCode(const QString &peerId, qint64 now,
                 QString *code=nullptr, qint64 *expiresAt=nullptr);

// Погасить код (после повышения pending -> current или Reject)
void consumeCode(const QString &peerId);
void clearCode(const QString &peerId);
void clearAll(void);

// --- протокол ----------------------------------------------------------

// Серверные колбэки к хранилищу pending-секрета (AppConfig в бою,
// словарь в тестах). Отделяют протокол от QSettings.
using PendingGetter=std::function<QString(const QString &peerId)>;
using PendingSetter=std::function<void(const QString &peerId,
                                       const QString &secret)>;

// Клиент -> сервер. code — уже нормализованный (normalizeCode).
QJsonObject buildBegin(const QString &clientId, const QString &clientName,
                       const QString &serverId, const QString &code,
                       const QByteArray &clientEphPub,
                       const QByteArray &clientNonce);

// Сервер: проверить begin и собрать ответ. Ошибка — поле "error".
QJsonObject handleBegin(const QString &serverId, const QJsonObject &request,
                        qint64 now, const PendingGetter &getPending,
                        const PendingSetter &setPending);

// Клиент: проверить ответ и расшифровать долговременный секрет.
// Возвращает hex-секрет (64 символа) или пусто; errorCode — машинный код.
QString finishBegin(const QString &clientId, const QString &serverId,
                    const QString &code,
                    const QByteArray &clientEphPriv,
                    const QByteArray &clientEphPub,
                    const QByteArray &clientNonce,
                    const QJsonObject &answer,
                    QString *errorCode=nullptr);

// Человекочитаемый текст ошибки для UI
QString errorText(const QString &errorCode);

// Только для тестов: сбросить in-memory коды/лимиты
void resetForTests(void);

} // namespace SyncPairing

#endif // SYNCPAIRING_H

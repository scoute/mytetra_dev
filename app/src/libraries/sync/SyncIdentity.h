#ifndef SYNCIDENTITY_H
#define SYNCIDENTITY_H

#include <QString>

// Идентичность устройства для синхронизации. Асимметричной криптографии
// в Qt нет, новых библиотек не тянем: взаимная аутентификация строится
// на общем секрете спаривания (HMAC-SHA256, QMessageAuthenticationCode),
// а фингерпринт — SHA256 от deviceId для сверки вслух/глазами.
class SyncIdentity
{
public:

    // Фингерпринт устройства: SHA256(deviceId) hex группами по 4
    // для сверки при спаривании. Детерминирован, секрета не содержит
    static QString fingerprint(const QString &deviceId);

    // Новый секрет спаривания: 32 случайных байта hex.
    // Хранится в конфиге обоих спаренных устройств, по сети не ходит
    // в открытом виде (только HMAC-доказательства владения)
    static QString generatePairingSecret(void);

    // HMAC-SHA256 доказательства владения секретом для challenge.
    // Hex-строка; сравнение только по полному совпадению
    static QString hmacProof(const QString &pairingSecretHex,
                             const QByteArray &challenge);
};

#endif // SYNCIDENTITY_H

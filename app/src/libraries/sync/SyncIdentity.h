#ifndef SYNCIDENTITY_H
#define SYNCIDENTITY_H

#include <QString>

class AppConfig;

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

    // Детерминированный secret спаривания из PIN, введенного на обеих
    // сторонах: по сети не ходит вообще. id сортируются чтобы обе стороны
    // посчитали одинаково независимо от ролей
    static QString derivePairingSecret(const QString &deviceIdA,
                                       const QString &deviceIdB,
                                       const QString &pin);

    // Детерминированный groupId для первого спаривания двух девственных
    // устройств (у обоих пусто): обе стороны считают одинаково
    static QString pairGroupId(const QString &deviceIdA,
                               const QString &deviceIdB);

    // Вывести secret из PIN и запомнить пару: secret + с кем (pairedDeviceId).
    // Единая точка для всех путей спаривания/починки (GUI, headless, retry).
    // Пустой PIN или id — false, ничего не меняет
    static bool adoptPairing(AppConfig *config, const QString &peerId,
                             const QString &pin);
};

#endif // SYNCIDENTITY_H

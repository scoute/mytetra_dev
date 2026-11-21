#ifndef SYNCIDENTITY_H
#define SYNCIDENTITY_H

#include <QString>
#include <QStringList>

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
    // Пустой PIN или id — false, ничего не меняет.
    // Аддитивно для кольца: новая пара НЕ затирает старые (у человека
    // 4-5 устройств), первой паре соответствует легаси-секрет
    static bool adoptPairing(AppConfig *config, const QString &peerId,
                             const QString &pin);

    // Новый путь (sync/2): долговременный секрет уже случайный (выдан
    // сервером после спаривания по одноразовому коду). Сохранить его
    // с той же семантикой кольца/эпохи, что adoptPairing
    static bool adoptPairingSecret(AppConfig *config, const QString &peerId,
                                   const QString &secret);

    // Кандидаты для проверки proof: текущий секрет, затем pending
    // (выданный по коду, но ещё не подтверждённый первым hello).
    // Без дублей; пусто — пары нет
    static QStringList candidateSecrets(const AppConfig *config,
                                        const QString &peerId);

    // Первый успешный hello с pending-секретом: повысить его до текущего
    // и забыть старый. false — pending нет
    static bool promotePendingSecret(AppConfig *config, const QString &peerId);

    // Короткий фингерпринт ПАРЫ от общего секрета, а не от публичного id.
    // Им можно сверить устройства глазами уже после спаривания: подделать
    // его, не зная секрета, нельзя
    static QString pairFingerprint(const QString &secretHex,
                                   const QString &deviceIdA,
                                   const QString &deviceIdB);

    // Секрет для конкретного пира из кольца: сначала запись устройства,
    // затем легаси-пара при совпадении id. Пусто — пары с ним нет
    static QString secretForPeer(const AppConfig *config,
                                 const QString &peerId);
};

#endif // SYNCIDENTITY_H

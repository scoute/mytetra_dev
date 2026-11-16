#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

#include "SyncIdentity.h"
#include "SyncSidecar.h"
#include "models/appConfig/AppConfig.h"


QString SyncIdentity::fingerprint(const QString &deviceId)
{
    // SHA256 hex, группами по 4 через пробел: удобно сличить глазами
    // или продиктовать. deviceId публичен, фингерпринт секрета не несет
    const QString hex=QString::fromLatin1(
        QCryptographicHash::hash(deviceId.toLatin1(),
                                 QCryptographicHash::Sha256).toHex()).toUpper();

    QStringList groups;
    for(int i=0; i<hex.size(); i+=4)
        groups << hex.mid(i, 4);

    return groups.join(QStringLiteral(" "));
}


QString SyncIdentity::generatePairingSecret(void)
{
    QByteArray bytes;
    bytes.reserve(32);
    for(int i=0; i<32; ++i)
        bytes.append(static_cast<char>(QRandomGenerator::global()->generate() & 0xFF));

    return QString::fromLatin1(bytes.toHex());
}


QString SyncIdentity::hmacProof(const QString &pairingSecretHex,
                                const QByteArray &challenge)
{
    // Секрет хранится hex-строкой; пустой секрет или пустой challenge
    // доказательства не дают (сравнение все равно не сойдется)
    if(pairingSecretHex.isEmpty() || challenge.isEmpty())
        return QString();

    const QByteArray key=QByteArray::fromHex(pairingSecretHex.toLatin1());

    if(key.isEmpty())
        return QString();

    return QString::fromLatin1(
        QMessageAuthenticationCode::hash(challenge, key,
                                         QCryptographicHash::Sha256).toHex());
}


QString SyncIdentity::derivePairingSecret(const QString &deviceIdA,
                                          const QString &deviceIdB,
                                          const QString &pin)
{
    // Сортировка чтобы клиент и сервер сошлись независимо от ролей.
    // Пустой PIN запрещен: иначе secret становится вычислимым по открытым id
    if(deviceIdA.isEmpty() || deviceIdB.isEmpty() || pin.isEmpty())
        return QString();

    const QString first=(deviceIdA<deviceIdB) ? deviceIdA : deviceIdB;
    const QString second=(deviceIdA<deviceIdB) ? deviceIdB : deviceIdA;

    return QString::fromLatin1(
        QCryptographicHash::hash((first+second+pin).toLatin1(),
                                 QCryptographicHash::Sha256).toHex());
}


QString SyncIdentity::pairGroupId(const QString &deviceIdA,
                                  const QString &deviceIdB)
{
    if(deviceIdA.isEmpty() || deviceIdB.isEmpty())
        return QString();

    const QString first=(deviceIdA<deviceIdB) ? deviceIdA : deviceIdB;
    const QString second=(deviceIdA<deviceIdB) ? deviceIdB : deviceIdA;

    // 16 hex-символов достаточно для группового id личного облака
    return QString::fromLatin1(
        QCryptographicHash::hash((first+second).toLatin1(),
                                 QCryptographicHash::Sha256).toHex()).left(16);
}


bool SyncIdentity::adoptPairing(AppConfig *config, const QString &peerId,
                                const QString &pin)
{
    if(config==nullptr || peerId.isEmpty() || pin.isEmpty())
        return false;

    if(peerId==config->get_syncDeviceId())
        return false;

    const QString derived=derivePairingSecret(config->get_syncDeviceId(),
                                              peerId, pin);

    if(derived.isEmpty())
        return false;

    // Кольцо: новая пара добавляется, старые не трогаем (у человека
    // 4-5 устройств: ПК, ноут, телефоны, планшет). Первая в жизни пара
    // дополнительно кладется в легаси-секрет для совместимости.
    // Новая эпоха (спрос стратегии + чистка tombstone'ов) — только когда
    // кольцо было пустым: присоединение второго устройства не должно
    // уничтожать чужие отложенные удаления и молча все сливать.
    // Починка пары с тем же пиром — не новая, стратегию не спрашиваем
    const QMap<QString, QString> ringBefore=config->get_syncSecretsRing();
    const bool firstEver=ringBefore.isEmpty();
    const bool isNewPair=firstEver || !ringBefore.contains(peerId);

    config->set_syncDeviceSecret(peerId, derived);

    if(firstEver)
        config->set_syncPairingSecret(derived);

    if(config->get_syncPairedDeviceId().isEmpty())
        config->set_syncPairedDeviceId(peerId);

    if(isNewPair)
    {
        config->set_syncAskStrategy(true);

        if(firstEver)
        {
            // Новая пара — новая эпоха: старые tombstone'ы не должны
            // поехать новому пиру и уничтожить его данные (это уже
            // случалось: удаление локальной базы + новая пара -> обнулился
            // сервер, потому что tombstone'ы пережили смену пары)
            SyncSidecar::clearTombstones();
        }
    }

    // Пара пересоздана: старые "запрос"/взаимность протухли,
    // взаимность наступит после первого обмена с новым секретом
    config->clear_syncDeviceOffer(peerId);
    config->clear_syncDeviceMutual(peerId);
    return true;
}


QString SyncIdentity::secretForPeer(const AppConfig *config,
                                    const QString &peerId)
{
    if(config==nullptr || peerId.isEmpty())
        return QString();

    // Сначала запись устройства (свежая), затем легаси-пара
    const QString direct=config->get_syncDeviceSecret(peerId);

    if(!direct.isEmpty())
        return direct;

    if(config->get_syncPairedDeviceId()==peerId)
        return config->get_syncPairingSecret();

    return QString();
}

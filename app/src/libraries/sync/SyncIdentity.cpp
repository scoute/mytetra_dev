#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

#include "SyncIdentity.h"
#include "SyncSidecar.h"
#include "SyncPairCrypto.h"
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
    return SyncPairCrypto::randomSecretHex();
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

    return adoptPairingSecret(config, peerId, derived);
}


bool SyncIdentity::adoptPairingSecret(AppConfig *config, const QString &peerId,
                                      const QString &secret)
{
    if(config==nullptr || peerId.isEmpty() || secret.isEmpty())
        return false;

    if(peerId==config->get_syncDeviceId())
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

    config->set_syncDeviceSecret(peerId, secret);

    if(firstEver)
        config->set_syncPairingSecret(secret);

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


QStringList SyncIdentity::candidateSecrets(const AppConfig *config,
                                           const QString &peerId)
{
    QStringList out;

    if(config==nullptr || peerId.isEmpty())
        return out;

    const QString current=secretForPeer(config, peerId);

    if(!current.isEmpty())
        out << current;

    const QString pending=config->get_syncDevicePendingSecret(peerId);

    if(!pending.isEmpty() && !out.contains(pending))
        out << pending;

    return out;
}


bool SyncIdentity::promotePendingSecret(AppConfig *config,
                                        const QString &peerId)
{
    if(config==nullptr || peerId.isEmpty())
        return false;

    const QString pending=config->get_syncDevicePendingSecret(peerId);

    if(pending.isEmpty())
        return false;

    if(!adoptPairingSecret(config, peerId, pending))
        return false;

    config->clear_syncDevicePendingSecret(peerId);
    return true;
}


QString SyncIdentity::pairFingerprint(const QString &secretHex,
                                      const QString &deviceIdA,
                                      const QString &deviceIdB)
{
    if(secretHex.isEmpty() || deviceIdA.isEmpty() || deviceIdB.isEmpty())
        return QString();

    const QByteArray secret=QByteArray::fromHex(secretHex.toLatin1());

    if(secret.size()!=32)
        return QString();

    const QString first=(deviceIdA<deviceIdB) ? deviceIdA : deviceIdB;
    const QString second=(deviceIdA<deviceIdB) ? deviceIdB : deviceIdA;

    const QByteArray data=QByteArrayLiteral("mytetra-pair-fingerprint-v2")
        +secret+first.toLatin1()+second.toLatin1();

    const QString hex=QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256)
            .toHex()).toUpper();

    QStringList groups;

    for(int i=0; i<hex.size(); i+=4)
        groups << hex.mid(i, 4);

    return groups.join(QStringLiteral(" "));
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

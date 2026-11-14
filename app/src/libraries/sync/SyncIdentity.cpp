#include <QByteArray>
#include <QStringList>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

#include "SyncIdentity.h"
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

    config->set_syncPairingSecret(derived);
    config->set_syncPairedDeviceId(peerId);

    // Пара пересоздана: старые "запрос"/взаимность протухли,
    // взаимность наступит после первого обмена с новым секретом
    config->clear_syncDeviceOffer(peerId);
    config->clear_syncDeviceMutual(peerId);
    return true;
}

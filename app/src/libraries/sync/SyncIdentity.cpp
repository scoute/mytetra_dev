#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

#include "SyncIdentity.h"


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

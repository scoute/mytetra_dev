#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStringList>

#include <cstring>

#include "monocypher.h"
#include "SyncPairCrypto.h"


namespace SyncPairCrypto
{

namespace
{
const char *const kBase32Alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int base32Value(char c)
{
    if(c>='A' && c<='Z')
        return c-'A';

    if(c>='2' && c<='7')
        return 26+(c-'2');

    return -1;
}
}


QByteArray randomBytes(int count)
{
    if(count<=0)
        return QByteArray();

    QByteArray out(count, 0);
    QRandomGenerator *rng=QRandomGenerator::system();
    int offset=0;

    while(offset<count)
    {
        const quint64 value=rng->generate64();
        const int chunk=qMin<int>(8, count-offset);
        std::memcpy(out.data()+offset, &value, static_cast<size_t>(chunk));
        offset+=chunk;
    }

    return out;
}


QString generateCode(void)
{
    return base32Encode(randomBytes(CodeBytes));
}


QString randomSecretHex(void)
{
    return toHex(randomBytes(KeyBytes));
}


QString normalizeCode(const QString &input)
{
    QString out;

    for(const QChar &ch : input)
    {
        if(ch.isSpace() || ch==QLatin1Char('-') || ch==QLatin1Char('_'))
            continue;

        const QChar upper=ch.toUpper();

        if(upper.unicode()>0x7F)
            return QString();

        const char c=static_cast<char>(upper.unicode());

        if(base32Value(c)<0)
            return QString();

        out.append(upper);
    }

    if(out.size()!=CodeChars)
        return QString();

    return out;
}


bool isValidCode(const QString &normalized)
{
    if(normalized.size()!=CodeChars)
        return false;

    for(const QChar &ch : normalized)
    {
        if(ch.unicode()>0x7F)
            return false;

        if(base32Value(static_cast<char>(ch.unicode()))<0)
            return false;
    }

    return true;
}


QString formatCode(const QString &normalized)
{
    if(!isValidCode(normalized))
        return normalized;

    QStringList groups;

    for(int i=0; i<normalized.size(); i+=4)
        groups << normalized.mid(i, 4);

    return groups.join(QLatin1Char('-'));
}


QString base32Encode(const QByteArray &bytes)
{
    QString out;
    quint32 buffer=0;
    int bits=0;

    for(const char raw : bytes)
    {
        buffer=(buffer<<8) | static_cast<quint8>(raw);
        bits+=8;

        while(bits>=5)
        {
            bits-=5;
            out.append(QLatin1Char(kBase32Alphabet[(buffer>>bits) & 0x1F]));
        }
    }

    if(bits>0)
        out.append(QLatin1Char(kBase32Alphabet[(buffer<<(5-bits)) & 0x1F]));

    return out;
}


QByteArray base32Decode(const QString &code)
{
    QByteArray out;
    quint32 buffer=0;
    int bits=0;

    for(const QChar &ch : code)
    {
        if(ch.unicode()>0x7F)
            return QByteArray();

        const int value=base32Value(static_cast<char>(ch.toUpper().unicode()));

        if(value<0)
            return QByteArray();

        buffer=(buffer<<5) | static_cast<quint32>(value);
        bits+=5;

        if(bits>=8)
        {
            bits-=8;
            out.append(static_cast<char>((buffer>>bits) & 0xFF));
        }
    }

    if(bits>=5)
        return QByteArray(); // лишние биты: код не кратен байтам

    // Младшие неиспользованные биты обязаны быть нулями
    if(bits>0 && (buffer & ((1u<<bits)-1))!=0)
        return QByteArray();

    return out;
}


QByteArray sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}


QByteArray hmacSha256(const QByteArray &key, const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, key,
                                            QCryptographicHash::Sha256);
}


QByteArray hkdfSha256(const QByteArray &ikm, const QByteArray &salt,
                      const QByteArray &info, int length)
{
    if(length<=0)
        return QByteArray();

    // RFC 5869: PRK = HMAC(salt, IKM), затем T(i) = HMAC(PRK, T(i-1)||info||i)
    const QByteArray zeroSalt(32, 0);
    const QByteArray prk=hmacSha256(salt.isEmpty() ? zeroSalt : salt, ikm);

    QByteArray okm;
    QByteArray block;
    quint8 counter=1;

    while(okm.size()<length)
    {
        QByteArray input=block;
        input+=info;
        input.append(static_cast<char>(counter));

        block=hmacSha256(prk, input);
        okm+=block;

        if(counter==255)
            break;

        ++counter;
    }

    return okm.left(length);
}


QByteArray codeKey(const QString &normalizedCode)
{
    if(!isValidCode(normalizedCode))
        return QByteArray();

    const QByteArray raw=base32Decode(normalizedCode);

    if(raw.size()!=CodeBytes)
        return QByteArray();

    return sha256(QByteArrayLiteral("mytetra-pair-code-v1")+raw);
}


bool generateX25519(QByteArray &privateKey, QByteArray &publicKey)
{
    privateKey=randomBytes(EphBytes);

    if(privateKey.size()!=EphBytes)
        return false;

    publicKey.resize(EphBytes);

    crypto_x25519_public_key(
        reinterpret_cast<uint8_t *>(publicKey.data()),
        reinterpret_cast<const uint8_t *>(privateKey.constData()));

    return true;
}


bool x25519(const QByteArray &privateKey, const QByteArray &peerPublicKey,
            QByteArray &shared)
{
    if(privateKey.size()!=EphBytes || peerPublicKey.size()!=EphBytes)
        return false;

    shared.resize(EphBytes);

    crypto_x25519(reinterpret_cast<uint8_t *>(shared.data()),
                  reinterpret_cast<const uint8_t *>(privateKey.constData()),
                  reinterpret_cast<const uint8_t *>(peerPublicKey.constData()));

    // Low-order точка даёт все нули: такой обмен бесполезен и опасен
    static const QByteArray zero(EphBytes, 0);

    if(constantTimeEquals(shared, zero))
    {
        shared.fill(0);
        return false;
    }

    return true;
}


QByteArray aeadEncrypt(const QByteArray &key, const QByteArray &nonce,
                       const QByteArray &plain, const QByteArray &aad)
{
    if(key.size()!=KeyBytes || nonce.size()!=NonceBytes)
        return QByteArray();

    QByteArray cipher(plain.size(), 0);
    uint8_t mac[TagBytes];

    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(
        &ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        reinterpret_cast<const uint8_t *>(nonce.constData()));

    crypto_aead_write(
        &ctx,
        reinterpret_cast<uint8_t *>(cipher.data()),
        mac,
        reinterpret_cast<const uint8_t *>(aad.constData()),
        static_cast<size_t>(aad.size()),
        reinterpret_cast<const uint8_t *>(plain.constData()),
        static_cast<size_t>(plain.size()));

    crypto_wipe(&ctx, sizeof(ctx));

    QByteArray out=nonce;
    out+=cipher;
    out.append(reinterpret_cast<const char *>(mac), TagBytes);
    return out;
}


QByteArray aeadEncrypt(const QByteArray &key, const QByteArray &plain,
                       const QByteArray &aad)
{
    const QByteArray nonce=randomBytes(NonceBytes);
    return aeadEncrypt(key, nonce, plain, aad);
}


bool aeadDecrypt(const QByteArray &key, const QByteArray &blob,
                 const QByteArray &aad, QByteArray &plain)
{
    if(key.size()!=KeyBytes || blob.size()<NonceBytes+TagBytes)
        return false;

    const QByteArray nonce=blob.left(NonceBytes);
    const QByteArray cipher=blob.mid(NonceBytes,
                                     blob.size()-NonceBytes-TagBytes);
    const QByteArray tag=blob.right(TagBytes);

    QByteArray out(cipher.size(), 0);

    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(
        &ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        reinterpret_cast<const uint8_t *>(nonce.constData()));

    const int rc=crypto_aead_read(
        &ctx,
        reinterpret_cast<uint8_t *>(out.data()),
        reinterpret_cast<const uint8_t *>(tag.constData()),
        reinterpret_cast<const uint8_t *>(aad.constData()),
        static_cast<size_t>(aad.size()),
        reinterpret_cast<const uint8_t *>(cipher.constData()),
        static_cast<size_t>(cipher.size()));

    crypto_wipe(&ctx, sizeof(ctx));

    if(rc!=0)
    {
        plain.clear();
        return false;
    }

    plain=out;
    return true;
}


bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if(a.size()!=b.size())
        return false;

    quint8 diff=0;

    for(int i=0; i<a.size(); ++i)
        diff=static_cast<quint8>(diff
             | (static_cast<quint8>(a.at(i)) ^ static_cast<quint8>(b.at(i))));

    return diff==0;
}


QString toHex(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex());
}


QByteArray fromHex(const QString &hex)
{
    return QByteArray::fromHex(hex.toLatin1());
}


QString toBase64(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64());
}


QByteArray fromBase64(const QString &text)
{
    return QByteArray::fromBase64(text.toLatin1());
}

} // namespace SyncPairCrypto

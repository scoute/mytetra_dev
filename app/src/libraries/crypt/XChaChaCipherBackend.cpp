#include "XChaChaCipherBackend.h"

#include <QtEndian>
#include <QRandomGenerator>

#include "monocypher.h"
#include "libraries/helpers/DebugHelper.h"
#include "models/dataBaseConfig/DataBaseConfig.h"

extern DataBaseConfig dataBaseConfig;

extern DataBaseConfig dataBaseConfig;


// Параметры KDF продакшена: 64 МБ памяти, 3 прохода, 1 поток.
// Разовый лаг секунды на ввод пароля, перебору больно
static const quint32 kdfMemKb=65536;
static const quint8 kdfPasses=3;
static const quint8 kdfLanes=1;

// Раскладка конверта MTENC1
static const int magicSize=6;
static const int headerSize=6+1+1+4+1+1;
static const int wrapNonceSize=24;
static const int wrappedDekSize=48;
static const int dataNonceSize=24;
static const int tagSize=16;


// KEK сессии: Argon2id считается раз, дальше кэш.
// Сброс при смене пароля или мастер-соли
static QByteArray sessionKekKey;
static QByteArray sessionKekSalt;
static unsigned char sessionKekBuf[32];
static bool sessionKekValid=false;


static QByteArray randomBytes(int size)
{
    QByteArray out;
    out.resize(size);

    for(int i=0; i<size; i+=4)
    {
        const quint32 r=QRandomGenerator::system()->generate();

        for(int j=0; j<4 && i+j<size; ++j)
            out[i+j]=static_cast<char>((r >> (8*j)) & 0xff);
    }

    return out;
}


// Мастер-соль базы: генерируется один раз при первом шифровании,
// сразу синкается (потеря после шифрования = потеря данных)
static QByteArray masterSalt(void)
{
    QByteArray salt=QByteArray::fromBase64(dataBaseConfig.get_cryptSalt().toLatin1());

    if(salt.size()!=16)
    {
        salt=randomBytes(16);
        dataBaseConfig.set_cryptSalt(QString::fromLatin1(salt.toBase64()));
    }

    return salt;
}


static QByteArray sessionKek(const QByteArray &key)
{
    const QByteArray salt=masterSalt();

    if(sessionKekValid && sessionKekKey==key && sessionKekSalt==salt)
        return QByteArray(reinterpret_cast<char *>(sessionKekBuf), 32);

    crypto_wipe(sessionKekBuf, sizeof(sessionKekBuf));

    void *workArea=malloc(kdfMemKb*1024);

    if(workArea==nullptr)
        return QByteArray();

    crypto_argon2_config config={ CRYPTO_ARGON2_ID, kdfMemKb, kdfPasses, kdfLanes };
    crypto_argon2_inputs inputs={ reinterpret_cast<const uint8_t *>(key.constData()),
                                  reinterpret_cast<const uint8_t *>(salt.constData()),
                                  static_cast<uint32_t>(key.size()),
                                  static_cast<uint32_t>(salt.size()) };

    crypto_argon2(sessionKekBuf, sizeof(sessionKekBuf), workArea,
                  config, inputs, crypto_argon2_no_extras);

    free(workArea);

    sessionKekKey=key;
    sessionKekSalt=salt;
    sessionKekValid=true;

    return QByteArray(reinterpret_cast<char *>(sessionKekBuf), 32);
}


QString XChaChaCipherBackend::id(void) const
{
    return QString::fromLatin1(CIPHER_XCHACHA);
}


QByteArray XChaChaCipherBackend::encryptBytes(const QByteArray &key,
                                              const QByteArray &data)
{
    if(data.size()==0)
        return QByteArray();

    const QByteArray kek=sessionKek(key);

    // Тихий пустой результат на шифровании недопустим: вызыватель
    // запишет пустоту поверх данных. Падаем громко
    if(kek.size()!=32)
        criticalError("XChaChaCipherBackend::encryptBytes() : Can not derive KEK.");

    const QByteArray dek=randomBytes(32);
    const QByteArray wrapNonce=randomBytes(wrapNonceSize);
    const QByteArray dataNonce=randomBytes(dataNonceSize);

    // Шифротекст данных случайным DEK
    QByteArray ct(data.size()+tagSize, Qt::Uninitialized);
    unsigned char dataTag[tagSize];

    crypto_aead_lock(reinterpret_cast<unsigned char *>(ct.data()),
                     dataTag,
                     reinterpret_cast<const unsigned char *>(dek.constData()),
                     reinterpret_cast<const unsigned char *>(dataNonce.constData()),
                     nullptr, 0,
                     reinterpret_cast<const unsigned char *>(data.constData()),
                     static_cast<size_t>(data.size()));

    // DEK завернут KEK сессии
    unsigned char wrapped[wrappedDekSize];
    unsigned char wrapTag[tagSize];

    crypto_aead_lock(wrapped,
                     wrapTag,
                     reinterpret_cast<const unsigned char *>(kek.constData()),
                     reinterpret_cast<const unsigned char *>(wrapNonce.constData()),
                     nullptr, 0,
                     reinterpret_cast<const unsigned char *>(dek.constData()),
                     32);

    QByteArray blob;
    blob.append(MTENC1_MAGIC, magicSize);
    blob.append(static_cast<char>(0x01));
    blob.append(static_cast<char>(0x01));

    const int memPos=blob.size();
    blob.resize(blob.size()+4);
    qToLittleEndian<quint32>(kdfMemKb, reinterpret_cast<uchar *>(blob.data()+memPos));

    blob.append(static_cast<char>(kdfPasses));
    blob.append(static_cast<char>(kdfLanes));
    blob.append(wrapNonce);
    blob.append(reinterpret_cast<char *>(wrapped), 32);
    blob.append(reinterpret_cast<char *>(wrapTag), tagSize);
    blob.append(dataNonce);
    blob.append(ct.constData(), data.size());
    blob.append(reinterpret_cast<char *>(dataTag), tagSize);

    crypto_wipe(wrapped, sizeof(wrapped));
    crypto_wipe(wrapTag, sizeof(wrapTag));
    crypto_wipe(dataTag, sizeof(dataTag));

    QByteArray result=blob;
    crypto_wipe(const_cast<char *>(dek.constData()), dek.size());
    crypto_wipe(const_cast<char *>(kek.constData()), kek.size());

    return result;
}


QByteArray XChaChaCipherBackend::decryptBytes(const QByteArray &key,
                                              const QByteArray &data)
{
    if(data.size() < headerSize+wrapNonceSize+wrappedDekSize+dataNonceSize+tagSize)
        return QByteArray();

    const char *p=data.constData();

    if(memcmp(p, MTENC1_MAGIC, magicSize)!=0)
        return QByteArray();

    if(static_cast<unsigned char>(p[6])!=0x01 ||
       static_cast<unsigned char>(p[7])!=0x01)
        return QByteArray();

    // Параметры KDF пока константны; чужой набор честно отклоняем,
    // а не молча считаем другим KEK (миграция — делом будущего ver)
    if(qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(p+8))!=kdfMemKb ||
       static_cast<unsigned char>(p[12])!=kdfPasses ||
       static_cast<unsigned char>(p[13])!=kdfLanes)
        return QByteArray();

    int pos=headerSize;

    const QByteArray wrapNonce=data.mid(pos, wrapNonceSize);
    pos+=wrapNonceSize;

    const QByteArray wrapped=data.mid(pos, wrappedDekSize);
    pos+=wrappedDekSize;

    const QByteArray dataNonce=data.mid(pos, dataNonceSize);
    pos+=dataNonceSize;

    const int ctLen=data.size()-pos-tagSize;

    if(ctLen<0)
        return QByteArray();

    const QByteArray kek=sessionKek(key);

    if(kek.size()!=32)
        return QByteArray();

    // Разворачиваем DEK
    unsigned char dek[32];

    if(crypto_aead_unlock(dek,
                          reinterpret_cast<const unsigned char *>(wrapped.constData()+32),
                          reinterpret_cast<const unsigned char *>(kek.constData()),
                          reinterpret_cast<const unsigned char *>(wrapNonce.constData()),
                          nullptr, 0,
                          reinterpret_cast<const unsigned char *>(wrapped.constData()),
                          32)!=0)
    {
        crypto_wipe(dek, sizeof(dek));
        return QByteArray();
    }

    // Расшифровываем данные
    QByteArray plain(ctLen, Qt::Uninitialized);

    const int ok=crypto_aead_unlock(reinterpret_cast<unsigned char *>(plain.data()),
                                    reinterpret_cast<const unsigned char *>(data.constData()+data.size()-tagSize),
                                    dek,
                                    reinterpret_cast<const unsigned char *>(dataNonce.constData()),
                                    nullptr, 0,
                                    reinterpret_cast<const unsigned char *>(data.constData()+pos),
                                    static_cast<size_t>(ctLen));

    crypto_wipe(dek, sizeof(dek));

    if(ok!=0)
    {
        crypto_wipe(plain.data(), plain.size());
        return QByteArray();
    }

    return plain;
}

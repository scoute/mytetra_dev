#include "RC5CipherBackend.h"
#include "RC5Simple.h"
#include "CryptService.h"

#include <vector>

using std::vector;


QString RC5CipherBackend::id(void) const
{
    return QString::fromLatin1(CIPHER_RC5);
}


QByteArray RC5CipherBackend::encryptBytes(const QByteArray &key,
                                          const QByteArray &data)
{
    if(data.size()==0)
        return QByteArray();

    vector<unsigned char> vectorKey;
    CryptService::convertByteArrayToVector(key, vectorKey);

    vector<unsigned char> vectorDataIn;
    CryptService::convertByteArrayToVector(data, vectorDataIn);

    vector<unsigned char> vectorDataOut;

    RC5Simple rc5;
    rc5.RC5_SetKey(vectorKey);
    rc5.RC5_Encrypt(vectorDataIn, vectorDataOut);

    QByteArray result;
    CryptService::convertVectorToByteArray(vectorDataOut, result);

    return result;
}


QByteArray RC5CipherBackend::decryptBytes(const QByteArray &key,
                                          const QByteArray &data)
{
    if(data.size()==0)
        return QByteArray();

    vector<unsigned char> vectorKey;
    CryptService::convertByteArrayToVector(key, vectorKey);

    vector<unsigned char> vectorDataIn;
    CryptService::convertByteArrayToVector(data, vectorDataIn);

    vector<unsigned char> vectorDataOut;

    RC5Simple rc5;
    rc5.RC5_SetKey(vectorKey);
    rc5.RC5_Decrypt(vectorDataIn, vectorDataOut);

    QByteArray result;
    CryptService::convertVectorToByteArray(vectorDataOut, result);

    return result;
}

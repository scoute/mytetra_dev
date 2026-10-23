#include "RC5CipherBackend.h"
#include "RC5Simple.h"
#include "CryptService.h"
#include "libraries/helpers/DebugHelper.h"

#include <vector>

using std::vector;


// Длина ключа RC5 в этом формате фиксирована: 128 бит.
// Иначе RC5_SetKey выставляет ошибку, а шифрование шло бы нулевым ключом
static const int rc5KeySize=16;

// Минимальный корректный блоб: заголовок формата 1 (16 байт IV)
// плюс хотя бы один блок данных. Меньше - заведомо мусор, и разбор
// полез бы за границу вектора
static const int rc5MinDataSize=32;


QString RC5CipherBackend::id(void) const
{
    return QString::fromLatin1(CIPHER_RC5);
}


QByteArray RC5CipherBackend::encryptBytes(const QByteArray &key,
                                          const QByteArray &data)
{
    if(data.size()==0)
        return QByteArray();

    if(key.size()!=rc5KeySize)
        criticalError("RC5CipherBackend::encryptBytes() : Bad key size.");

    vector<unsigned char> vectorKey;
    CryptService::convertByteArrayToVector(key, vectorKey);

    vector<unsigned char> vectorDataIn;
    CryptService::convertByteArrayToVector(data, vectorDataIn);

    vector<unsigned char> vectorDataOut;

    RC5Simple rc5;
    rc5.RC5_SetKey(vectorKey);
    rc5.RC5_Encrypt(vectorDataIn, vectorDataOut);

    if(rc5.RC5_GetErrorCode()!=0 || vectorDataOut.empty())
        criticalError("RC5CipherBackend::encryptBytes() : Encryption failed.");

    QByteArray result;
    CryptService::convertVectorToByteArray(vectorDataOut, result);

    return result;
}


QByteArray RC5CipherBackend::decryptBytes(const QByteArray &key,
                                          const QByteArray &data)
{
    if(data.size()==0)
        return QByteArray();

    // Обрезанный/чужой блоб: RC5_Decrypt без проверки длины читает
    // IV и блок размера за границей вектора. Возвращаем отказ
    if(data.size()<rc5MinDataSize)
        return QByteArray();

    if(key.size()!=rc5KeySize)
        return QByteArray();

    vector<unsigned char> vectorKey;
    CryptService::convertByteArrayToVector(key, vectorKey);

    vector<unsigned char> vectorDataIn;
    CryptService::convertByteArrayToVector(data, vectorDataIn);

    vector<unsigned char> vectorDataOut;

    RC5Simple rc5;
    rc5.RC5_SetKey(vectorKey);
    rc5.RC5_Decrypt(vectorDataIn, vectorDataOut);

    if(rc5.RC5_GetErrorCode()!=0)
        return QByteArray();

    QByteArray result;
    CryptService::convertVectorToByteArray(vectorDataOut, result);

    return result;
}

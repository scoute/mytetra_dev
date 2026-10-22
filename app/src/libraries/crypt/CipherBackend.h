#ifndef _CIPHERBACKEND_H_
#define _CIPHERBACKEND_H_

#include <QString>
#include <QByteArray>

// Магия нового формата (конверт MTENC1: параметры, соль, нонс,
// завернутый DEK, шифротекст+тег). Расшифровка отличает шифры
// по ней, отдельных метаданных не нужно
static const char MTENC1_MAGIC[]="MTENC1";

// Идентификаторы шифров в настройке cryptMethod.
// rc5 оставлен дефолтом ради совместимости
static const char CIPHER_RC5[]="rc5";
static const char CIPHER_XCHACHA[]="xchacha";

// Шифробэкенд: байты туда-обратно. Строки (Base64), файлы
// и выбор по настройке живут в CryptService поверх
class CipherBackend
{

public:

    virtual ~CipherBackend(void) {}

    // Идентификатор для настройки и детекта
    virtual QString id(void) const=0;

    // Пустой результат расшифровки = неуспех (чужой тег/пароль).
    // Шифрование неуспеха не имеет: вход всегда валиден
    virtual QByteArray encryptBytes(const QByteArray &key,
                                    const QByteArray &data)=0;
    virtual QByteArray decryptBytes(const QByteArray &key,
                                    const QByteArray &data)=0;
};

#endif /* _CIPHERBACKEND_H_ */

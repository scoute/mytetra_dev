#ifndef _RC5CIPHERBACKEND_H_
#define _RC5CIPHERBACKEND_H_

#include "CipherBackend.h"

// Исторический шифр MyTetra (RC5, блочный, без аутентификации).
// Оставлен ради совместимости и стоит дефолтом в cryptMethod.
// Байтовая логика перенесена из CryptService без изменений,
// формат блобов не менялся: старые данные читаются как раньше

class RC5CipherBackend : public CipherBackend
{

public:

    QString id(void) const override;
    QByteArray encryptBytes(const QByteArray &key,
                            const QByteArray &data) override;
    QByteArray decryptBytes(const QByteArray &key,
                            const QByteArray &data) override;
};

#endif /* _RC5CIPHERBACKEND_H_ */

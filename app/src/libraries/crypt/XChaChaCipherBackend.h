#ifndef _XCHACHACIPHERBACKEND_H_
#define _XCHACHACIPHERBACKEND_H_

#include "CipherBackend.h"

// Современный шифр MyTetra: XChaCha20-Poly1305 (Monocypher) + Argon2id.
// Формат блоба (конверт MTENC1), все целые little-endian:
//   "MTENC1" | ver=1 | kdf=1 | kdf_mem_kb u32 | kdf_passes u8 | kdf_lanes u8 |
//   wrap_nonce[24] | wrapped_dek[48] | data_nonce[24] | ct + tag[16]
// KEK = Argon2id(ключ, мастер-соль базы), кэшируется на сессию.
// DEK случаен на файл: смена пароля = перезавернуть DEKи.
class XChaChaCipherBackend : public CipherBackend
{

public:

    QString id(void) const override;
    QByteArray encryptBytes(const QByteArray &key,
                            const QByteArray &data) override;
    QByteArray decryptBytes(const QByteArray &key,
                            const QByteArray &data) override;
};

#endif /* _XCHACHACIPHERBACKEND_H_ */

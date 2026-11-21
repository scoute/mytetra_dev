#ifndef SYNCPAIRCRYPTO_H
#define SYNCPAIRCRYPTO_H

#include <QByteArray>
#include <QString>

// Криптография спаривания sync/2.
//
// Примитивы — стандартные, без самодельной математики:
//   X25519 (RFC 7748)          — эфемерный обмен ключами
//   HKDF-SHA256 (RFC 5869)     — вывод ключей из общего секрета
//   ChaCha20-Poly1305 (RFC 8439) — шифрование долговременного секрета
//
// X25519 и ChaCha20-Poly1305 берём из вендоренного Monocypher
// (thirdParty/monocypher), SHA-256/HMAC — из Qt. Те же примитивы на
// Android: Tink (X25519, Hkdf, ChaCha20Poly1305). Совместимость
// доказывается тестами на RFC-векторах, а не общим кодом.
namespace SyncPairCrypto
{

// 16 символов base32 = 80 бит. Это одноразовый код спаривания:
// он не является долговременным секретом и живёт минуты
constexpr int CodeChars=16;
constexpr int CodeBytes=10;
constexpr int KeyBytes=32;
constexpr int NonceBytes=12;
constexpr int TagBytes=16;
constexpr int MacBytes=32;
constexpr int EphBytes=32;

// Криптостойкие случайные байты (QRandomGenerator::system)
QByteArray randomBytes(int count);

// 32 случайных байта hex (64 символа) — долговременный секрет пары
QString randomSecretHex(void);

// Одноразовый код: 10 случайных байт -> 16 base32-символов
QString generateCode(void);

// Привести ввод пользователя к канону: верхний регистр, без пробелов,
// дефисов и подчёркиваний. Пусто — невалидный код
QString normalizeCode(const QString &input);
bool isValidCode(const QString &normalized);

// Показать код группами по 4: ABCD-EFGH-IJKL-MNOP
QString formatCode(const QString &normalized);

// RFC 4648 base32 без padding (10 байт <-> 16 символов)
QString base32Encode(const QByteArray &bytes);
QByteArray base32Decode(const QString &code);

QByteArray sha256(const QByteArray &data);
QByteArray hmacSha256(const QByteArray &key, const QByteArray &data);

// RFC 5869. salt может быть пустым (тогда нули длины hash)
QByteArray hkdfSha256(const QByteArray &ikm, const QByteArray &salt,
                      const QByteArray &info, int length);

// Ключ из одноразового кода: SHA256("mytetra-pair-code-v1" || codeBytes)
QByteArray codeKey(const QString &normalizedCode);

// Эфемерная пара X25519
bool generateX25519(QByteArray &privateKey, QByteArray &publicKey);

// Общий секрет. false — неверные размеры или low-order точка (все нули)
bool x25519(const QByteArray &privateKey, const QByteArray &peerPublicKey,
            QByteArray &shared);

// AEAD: возвращает nonce(12) || ciphertext || tag(16) — ровно тот же
// формат, что отдаёт Tink ChaCha20Poly1305 на Android.
// Перегрузка с явным nonce нужна для RFC-векторов в тестах.
QByteArray aeadEncrypt(const QByteArray &key, const QByteArray &nonce,
                       const QByteArray &plain, const QByteArray &aad);
QByteArray aeadEncrypt(const QByteArray &key, const QByteArray &plain,
                       const QByteArray &aad);

// Разобрать nonce||ciphertext||tag и проверить тег
bool aeadDecrypt(const QByteArray &key, const QByteArray &blob,
                 const QByteArray &aad, QByteArray &plain);

// Сравнение без раннего выхода (MAC/теги)
bool constantTimeEquals(const QByteArray &a, const QByteArray &b);

QString toHex(const QByteArray &data);
QByteArray fromHex(const QString &hex);
QString toBase64(const QByteArray &data);
QByteArray fromBase64(const QString &text);

} // namespace SyncPairCrypto

#endif // SYNCPAIRCRYPTO_H

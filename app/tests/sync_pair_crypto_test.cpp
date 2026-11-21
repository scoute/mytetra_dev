#include <QByteArray>
#include <QString>

#include <cstdio>

#include "SyncPairCrypto.h"

// Векторы RFC: X25519 (7748), HKDF-SHA256 (5869),
// ChaCha20-Poly1305 (8439), base32 (4648). Те же байты обязаны
// получить C++ и Kotlin — иначе протокол спаривания несовместим.
//
//   make -C app/tests run

static int failures=0;

#define CHECK(cond) do { \
    if(!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        ++failures; \
    } \
} while(0)

#define CHECK_HEX(actual, expected) do { \
    const QString a=SyncPairCrypto::toHex(actual); \
    const QString e=QStringLiteral(expected); \
    if(a!=e) { \
        std::fprintf(stderr, "FAIL %s:%d\n  got  %s\n  want %s\n", \
                     __FILE__, __LINE__, qPrintable(a), qPrintable(e)); \
        ++failures; \
    } \
} while(0)


int main(void)
{
    // --- base32: RFC 4648 без padding ---
    CHECK(SyncPairCrypto::base32Encode(QByteArrayLiteral("foo"))
          ==QStringLiteral("MZXW6"));
    CHECK(SyncPairCrypto::base32Encode(QByteArrayLiteral("foobar"))
          ==QStringLiteral("MZXW6YTBOI"));
    CHECK(SyncPairCrypto::base32Decode(QStringLiteral("MZXW6"))
          ==QByteArrayLiteral("foo"));
    CHECK(SyncPairCrypto::base32Decode(QStringLiteral("MZXW6YTBOI"))
          ==QByteArrayLiteral("foobar"));

    // 10 байт <-> 16 символов (ровно код спаривания)
    {
        QByteArray raw;
        for(int i=0; i<10; ++i)
            raw.append(static_cast<char>(i));

        const QString encoded=SyncPairCrypto::base32Encode(raw);
        CHECK(encoded==QStringLiteral("AAAQEAYEAUDAOCAJ"));
        CHECK(SyncPairCrypto::base32Decode(encoded)==raw);
    }

    // --- код: канонизация и формат ---
    {
        const QString code=QStringLiteral("abcd-efgh ijkl_mnop");
        CHECK(SyncPairCrypto::normalizeCode(code)
              ==QStringLiteral("ABCDEFGHIJKLMNOP"));
        CHECK(SyncPairCrypto::isValidCode(QStringLiteral("ABCDEFGHIJKLMNOP")));
        CHECK(!SyncPairCrypto::isValidCode(QStringLiteral("ABCDEFGHIJKLMNO0")));
        CHECK(!SyncPairCrypto::isValidCode(QStringLiteral("SHORT")));
        CHECK(SyncPairCrypto::formatCode(QStringLiteral("ABCDEFGHIJKLMNOP"))
              ==QStringLiteral("ABCD-EFGH-IJKL-MNOP"));
    }

    // --- ключ из кода ---
    {
        const QByteArray key=SyncPairCrypto::codeKey(
            QStringLiteral("ABCDEFGHIJKLMNOP"));
        CHECK_HEX(key, "b5acd61fdbf43e9d8b5c4cb7d1eb76e8"
                       "3c5da06c3813a6139fe832ec3f9ec74b");
    }

    // --- HKDF-SHA256: RFC 5869 Test Case 1 ---
    {
        QByteArray ikm;
        for(int i=0; i<22; ++i)
            ikm.append(static_cast<char>(0x0b));

        const QByteArray okm=SyncPairCrypto::hkdfSha256(
            ikm,
            SyncPairCrypto::fromHex(QStringLiteral("000102030405060708090a0b0c")),
            SyncPairCrypto::fromHex(QStringLiteral("f0f1f2f3f4f5f6f7f8f9")),
            42);

        CHECK_HEX(okm, "3cb25f25faacd57a90434f64d0362f2a"
                       "2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
                       "34007208d5b887185865");
    }

    // --- X25519: RFC 7748 §6.1 ---
    {
        const QByteArray alicePriv=SyncPairCrypto::fromHex(
            QStringLiteral("77076d0a7318a57d3c16c17251b26645"
                           "df4c2f87ebc0992ab177fba51db92c2a"));
        const QByteArray alicePub=SyncPairCrypto::fromHex(
            QStringLiteral("8520f0098930a754748b7ddcb43ef75a"
                           "0dbf3a0d26381af4eba4a98eaa9b4e6a"));
        const QByteArray bobPriv=SyncPairCrypto::fromHex(
            QStringLiteral("5dab087e624a8a4b79e17f8b83800ee6"
                           "6f3bb1292618b6fd1c2f8b27ff88e0eb"));
        const QByteArray bobPub=SyncPairCrypto::fromHex(
            QStringLiteral("de9edb7d7b7dc1b4d35b61c2ece43537"
                           "3f8343c85b78674dadfc7e146f882b4f"));
        const QByteArray expected=SyncPairCrypto::fromHex(
            QStringLiteral("4a5d9d5ba4ce2de1728e3bf480350f25"
                           "e07e21c947d19e3376f09b3c1e161742"));

        QByteArray shared;
        CHECK(SyncPairCrypto::x25519(alicePriv, bobPub, shared));
        CHECK_HEX(shared, "4a5d9d5ba4ce2de1728e3bf480350f25"
                          "e07e21c947d19e3376f09b3c1e161742");
        CHECK(shared==expected);

        QByteArray shared2;
        CHECK(SyncPairCrypto::x25519(bobPriv, alicePub, shared2));
        CHECK(shared2==expected);
    }

    // --- ChaCha20-Poly1305: RFC 8439 §2.8.2 ---
    {
        const QByteArray key=SyncPairCrypto::fromHex(
            QStringLiteral("808182838485868788898a8b8c8d8e8f"
                           "909192939495969798999a9b9c9d9e9f"));
        const QByteArray nonce=SyncPairCrypto::fromHex(
            QStringLiteral("070000004041424344454647"));
        const QByteArray aad=SyncPairCrypto::fromHex(
            QStringLiteral("50515253c0c1c2c3c4c5c6c7"));
        const QByteArray plain=QByteArrayLiteral(
            "Ladies and Gentlemen of the class of '99: If I could "
            "offer you only one tip for the future, sunscreen would be it.");
        const QByteArray expected=SyncPairCrypto::fromHex(
            QStringLiteral("070000004041424344454647"
                           "d31a8d34648e60db7b86afbc53ef7ec2"
                           "a4aded51296e08fea9e2b5a736ee62d6"
                           "3dbea45e8ca9671282fafb69da92728b"
                           "1a71de0a9e060b2905d6a5b67ecd3b36"
                           "92ddbd7f2d778b8c9803aee328091b58"
                           "fab324e4fad675945585808b4831d7bc"
                           "3ff4def08e4b7a9de576d26586cec64b"
                           "6116"
                           "1ae10b594f09e26a7e902ecbd0600691"));

        const QByteArray blob=SyncPairCrypto::aeadEncrypt(
            key, nonce, plain, aad);
        CHECK(blob==expected);

        QByteArray decrypted;
        CHECK(SyncPairCrypto::aeadDecrypt(key, expected, aad, decrypted));
        CHECK(decrypted==plain);

        // Подмена тега/шифртекста обязана провалиться
        QByteArray broken=expected;
        broken[broken.size()-1]=static_cast<char>(
            static_cast<quint8>(broken.at(broken.size()-1)) ^ 0x01);
        QByteArray dummy;
        CHECK(!SyncPairCrypto::aeadDecrypt(key, broken, aad, dummy));

        QByteArray brokenAad=aad;
        brokenAad[0]=static_cast<char>(
            static_cast<quint8>(brokenAad.at(0)) ^ 0x01);
        CHECK(!SyncPairCrypto::aeadDecrypt(key, expected, brokenAad, dummy));
    }

    // --- генерация/обмен: обе стороны сходятся ---
    {
        QByteArray aPriv, aPub, bPriv, bPub;
        CHECK(SyncPairCrypto::generateX25519(aPriv, aPub));
        CHECK(SyncPairCrypto::generateX25519(bPriv, bPub));
        CHECK(aPriv.size()==32 && aPub.size()==32);
        CHECK(aPub!=bPub);

        QByteArray ab, ba;
        CHECK(SyncPairCrypto::x25519(aPriv, bPub, ab));
        CHECK(SyncPairCrypto::x25519(bPriv, aPub, ba));
        CHECK(SyncPairCrypto::constantTimeEquals(ab, ba));
    }

    if(failures==0)
        std::printf("sync_pair_crypto_test OK\n");

    return failures==0 ? 0 : 1;
}

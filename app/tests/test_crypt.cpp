// Пути шифрования/расшифровки на QTest, линия v2 (см. 496).
// RC5, XChaCha, перекрестные чтения, неверный пароль, пустые входы.
// v2-набор: без decryptTextFile и UTF-8-трипвайра (фича 511 — только v1).
// criticalError-пути не гоняются: они роняют процесс по дизайну.
// Глобалы — extern из main.cpp (в v2 нет AppContext).
#include <QtTest>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "libraries/crypt/CryptService.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/appConfig/AppConfig.h"

extern AppConfig mytetraConfig;
extern DataBaseConfig dataBaseConfig;

class TestCrypt : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase(void);
    void cleanupTestCase(void);

    void rc5Roundtrip(void);
    void xchachaRoundtrip(void);
    void crossReads(void);
    void wrongKeyEmpty(void);
    void detectMethod(void);
    void emptyInputs(void);
    void badConfigFallback(void);
    void rc5NeverSilentNoop(void);
    void nullableCheckData(void);

private:

    QTemporaryDir m_tmp;
    QSettings *m_settings=nullptr;

    const QByteArray m_key="0123456789abcdef";
    const QByteArray m_badKey="fedcba9876543210";
    const QByteArray m_text="The quick brown fox jumps over the lazy dog. 1234567890! "
                            "Быстрая коричневая лиса прыгает через ленивую собаку.";

    QByteArray m_rc5blob;
    QByteArray m_xblob;
};

void TestCrypt::initTestCase(void)
{
    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_crypt.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;

    // Изолированная база: init создает database.ini,
    // первое XChaCha-шифрование допишет туда соль
    const QString dbDir=m_tmp.filePath(QStringLiteral("cryptdb"));
    QVERIFY2(QDir().mkpath(dbDir), "crypt db dir must be created");
    m_settings->setValue(QStringLiteral("tetradir"), dbDir);
    dataBaseConfig.init();
}

void TestCrypt::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestCrypt::rc5Roundtrip(void)
{
    mytetraConfig.set_cryptMethod(QStringLiteral("rc5"));
    m_rc5blob=CryptService::encryptByteArray(m_key, m_text);
    QVERIFY2(!m_rc5blob.isEmpty(), "RC5 must produce output");
    QVERIFY2(m_rc5blob!=m_text, "RC5 output must differ from input");
    QCOMPARE(CryptService::decryptByteArray(m_key, m_rc5blob), m_text);
}

void TestCrypt::xchachaRoundtrip(void)
{
    mytetraConfig.set_cryptMethod(QStringLiteral("xchacha"));
    m_xblob=CryptService::encryptByteArray(m_key, m_text);
    QVERIFY2(!m_xblob.isEmpty(), "XChaCha must produce output");
    QVERIFY2(m_xblob!=m_text, "XChaCha output must differ from input");
    QVERIFY2(m_xblob.startsWith("MTENC1"), "XChaCha blob must carry MTENC1 magic");
    QCOMPARE(CryptService::decryptByteArray(m_key, m_xblob), m_text);
}

void TestCrypt::crossReads(void)
{
    // Шифр RC5, конфиг уже XChaCha (миграция читает старое)
    mytetraConfig.set_cryptMethod(QStringLiteral("xchacha"));
    QCOMPARE(CryptService::decryptByteArray(m_key, m_rc5blob), m_text);

    // Шифр XChaCha, конфиг откачен на RC5
    mytetraConfig.set_cryptMethod(QStringLiteral("rc5"));
    QCOMPARE(CryptService::decryptByteArray(m_key, m_xblob), m_text);
}

void TestCrypt::wrongKeyEmpty(void)
{
    mytetraConfig.set_cryptMethod(QStringLiteral("xchacha"));
    QVERIFY2(CryptService::decryptByteArray(m_badKey, m_xblob).isEmpty(),
             "XChaCha with wrong key must return empty");
}

void TestCrypt::detectMethod(void)
{
    QCOMPARE(CryptService::detectMethod(m_xblob), QStringLiteral("xchacha"));
    QCOMPARE(CryptService::detectMethod(m_rc5blob), QStringLiteral("rc5"));
    QCOMPARE(CryptService::detectMethod(m_text), QStringLiteral("rc5"));
}

void TestCrypt::emptyInputs(void)
{
    QVERIFY2(CryptService::encryptByteArray(m_key, QByteArray()).isEmpty(),
             "encrypt of empty must be empty");
    QVERIFY2(CryptService::decryptByteArray(m_key, QByteArray()).isEmpty(),
             "decrypt of empty must be empty");
}

void TestCrypt::badConfigFallback(void)
{
    // Мусор в конфиге: откат на RC5, никогда не молча в никуда
    m_settings->setValue(QStringLiteral("cryptMethod"), QStringLiteral("rot13"));
    QCOMPARE(mytetraConfig.get_cryptMethod(), QStringLiteral("rc5"));
    mytetraConfig.set_cryptMethod(QStringLiteral("xchacha"));
}

void TestCrypt::rc5NeverSilentNoop(void)
{
    // Расшифровка открытого текста никогда не возвращает его же как есть
    QByteArray noise=CryptService::decryptByteArray(m_key, m_text);
    QVERIFY2(noise!=m_text, "RC5 decrypt of plaintext must never echo input");
}

void TestCrypt::nullableCheckData(void)
{
    // Пустые проверочные ключи - норма для базы без сохраненного
    // пароля. Геттеры обязаны вернуть "", а не уронить процесс
    // через criticalError (как падало в reencrypt)
    QCOMPARE(dataBaseConfig.get_middle_hash_check_data(), QString());
    QCOMPARE(dataBaseConfig.get_crypt_check_salt(), QString());
    QCOMPARE(dataBaseConfig.get_crypt_check_hash(), QString());

    QVERIFY2(QByteArray::fromBase64(dataBaseConfig.get_middle_hash_check_data().toLatin1()).isEmpty(),
             "empty check data must stay empty");
}

QTEST_APPLESS_MAIN(TestCrypt)

#include "test_crypt.moc"

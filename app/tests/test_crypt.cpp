// Пути шифрования/расшифровки на QTest (см. 511, 516).
// Порт cryptprobe: RC5, XChaCha, перекрестные чтения,
// неверный пароль, пустые входы, UTF-8-трипвайр, decryptTextFile.
// criticalError-пути не гоняются: они роняют процесс по дизайну.
// Фикстура программная: QTemporaryDir + QSettings внутри теста.
#include <QtTest>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include "libraries/crypt/CryptService.h"
#include "libraries/AppContext.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/TrashMonitoring.h"
#include "libraries/WalkHistory.h"
#include "libraries/VisitHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/ShortcutManager.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/InternalClipboard.h"
#include "models/appConfig/AppFiles.h"
#include "models/dataBaseConfig/DataBaseConfig.h"
#include "models/appConfig/AppConfig.h"
#include "models/appConfig/AppConfigCrypto.h"

extern ShortcutManager shortcutManager;

extern AppConfig mytetraConfig;

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
    void utf8Tripwire(void);
    void textFileRoundtrip(void);

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
    static FixedParameters *fp=new FixedParameters;
    static GlobalParameters *gp=new GlobalParameters;
    static AppFiles *af=new AppFiles;
    static DataBaseConfig *dbc=new DataBaseConfig;
    static TrashMonitoring *tm=new TrashMonitoring;
    static WalkHistory *wh=new WalkHistory;
    static VisitHistory *vh=new VisitHistory;
    static ActionLogger *al=new ActionLogger;
    static PeriodicCheckBase *pcb=new PeriodicCheckBase;
    static PeriodicSyncro *ps=new PeriodicSyncro;
    static InternalClipboard *ic=new InternalClipboard;
    AppContext::instance().init(fp, gp, &mytetraConfig, af, dbc, tm,
                                wh, vh, al, &shortcutManager, pcb, ps,
                                ic, nullptr, &gp->cryptHolder());

    QVERIFY2(m_tmp.isValid(), "QTemporaryDir must be created");

    m_settings=new QSettings(m_tmp.filePath(QStringLiteral("test_crypt.ini")),
                             QSettings::IniFormat, this);
    mytetraConfig.m_conf=m_settings;

    // Изолированная база: init создает database.ini,
    // первое XChaCha-шифрование допишет туда соль
    const QString dbDir=m_tmp.filePath(QStringLiteral("cryptdb"));
    QVERIFY2(QDir().mkpath(dbDir), "crypt db dir must be created");
    m_settings->setValue(QStringLiteral("tetradir"), dbDir);
    dbc->init();
}

void TestCrypt::cleanupTestCase(void)
{
    mytetraConfig.m_conf=nullptr;
}

void TestCrypt::rc5Roundtrip(void)
{
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("rc5"));
    m_rc5blob=CryptService::encryptByteArray(m_key, m_text);
    QVERIFY2(!m_rc5blob.isEmpty(), "RC5 must produce output");
    QVERIFY2(m_rc5blob!=m_text, "RC5 output must differ from input");
    QCOMPARE(CryptService::decryptByteArray(m_key, m_rc5blob), m_text);
}

void TestCrypt::xchachaRoundtrip(void)
{
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
    m_xblob=CryptService::encryptByteArray(m_key, m_text);
    QVERIFY2(!m_xblob.isEmpty(), "XChaCha must produce output");
    QVERIFY2(m_xblob!=m_text, "XChaCha output must differ from input");
    QVERIFY2(m_xblob.startsWith("MTENC1"), "XChaCha blob must carry MTENC1 magic");
    QCOMPARE(CryptService::decryptByteArray(m_key, m_xblob), m_text);
}

void TestCrypt::crossReads(void)
{
    // Шифр RC5, конфиг уже XChaCha (миграция читает старое)
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
    QCOMPARE(CryptService::decryptByteArray(m_key, m_rc5blob), m_text);

    // Шифр XChaCha, конфиг откачен на RC5
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("rc5"));
    QCOMPARE(CryptService::decryptByteArray(m_key, m_xblob), m_text);
}

void TestCrypt::wrongKeyEmpty(void)
{
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
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
    QCOMPARE(AppContext::instance().appConfig()->cryptoSection()->get_cryptMethod(),
             QStringLiteral("rc5"));
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
}

void TestCrypt::rc5NeverSilentNoop(void)
{
    // Расшифровка открытого текста никогда не возвращает его же как есть
    QByteArray noise=CryptService::decryptByteArray(m_key, m_text);
    QVERIFY2(noise!=m_text, "RC5 decrypt of plaintext must never echo input");
}

void TestCrypt::utf8Tripwire(void)
{
    QVERIFY2(CryptService::isValidUtf8(m_text), "text must be valid UTF-8");
    QVERIFY2(!CryptService::isValidUtf8(QByteArray::fromHex("fffe0041")),
             "binary must be invalid UTF-8");
    QVERIFY2(CryptService::isValidUtf8(QByteArray()), "empty must be valid UTF-8");
}

void TestCrypt::textFileRoundtrip(void)
{
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));

    const QString fpath=m_tmp.filePath(QStringLiteral("note.html"));
    QFile f(fpath);
    QVERIFY2(f.open(QIODevice::WriteOnly), "fixture file must be writable");
    f.write(CryptService::encryptByteArray(m_key, m_text));
    f.close();

    CryptService::decryptTextFile(m_key, fpath);

    QFile g(fpath);
    QVERIFY2(g.open(QIODevice::ReadOnly), "decrypted file must be readable");
    QCOMPARE(g.readAll(), m_text);
}

QTEST_APPLESS_MAIN(TestCrypt)

#include "test_crypt.moc"

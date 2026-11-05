// Пути шифрования/расшифровки: RC5, XChaCha, перекрестные,
// неверный пароль, пустые входы, UTF-8-трипвайр (см. 511).
// criticalError-пути не гоняются: они роняют процесс по дизайну.
#include <QCoreApplication>
#include <QDebug>
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

static int fails=0;

static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;

    if(!ok)
        ++fails;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    static FixedParameters fp;
    static GlobalParameters gp;
    static AppFiles af;
    static DataBaseConfig dbc;
    static TrashMonitoring tm;
    static WalkHistory wh;
    static VisitHistory vh;
    static ActionLogger al;
    static PeriodicCheckBase pcb;
    static PeriodicSyncro ps;
    static InternalClipboard ic;
    AppContext::instance().init(&fp, &gp, &mytetraConfig, &af, &dbc, &tm,
                                &wh, &vh, &al, &shortcutManager, &pcb, &ps,
                                &ic, nullptr, &gp.cryptHolder());

    QFile::remove(QStringLiteral("/tmp/opencode/probe_crypt.ini"));
    QFile::copy(QStringLiteral("/tmp/opencode/bktinystand/conf.ini"),
                QStringLiteral("/tmp/opencode/probe_crypt.ini"));
    mytetraConfig.m_conf=new QSettings(QStringLiteral("/tmp/opencode/probe_crypt.ini"),
                                       QSettings::IniFormat);

    // Изолированная база для пробы: init создает database.ini,
    // первое XChaCha-шифрование допишет туда соль. Фикстуры не трогаем
    QDir().mkpath(QStringLiteral("/tmp/opencode/probe_crypt_db"));
    mytetraConfig.m_conf->setValue(QStringLiteral("tetradir"),
                                   QStringLiteral("/tmp/opencode/probe_crypt_db"));
    dbc.init();

    const QByteArray key("0123456789abcdef");
    const QByteArray badKey("fedcba9876543210");
    const QByteArray text("The quick brown fox jumps over the lazy dog. 1234567890! "
                          "Быстрая коричневая лиса прыгает через ленивую собаку.");

    // RC5 кругом
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("rc5"));
    QByteArray rc5blob=CryptService::encryptByteArray(key, text);
    check(!rc5blob.isEmpty() && rc5blob!=text, "CRYPT-RC5-ENCRYPTS");
    check(CryptService::decryptByteArray(key, rc5blob)==text, "CRYPT-RC5-ROUNDTRIP");

    // XChaCha кругом
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
    QByteArray xblob=CryptService::encryptByteArray(key, text);
    check(!xblob.isEmpty() && xblob!=text, "CRYPT-X-ENCRYPTS");
    check(xblob.startsWith("MTENC1"), "CRYPT-X-MAGIC");
    check(CryptService::decryptByteArray(key, xblob)==text, "CRYPT-X-ROUNDTRIP");

    // Перекрестно: шифр RC5, конфиг уже XChaCha (миграция читает старое)
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
    check(CryptService::decryptByteArray(key, rc5blob)==text, "CRYPT-X-CONFIG-READS-RC5");

    // Перекрестно: шифр XChaCha, конфиг откачен на RC5
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("rc5"));
    check(CryptService::decryptByteArray(key, xblob)==text, "CRYPT-RC5-CONFIG-READS-X");

    // Неверный пароль: XChaCha честно пусто
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));
    check(CryptService::decryptByteArray(badKey, xblob).isEmpty(), "CRYPT-X-WRONGKEY-EMPTY");

    // Детект по магии, не по конфигу
    check(CryptService::detectMethod(xblob)==QStringLiteral("xchacha"), "CRYPT-DETECT-X");
    check(CryptService::detectMethod(rc5blob)==QStringLiteral("rc5"), "CRYPT-DETECT-RC5");
    check(CryptService::detectMethod(text)==QStringLiteral("rc5"), "CRYPT-DETECT-PLAIN-AS-RC5");

    // Пустые входы
    check(CryptService::encryptByteArray(key, QByteArray()).isEmpty(), "CRYPT-EMPTY-ENC");
    check(CryptService::decryptByteArray(key, QByteArray()).isEmpty(), "CRYPT-EMPTY-DEC");

    // Мусор в конфиге: откат на RC5, никогда не молча в никуда
    mytetraConfig.m_conf->setValue(QStringLiteral("cryptMethod"), QStringLiteral("rot13"));
    check(AppContext::instance().appConfig()->cryptoSection()->get_cryptMethod()==QStringLiteral("rc5"),
          "CRYPT-BADCONFIG-FALLBACK-RC5");
    AppContext::instance().appConfig()->cryptoSection()->set_cryptMethod(QStringLiteral("xchacha"));

    // Зафиксированное поведение RC5: расшифровка открытого текста никогда
    // не возвращает его же как есть (тихий no-op исключен). Пустота тоже
    // возможна (проверка размера): оба исхода ловит трипвайр вызывателя
    QByteArray noise=CryptService::decryptByteArray(key, text);
    check(noise!=text, "CRYPT-RC5-NEVER-SILENT-NOOP");

    // UTF-8-трипвайр на детерминированных данных
    check(CryptService::isValidUtf8(text), "CRYPT-UTF8-TEXT-VALID");
    check(!CryptService::isValidUtf8(QByteArray::fromHex("fffe0041")), "CRYPT-UTF8-BINARY-INVALID");
    check(CryptService::isValidUtf8(QByteArray()), "CRYPT-UTF8-EMPTY-VALID");

    // decryptTextFile: счастливый путь пишет исходник обратно
    QTemporaryDir tmp;
    check(tmp.isValid(), "CRYPT-TMPDIR");
    const QString fpath=tmp.filePath(QStringLiteral("note.html"));
    QFile f(fpath);
    check(f.open(QIODevice::WriteOnly), "CRYPT-FIXTURE");
    f.write(CryptService::encryptByteArray(key, text));
    f.close();
    CryptService::decryptTextFile(key, fpath);
    QFile g(fpath);
    check(g.open(QIODevice::ReadOnly), "CRYPT-READBACK");
    check(g.readAll()==text, "CRYPT-TEXTFILE-ROUNDTRIP");

    if(fails==0)
        qDebug() << "PROBE-OK crypt";

    return 0;
}

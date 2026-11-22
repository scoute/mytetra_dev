#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QSslKey>
#include <QSslSocket>
#include <QTemporaryFile>

#include "SyncTls.h"


namespace SyncTls
{

QString certificateFingerprint(const QSslCertificate &cert)
{
    if(cert.isNull())
        return QString();

    return QString::fromLatin1(
        QCryptographicHash::hash(cert.toDer(),
                                 QCryptographicHash::Sha256).toHex());
}


QString certificateFingerprint(const QString &certPath)
{
    const QList<QSslCertificate> chain=
        QSslCertificate::fromPath(certPath, QSsl::Pem);

    if(chain.isEmpty())
        return QString();

    return certificateFingerprint(chain.first());
}


bool loadServerConfig(const QString &certPath, const QString &keyPath,
                      QSslConfiguration &config, QString *fingerprint,
                      QString *error)
{
    const auto fail=[&](const QString &text) -> bool {
        if(error!=nullptr)
            *error=text;

        return false;
    };

    if(certPath.trimmed().isEmpty() || keyPath.trimmed().isEmpty())
        return fail(QStringLiteral("TLS: certificate/key paths are empty"));

    QList<QSslCertificate> chain=
        QSslCertificate::fromPath(certPath, QSsl::Pem);

    if(chain.isEmpty())
        chain=QSslCertificate::fromPath(certPath, QSsl::Der);

    if(chain.isEmpty())
        return fail(QStringLiteral("TLS: cannot read certificate %1")
                        .arg(certPath));

    QFile keyFile(keyPath);

    if(!keyFile.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("TLS: cannot open private key %1")
                        .arg(keyPath));

    const QByteArray keyData=keyFile.readAll();
    keyFile.close();

    QSslKey key(keyData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey,
                QByteArray());

    if(key.isNull())
        key=QSslKey(keyData, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey,
                    QByteArray());

    if(key.isNull())
        key=QSslKey(keyData, QSsl::Rsa, QSsl::Der, QSsl::PrivateKey,
                    QByteArray());

    if(key.isNull())
        return fail(QStringLiteral("TLS: cannot read private key %1 "
                                   "(PEM/DER, без passphrase)")
                        .arg(keyPath));

    config=QSslConfiguration::defaultConfiguration();
    config.setLocalCertificateChain(chain);
    config.setPrivateKey(key);
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    config.setProtocol(QSsl::TlsV1_2OrLater);

    if(fingerprint!=nullptr)
        *fingerprint=certificateFingerprint(chain.first());

    if(error!=nullptr)
        error->clear();

    return true;
}


QString primaryAddress(void)
{
    for(const QHostAddress &address : QNetworkInterface::allAddresses())
    {
        if(address.protocol()!=QAbstractSocket::IPv4Protocol)
            continue;

        if(address.isLoopback() || address.isNull())
            continue;

        return address.toString();
    }

    return QString();
}


bool ensureSelfSigned(const QString &dir, const QString &host,
                      QString *certPath, QString *keyPath,
                      QString *error, bool force)
{
    const auto fail=[&](const QString &text) -> bool {
        if(error!=nullptr)
            *error=text;

        return false;
    };

    if(dir.trimmed().isEmpty())
        return fail(QStringLiteral("TLS: no directory for self-signed cert"));

    if(!QDir().mkpath(dir))
        return fail(QStringLiteral("TLS: cannot create directory %1").arg(dir));

    const QString cert=QDir(dir).filePath(QStringLiteral("server-cert.pem"));
    const QString key=QDir(dir).filePath(QStringLiteral("server-key.pem"));

    if(certPath!=nullptr)
        *certPath=cert;

    if(keyPath!=nullptr)
        *keyPath=key;

    if(!force && QFile::exists(cert) && QFile::exists(key))
    {
        if(error!=nullptr)
            error->clear();

        return true;
    }

    const QString openssl=
        QStandardPaths::findExecutable(QStringLiteral("openssl"));

    if(openssl.isEmpty())
        return fail(QStringLiteral(
            "TLS: openssl not found. Install openssl or provide your own "
            "certificate/key (e.g. Let's Encrypt fullchain.pem/privkey.pem)."));

    const QString primary=primaryAddress();
    const QString localHost=QHostInfo::localHostName();

    QHostAddress parsed;
    const bool hostIsIp=parsed.setAddress(host);
    const QString cn=host.trimmed().isEmpty()
        ? (primary.isEmpty() ? localHost : primary) : host.trimmed();

    // SAN: CN, localhost, первичный IPv4 и машина. Клиент может
    // подключиться по IP или по имени — сертификат должен подходить
    QString san;
    int dnsIndex=0;
    int ipIndex=0;

    const auto addDns=[&](const QString &name) {
        if(name.trimmed().isEmpty())
            return;
        san+=QStringLiteral("DNS.%1 = %2\n").arg(++dnsIndex).arg(name);
    };
    const auto addIp=[&](const QString &ip) {
        QHostAddress a;
        if(ip.trimmed().isEmpty() || !a.setAddress(ip))
            return;
        san+=QStringLiteral("IP.%1 = %2\n").arg(++ipIndex).arg(ip);
    };

    if(hostIsIp)
        addIp(cn);
    else
        addDns(cn);

    addDns(QStringLiteral("localhost"));
    addDns(localHost);
    addIp(QStringLiteral("127.0.0.1"));
    addIp(primary);

    QTemporaryFile conf(QDir(dir).filePath(QStringLiteral("openssl-XXXXXX.cnf")));
    conf.setAutoRemove(true);

    if(!conf.open())
        return fail(QStringLiteral("TLS: cannot create openssl config"));

    const QByteArray cfg=
        "[req]\n"
        "prompt = no\n"
        "distinguished_name = dn\n"
        "x509_extensions = v3\n"
        "[dn]\n"
        "CN = "+cn.toUtf8()+"\n"
        "[v3]\n"
        "basicConstraints = CA:FALSE\n"
        "keyUsage = digitalSignature, keyEncipherment\n"
        "extendedKeyUsage = serverAuth\n"
        "subjectAltName = @alt\n"
        "[alt]\n"+san.toUtf8();

    conf.write(cfg);
    conf.flush();

    QProcess process;
    process.setProgram(openssl);
    process.setArguments({
        QStringLiteral("req"), QStringLiteral("-x509"),
        QStringLiteral("-newkey"), QStringLiteral("rsa:2048"),
        QStringLiteral("-nodes"), QStringLiteral("-sha256"),
        QStringLiteral("-days"), QStringLiteral("3650"),
        QStringLiteral("-keyout"), key,
        QStringLiteral("-out"), cert,
        QStringLiteral("-config"), conf.fileName(),
    });

    process.start();

    if(!process.waitForFinished(60000))
    {
        process.kill();
        process.waitForFinished(2000);
        return fail(QStringLiteral("TLS: openssl timed out"));
    }

    if(process.exitStatus()!=QProcess::NormalExit || process.exitCode()!=0)
        return fail(QStringLiteral("TLS: openssl failed: %1")
                        .arg(QString::fromLocal8Bit(process.readAllStandardError())
                                 .trimmed()));

    if(!QFile::exists(cert) || !QFile::exists(key))
        return fail(QStringLiteral("TLS: openssl did not create cert/key"));

    QFile::setPermissions(key, QFileDevice::ReadOwner
                               | QFileDevice::WriteOwner);

    if(error!=nullptr)
        error->clear();

    return true;
}


QString formatFingerprint(const QString &hex)
{
    QString out;

    for(int i=0; i<hex.size(); ++i)
    {
        if(i>0 && i%4==0)
            out+=QLatin1Char(' ');

        out+=hex.at(i);
    }

    return out.toUpper();
}

} // namespace SyncTls

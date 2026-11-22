#ifndef SYNCTLS_H
#define SYNCTLS_H

#include <QByteArray>
#include <QString>

#include <QSslCertificate>
#include <QSslConfiguration>

// TLS-помощники для sync-сервера/клиента.
//
// Сервер: PEM-сертификат (цепочка) + приватный ключ. Для публичного
// хоста — Let's Encrypt fullchain.pem/privkey.pem, для LAN можно
// сгенерировать self-signed через openssl CLI (Qt 5 не умеет создавать
// X.509 сам, а тащить ещё одну криптобиблиотеку ради этого не хочется).
namespace SyncTls
{

// Загрузить серверный сертификат/ключ. fingerprint — SHA256(cert DER)
// в нижнем регистре. false + error при любой проблеме.
bool loadServerConfig(const QString &certPath, const QString &keyPath,
                      QSslConfiguration &config, QString *fingerprint,
                      QString *error);

// SHA256(cert.toDer()) hex, нижний регистр
QString certificateFingerprint(const QSslCertificate &cert);
QString certificateFingerprint(const QString &certPath);

// Сгенерировать self-signed cert/key в dir (cert.pem/key.pem).
// force=false — если файлы уже есть, просто вернуть их.
// host — CN/SAN: домен или IP; в SAN также попадут localhost и
// первичный IPv4 машины.
bool ensureSelfSigned(const QString &dir, const QString &host,
                      QString *certPath, QString *keyPath,
                      QString *error, bool force=false);

// Первичный не-loopback IPv4 (для QR/self-signed SAN). Пусто — нет.
QString primaryAddress(void);

// Человекочитаемая группами по 4: ABCD EFGH ...
QString formatFingerprint(const QString &hex);

} // namespace SyncTls

#endif // SYNCTLS_H

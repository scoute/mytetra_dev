#ifndef SYNCPAIRQR_H
#define SYNCPAIRQR_H

#include <QPixmap>
#include <QString>

// QR-транспорт для одноразового кода спаривания.
//
// QR не заменяет криптографию: он лишь передаёт клиенту код и адрес
// сервера (host/port/TLS/fingerprint), чтобы не набирать 16 символов
// руками. Код по-прежнему одноразовый и живёт 5 минут.
namespace SyncPairQr
{

// mytetra://pair?v=1&host=...&port=...&code=...&tls=1&fp=...&name=...
QString buildUri(const QString &host, quint16 port, const QString &code,
                 bool tls, const QString &fingerprint=QString(),
                 const QString &name=QString());

// Чёрно-белый QR с белым quiet zone. Пусто — текст слишком длинный/ошибка.
QPixmap render(const QString &text, int pixelSize=260);

} // namespace SyncPairQr

#endif // SYNCPAIRQR_H

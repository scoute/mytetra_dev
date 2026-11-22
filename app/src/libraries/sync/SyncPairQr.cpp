#include <QImage>
#include <QPainter>
#include <QUrl>
#include <QUrlQuery>

#include "qrcodegen.hpp"
#include "SyncPairQr.h"


namespace SyncPairQr
{

QString buildUri(const QString &host, quint16 port, const QString &code,
                 bool tls, const QString &fingerprint,
                 const QString &name)
{
    QUrl url;
    url.setScheme(QStringLiteral("mytetra"));
    url.setHost(QStringLiteral("pair"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("v"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("host"), host);
    query.addQueryItem(QStringLiteral("port"), QString::number(port));
    query.addQueryItem(QStringLiteral("code"), code);
    query.addQueryItem(QStringLiteral("tls"), tls ? QStringLiteral("1")
                                                  : QStringLiteral("0"));

    if(tls && !fingerprint.isEmpty())
        query.addQueryItem(QStringLiteral("fp"), fingerprint);

    if(!name.isEmpty())
        query.addQueryItem(QStringLiteral("name"), name);

    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}


QPixmap render(const QString &text, int pixelSize)
{
    if(text.isEmpty() || pixelSize<=0)
        return QPixmap();

    try
    {
        const qrcodegen::QrCode qr=qrcodegen::QrCode::encodeText(
            text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);

        const int quiet=4;
        const int size=qr.getSize()+quiet*2;

        QImage image(size, size, QImage::Format_RGB32);
        image.fill(Qt::white);

        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);

        for(int y=0; y<qr.getSize(); ++y)
            for(int x=0; x<qr.getSize(); ++x)
                if(qr.getModule(x, y))
                    painter.drawRect(x+quiet, y+quiet, 1, 1);

        painter.end();

        return QPixmap::fromImage(image.scaled(
            pixelSize, pixelSize, Qt::KeepAspectRatio,
            Qt::FastTransformation));
    }
    catch(...)
    {
        return QPixmap();
    }
}

} // namespace SyncPairQr

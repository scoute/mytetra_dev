#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>

#include "SyncDiscovery.h"

namespace
{
const char *kProto="sync/1";
const char *kTypeProbe="discover";
const char *kTypeReply="discover-reply";
}


QByteArray SyncDiscovery::buildProbe(const QString &deviceId)
{
    QJsonObject obj;
    obj[QStringLiteral("proto")]=QString::fromLatin1(kProto);
    obj[QStringLiteral("type")]=QString::fromLatin1(kTypeProbe);
    obj[QStringLiteral("deviceId")]=deviceId;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}


QJsonObject SyncDiscovery::parseProbe(const QByteArray &datagram)
{
    QJsonParseError error;
    const QJsonDocument doc=QJsonDocument::fromJson(datagram, &error);

    if(error.error!=QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();

    const QJsonObject obj=doc.object();

    if(obj.value(QStringLiteral("proto")).toString()!=QString::fromLatin1(kProto)
       || obj.value(QStringLiteral("type")).toString()!=QString::fromLatin1(kTypeProbe))
        return QJsonObject();

    return obj;
}


QByteArray SyncDiscovery::buildReply(const QString &deviceId,
                                     const QString &deviceName,
                                     quint16 tcpPort)
{
    QJsonObject obj;
    obj[QStringLiteral("proto")]=QString::fromLatin1(kProto);
    obj[QStringLiteral("type")]=QString::fromLatin1(kTypeReply);
    obj[QStringLiteral("deviceId")]=deviceId;
    obj[QStringLiteral("deviceName")]=deviceName;
    obj[QStringLiteral("port")]=tcpPort;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}


SyncDiscoveredDevice SyncDiscovery::parseReply(const QByteArray &datagram,
                                               const QHostAddress &from)
{
    QJsonParseError error;
    const QJsonDocument doc=QJsonDocument::fromJson(datagram, &error);

    if(error.error!=QJsonParseError::NoError || !doc.isObject())
        return SyncDiscoveredDevice();

    const QJsonObject obj=doc.object();

    if(obj.value(QStringLiteral("proto")).toString()!=QString::fromLatin1(kProto)
       || obj.value(QStringLiteral("type")).toString()!=QString::fromLatin1(kTypeReply))
        return SyncDiscoveredDevice();

    SyncDiscoveredDevice device;
    device.deviceId=obj.value(QStringLiteral("deviceId")).toString();
    device.deviceName=obj.value(QStringLiteral("deviceName")).toString();
    device.port=static_cast<quint16>(obj.value(QStringLiteral("port")).toInt());
    device.address=from.toString();

    if(device.deviceId.isEmpty() || device.port==0)
        return SyncDiscoveredDevice();

    return device;
}


QList<SyncDiscoveredDevice> SyncDiscovery::scan(const QString &selfDeviceId,
                                                quint16 port,
                                                int timeoutMs)
{
    QList<SyncDiscoveredDevice> found;

    QUdpSocket socket;

    if(!socket.bind(QHostAddress::AnyIPv4, 0,
                    QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        return found;

    const QByteArray probe=buildProbe(selfDeviceId);

    // Broadcast — основной путь; loopback — стенд "два экземпляра
    // на одной машине" (broadcast до loopback не доходит)
    socket.writeDatagram(probe, QHostAddress::Broadcast, port);
    socket.writeDatagram(probe, QHostAddress::LocalHost, port);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QUdpSocket::readyRead, &loop, [&]() {
        while(socket.hasPendingDatagrams())
        {
            const QNetworkDatagram datagram=socket.receiveDatagram();
            const SyncDiscoveredDevice device=
                parseReply(datagram.data(), datagram.senderAddress());

            if(device.deviceId.isEmpty() || device.deviceId==selfDeviceId)
                continue;

            // дедуп по deviceId: побеждает последний ответ
            for(int i=found.size()-1; i>=0; --i)
                if(found.at(i).deviceId==device.deviceId)
                    found.removeAt(i);

            found.append(device);
        }
    });

    timer.start(timeoutMs);
    loop.exec();

    return found;
}

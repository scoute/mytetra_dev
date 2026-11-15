#ifndef SYNCDISCOVERY_H
#define SYNCDISCOVERY_H

#include <QByteArray>
#include <QHostAddress>
#include <QJsonObject>
#include <QList>
#include <QString>

// Устройство, найденное сканом сети
struct SyncDiscoveredDevice
{
    QString deviceId;
    QString deviceName;
    QString address;  // откуда пришел ответ (адрес хаба в LAN)
    quint16 port=0;   // TCP-порт sync-сервера хаба
};

// LAN-discovery для синка (UDP, порт = TCP-порт синка).
// Зачем: адреса устройств меняются DHCP, а хранить "last-ip" — только
// подсказка. Скан широковещательным пробом находит хабы заново.
//
// Формат пакетов (JSON, UTF-8):
//   проб:   {"proto":"sync/1","type":"discover","deviceId":"<кто спрашивает>"}
//   ответ:  {"proto":"sync/1","type":"discover-reply","deviceId","deviceName","port"}
// Приватность: ответ открывает deviceId/имя всему LAN — ровно как
// неаутентифицированный /hello, для MVP принято.
class SyncDiscovery
{
public:

    // Проб и ответ (для серверной стороны)
    static QByteArray buildProbe(const QString &deviceId);
    static QJsonObject parseProbe(const QByteArray &datagram);
    static QByteArray buildReply(const QString &deviceId,
                                 const QString &deviceName,
                                 quint16 tcpPort);

    // Разбор ответа; пустой объект — не наш/битый пакет
    static SyncDiscoveredDevice parseReply(const QByteArray &datagram,
                                           const QHostAddress &from);

    // Скан: проба в broadcast и loopback (стенд "два экземпляра на одной
    // машине"), сбор ответов timeoutMs. Свои ответы игнорируются
    static QList<SyncDiscoveredDevice> scan(const QString &selfDeviceId,
                                            quint16 port,
                                            int timeoutMs=2000);
};

#endif // SYNCDISCOVERY_H

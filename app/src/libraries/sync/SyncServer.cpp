#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QElapsedTimer>
#include <QDebug>

#include "SyncServer.h"
#include "SyncDiscovery.h"
#include "SyncDebug.h"
#include "main.h"


namespace
{
// Адрес пира в удобном виде: IPv6-mapped IPv4 (::ffff:192.168.1.2)
// нормализуем к dotted-quad, иначе в списке устройств мусор
QString normalizedPeerAddress(QTcpSocket *socket)
{
    if(socket==nullptr)
        return QString();

    const QHostAddress addr=socket->peerAddress();
    bool ok=false;
    const quint32 v4=addr.toIPv4Address(&ok);

    if(ok)
        return QHostAddress(v4).toString();

    return addr.toString();
}
}


SyncServer::SyncServer(QObject *parent) : QObject(parent)
{    SYNC_TRACE();

    server=new QTcpServer(this);

    connect(server, &QTcpServer::newConnection,
            this,   &SyncServer::onNewConnection);
}


SyncServer::~SyncServer(void)
{    SYNC_TRACE();

    stop();
}


bool SyncServer::start(quint16 port)
{    SYNC_TRACE();

    if(server->isListening())
        return true;

    if(!server->listen(QHostAddress::Any, port))
        return false;

    listenPort=server->serverPort();

    // LAN-discovery: UDP на том же порту. Если порт UDP занят —
    // работаем без скана, TCP-синк важнее
    if(udpSocket==nullptr)
    {
        udpSocket=new QUdpSocket(this);
        connect(udpSocket, &QUdpSocket::readyRead,
                this, &SyncServer::onDiscoveryDatagram);
    }

    if(!udpSocket->bind(QHostAddress::AnyIPv4, listenPort,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        qWarning() << "SyncServer: UDP discovery bind failed on port"
                   << listenPort << "- scan will not find this hub";
        delete udpSocket;
        udpSocket=nullptr;
    }

    // Встроенный маршрут живости: версия приложения и протокола sync/1.
    // Клиент по нему же проверяет что говорит с MyTetra, а не с чем-то еще
    addRoute(QStringLiteral("GET /ping"),
             [](const QJsonObject &) -> QJsonObject {
        QJsonObject answer;
        answer[QStringLiteral("service")]=QStringLiteral("mytetrasync");
        answer[QStringLiteral("proto")]=QStringLiteral("sync/1");
        // Версия из main.h, не константа: раньше была захардкожена "1.44"
        // и врала клиенту после каждого бампа версии
        answer[QStringLiteral("app")]=QStringLiteral("%1.%2")
            .arg(APPLICATION_RELEASE_VERSION)
            .arg(APPLICATION_RELEASE_SUBVERSION);
        return answer;
    });

    return true;
}


void SyncServer::stop(void)
{    SYNC_TRACE();

    if(server->isListening())
        server->close();

    if(udpSocket!=nullptr)
        udpSocket->close();

    listenPort=0;
}


void SyncServer::setDiscoveryIdentity(DiscoveryIdentity identity)
{
    discoveryIdentity=identity;
}


void SyncServer::onDiscoveryDatagram(void)
{    SYNC_TRACE();

    if(udpSocket==nullptr)
        return;

    while(udpSocket->hasPendingDatagrams())
    {
        const QNetworkDatagram datagram=udpSocket->receiveDatagram();
        const QJsonObject probe=SyncDiscovery::parseProbe(datagram.data());

        if(probe.isEmpty() || !discoveryIdentity)
            continue;

        const QJsonObject identity=discoveryIdentity();
        const QString selfId=identity.value(QStringLiteral("deviceId")).toString();
        const QString theirId=probe.value(QStringLiteral("deviceId")).toString();

        // на свой же проб не отвечаем (стенд "два экземпляра на одной
        // машине" шлет проб и в loopback)
        if(theirId.isEmpty() || theirId==selfId)
            continue;

        const QByteArray reply=SyncDiscovery::buildReply(
            selfId,
            identity.value(QStringLiteral("deviceName")).toString(),
            listenPort);
        udpSocket->writeDatagram(reply, datagram.senderAddress(),
                                 datagram.senderPort());

        syncDbg("server: discovery probe from "+theirId
                +" answered, port "+QString::number(listenPort));
    }
}


bool SyncServer::isRunning(void) const
{
    return server->isListening();
}


quint16 SyncServer::port(void) const
{
    return listenPort;
}


void SyncServer::addRoute(const QString &methodPath, Handler handler)
{
    routes.insert(methodPath, handler);
}


QString SyncServer::lastPeerAddress(void) const
{
    return currentPeerIp;
}


void SyncServer::onNewConnection(void)
{    SYNC_TRACE();

    while(server->hasPendingConnections())
    {
        QTcpSocket *socket=server->nextPendingConnection();

        connect(socket, &QTcpSocket::readyRead,
                this,   &SyncServer::onReadyRead);

        connect(socket, &QTcpSocket::disconnected,
                this, [this, socket]() {
            pendingRequests.remove(socket);
            socket->deleteLater();
        });
    }
}


void SyncServer::onReadyRead(void)
{    SYNC_TRACE();

    QTcpSocket *socket=qobject_cast<QTcpSocket *>(sender());

    if(socket==nullptr)
        return;

    QString methodPath;
    QJsonObject body;
    bool broken=false;

    // Данных пока мало — выходим и ждем следующего readyRead
    if(!tryParseRequest(socket, methodPath, body, broken))
    {
        if(broken)
        {
            QJsonObject error;
            error[QStringLiteral("error")]=QStringLiteral("bad request");

            syncDbg("server: bad request");

            sendJson(socket, 400, error);
            socket->disconnectFromHost();
        }

        return;
    }

    if(!routes.contains(methodPath))
    {
        QJsonObject error;
        error[QStringLiteral("error")]=QStringLiteral("unknown route");

        syncDbg("server: unknown route "+methodPath);

        sendJson(socket, 404, error);
        socket->disconnectFromHost();
        return;
    }

    QElapsedTimer timer;
    timer.start();

    // Запоминаем, откуда прилетел запрос: hello-маршрут запишет IP пира.
    // Сокет жив весь вызов обработчика ниже (удаляется только по disconnect)
    currentPeerIp=normalizedPeerAddress(socket);

    const QJsonObject answer=routes.value(methodPath)(body);
    const qint64 elapsed=timer.elapsed();

    {
        const QString err=answer.value(QStringLiteral("error")).toString();
        const QString warn=answer.value(QStringLiteral("warning")).toString();

        syncDbg("server: "+methodPath+" "
                +QString::number(elapsed)+"ms "
                +(err.isEmpty() ? QStringLiteral("ok")
                                : QStringLiteral("error=")+err)
                +(warn.isEmpty() ? QString()
                                 : QStringLiteral(" warning=")+warn));
    }

    sendJson(socket, 200, answer);
    socket->disconnectFromHost();
}


bool SyncServer::tryParseRequest(QTcpSocket *socket, QString &methodPath,
                                QJsonObject &body, bool &broken)
{    SYNC_TRACE();

    broken=false;

    // Добираем приехавшее к остатку прошлого пакета: заголовки
    // и тело могут прийти разными TCP-сегментами
    pendingRequests[socket]+=socket->readAll();
    const QByteArray raw=pendingRequests.value(socket);

    if(raw.isEmpty())
        return false;

    // Стартовая строка: "METHOD /path HTTP/1.x"
    const int lineEnd=raw.indexOf("\r\n");

    if(lineEnd<0)
        return false;

    const QList<QByteArray> parts=QByteArray(raw.constData(), lineEnd).split(' ');

    if(parts.size()<2)
    {
        broken=true;
        return false;
    }

    const QString method=QString::fromLatin1(parts.at(0)).toUpper();
    const QString path=QString::fromLatin1(parts.at(1));

    methodPath=method+QStringLiteral(" ")+path;

    // Длина тела из заголовков. Без тела — запрос полный сразу
    int contentLength=0;
    const int headEnd=raw.indexOf("\r\n\r\n");

    if(headEnd>=0)
    {
        const QString head=QString::fromLatin1(raw.constData(), headEnd);

        for(const QString &line : head.split(QStringLiteral("\r\n")))
        {
            if(line.startsWith(QStringLiteral("Content-Length:"),
                               Qt::CaseInsensitive))
            {
                contentLength=line.section(QLatin1Char(':'), 1).trimmed().toInt();
                break;
            }
        }
    }

    bool expectContinue=false;

    if(headEnd>=0)
    {
        const QString headLower=QString::fromLatin1(raw.constData(), headEnd).toLower();

        if(headLower.contains(QStringLiteral("expect: 100-continue")))
            expectContinue=true;
    }

    const int bodyStart=(headEnd>=0) ? headEnd+4 : raw.size();
    const int bodyHave=raw.size()-bodyStart;

    if(bodyHave<contentLength)
    {
        // Клиент (QNetworkAccessManager) ждет промежуточного подтверждения,
        // иначе тело не отправит вообще. Отвечаем сразу, тело ждем дальше
        if(expectContinue && bodyHave==0)
        {
            static const QByteArray cont="HTTP/1.1 100 Continue\r\n\r\n";
            socket->write(cont);
            socket->flush();
        }

        return false;
    }

    // Запрос целый — остаток из буфера убираем (соединение одноразовое,
    // но повторный разбор того же мусора нам не нужен)
    pendingRequests.remove(socket);

    if(contentLength<=0)
    {
        body=QJsonObject();
        return true;
    }

    // Тело обязано быть JSON. Не JSON — битый запрос, не молчим
    QJsonParseError parseError;
    const QJsonDocument doc=QJsonDocument::fromJson(
        QByteArray(raw.constData()+bodyStart, contentLength), &parseError);

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
    {
        pendingRequests.remove(socket);
        broken=true;
        return false;
    }

    pendingRequests.remove(socket);

    body=doc.object();
    return true;
}


void SyncServer::sendJson(QTcpSocket *socket, int code, const QJsonObject &obj)
{    SYNC_TRACE();

    const QByteArray payload=QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QByteArray out=statusLine(code);
    out+="Content-Type: application/json; charset=utf-8\r\n";
    out+="Content-Length: "+QByteArray::number(payload.size())+"\r\n";
    out+="Connection: close\r\n";
    out+="\r\n";
    out+=payload;

    socket->write(out);
    socket->flush();
}


QByteArray SyncServer::statusLine(int code)
{
    switch(code)
    {
    case 200: return QByteArrayLiteral("HTTP/1.1 200 OK\r\n");
    case 400: return QByteArrayLiteral("HTTP/1.1 400 Bad Request\r\n");
    case 404: return QByteArrayLiteral("HTTP/1.1 404 Not Found\r\n");
    default:  return QByteArrayLiteral("HTTP/1.1 500 Internal Error\r\n");
    }
}

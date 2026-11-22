#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QElapsedTimer>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QDebug>

#include "SyncServer.h"
#include "SyncDiscovery.h"
#include "SyncDebug.h"
#include "SyncHttp.h"
#include "SyncTls.h"
#include "main.h"


namespace
{
// Адрес пира в удобном виде: IPv6-mapped IPv4 (::ffff:192.168.1.2)
// нормализуем к dotted-quad, иначе в списке устройств мусор
// QTcpServer, который принимает соединения как QSslSocket. addPending-
// Connection оставляет прежний onNewConnection: для SyncServer разницы
// между plain и TLS-сокетом нет — readAll/write уже работают с данными
class TlsTcpServer : public QTcpServer
{
public:

    TlsTcpServer(const QSslConfiguration &config, QObject *parent)
        : QTcpServer(parent), config(config) {}

protected:

    void incomingConnection(qintptr socketDescriptor) override
    {
        QSslSocket *socket=new QSslSocket(this);

        if(!socket->setSocketDescriptor(socketDescriptor))
        {
            delete socket;
            return;
        }

        socket->setSslConfiguration(config);

        QObject::connect(socket,
                         QOverload<const QList<QSslError> &>::of(
                             &QSslSocket::sslErrors),
                         socket, [](const QList<QSslError> &errors) {
            for(const QSslError &error : errors)
                qWarning() << "SyncServer TLS:" << error.errorString();
        });

        addPendingConnection(socket);
        socket->startServerEncryption();
    }

private:

    QSslConfiguration config;
};


QString normalizedLocalAddress(QTcpSocket *socket)
{
    if(socket==nullptr)
        return QString();

    const QHostAddress addr=socket->localAddress();
    bool ok=false;
    const quint32 v4=addr.toIPv4Address(&ok);

    if(ok)
        return QHostAddress(v4).toString();

    return addr.toString();
}


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

    if(server!=nullptr && server->isListening())
        return true;

    // Сокет-класс зависит от TLS: пересоздаём слушатель при каждом
    // старте (старый уже закрыт/не слушает)
    if(server!=nullptr)
    {
        server->close();
        delete server;
        server=nullptr;
    }

    if(tls)
        server=new TlsTcpServer(sslConfig, this);
    else
        server=new QTcpServer(this);

    connect(server, &QTcpServer::newConnection,
            this,   &SyncServer::onNewConnection);

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
    {
        // Удаляем, а не только close(): start() биндит UDP лишь при
        // udpSocket==nullptr — после stop/start скан в LAN переставал
        // получать ответы (TCP при этом работал).
        udpSocket->close();
        delete udpSocket;
        udpSocket=nullptr;
    }

    listenPort=0;
}


bool SyncServer::setTls(const QString &certPath, const QString &keyPath,
                        QString *error)
{
    QSslConfiguration config;
    QString fingerprint;
    QString reason;

    if(!SyncTls::loadServerConfig(certPath, keyPath, config,
                                  &fingerprint, &reason))
    {
        if(error!=nullptr)
            *error=reason;

        return false;
    }

    sslConfig=config;
    tlsFingerprintText=fingerprint;
    discoveryTls=true;
    discoveryTlsFingerprint=fingerprint;
    tls=true;

    if(error!=nullptr)
        error->clear();

    qDebug() << "SyncServer: TLS enabled, cert fingerprint"
             << fingerprint.left(16)+"...";
    return true;
}


void SyncServer::clearTls(void)
{
    tls=false;
    sslConfig=QSslConfiguration();
    tlsFingerprintText.clear();
    discoveryTls=false;
    discoveryTlsFingerprint.clear();
}


bool SyncServer::tlsEnabled(void) const
{
    return tls;
}


QString SyncServer::tlsFingerprint(void) const
{
    return tlsFingerprintText;
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
            listenPort,
            discoveryTls,
            discoveryTlsFingerprint);
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


QString SyncServer::lastLocalAddress(void) const
{
    return currentLocalIp;
}


void SyncServer::onNewConnection(void)
{    SYNC_TRACE();

    while(server->hasPendingConnections())
    {
        QTcpSocket *socket=server->nextPendingConnection();

        if(QSslSocket *ssl=qobject_cast<QSslSocket *>(socket))
            connect(ssl, &QSslSocket::encrypted, this, [ssl]() {
                syncDbg("server: TLS established from "
                        +ssl->peerAddress().toString());
            });

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

    if(socket!=nullptr)
        processRequests(socket);
}


// Одно TCP-соединение обслуживает несколько последовательных запросов:
// без этого каждый pull-note открывал новый сокет (а с TLS — и новый
// handshake), и синк тысяч заметок упирался в round-trip, а не в диск.
void SyncServer::processRequests(QTcpSocket *socket)
{
    while(true)
    {
        QString methodPath;
        QJsonObject body;
        bool broken=false;
        int consumed=0;

        // Данных пока мало — выходим и ждем следующего readyRead
        if(!tryParseRequest(socket, methodPath, body, broken, &consumed))
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
        currentLocalIp=normalizedLocalAddress(socket);

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

        // В буфере уже может лежать следующий запрос (pipelining);
        // иначе ждём readyRead на этом же сокете
        if(pendingRequests.value(socket).isEmpty())
            return;
    }
}


bool SyncServer::tryParseRequest(QTcpSocket *socket, QString &methodPath,
                                QJsonObject &body, bool &broken,
                                int *consumed)
{    SYNC_TRACE();

    broken=false;

    if(consumed!=nullptr)
        *consumed=0;

    // Добираем приехавшее к остатку прошлого пакета: заголовки
    // и тело могут прийти разными TCP-сегментами
    pendingRequests[socket]+=socket->readAll();
    const QByteArray raw=pendingRequests.value(socket);

    if(raw.isEmpty())
        return false;

    const SyncHttp::ParseResult parsed=SyncHttp::parse(raw);

    if(parsed.status==SyncHttp::Status::NeedMore)
    {
        // Клиент ждёт 100-continue, иначе тело не отправит
        if(parsed.sendContinue)
        {
            static const QByteArray cont="HTTP/1.1 100 Continue\r\n\r\n";
            socket->write(cont);
            socket->flush();
        }

        return false;
    }

    if(parsed.status==SyncHttp::Status::Broken)
    {
        pendingRequests.remove(socket);
        broken=true;
        return false;
    }

    // Запрос целый. Соединение живёт дальше: в буфере может остаться
    // начало/целиком следующий запрос (HTTP/1.1 keep-alive)
    if(parsed.consumed>0 && parsed.consumed<raw.size())
        pendingRequests[socket]=raw.mid(parsed.consumed);
    else
        pendingRequests.remove(socket);

    if(consumed!=nullptr)
        *consumed=parsed.consumed;

    methodPath=parsed.methodPath;
    body=parsed.body;
    return true;
}


void SyncServer::sendJson(QTcpSocket *socket, int code, const QJsonObject &obj)
{    SYNC_TRACE();

    const QByteArray payload=QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QByteArray out=statusLine(code);
    out+="Content-Type: application/json; charset=utf-8\r\n";
    out+="Content-Length: "+QByteArray::number(payload.size())+"\r\n";
    out+="Connection: keep-alive\r\n";
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

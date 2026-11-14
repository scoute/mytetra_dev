#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QDebug>

#include "SyncServer.h"
#include "SyncDebug.h"
#include "main.h"


SyncServer::SyncServer(QObject *parent) : QObject(parent)
{
    server=new QTcpServer(this);

    connect(server, &QTcpServer::newConnection,
            this,   &SyncServer::onNewConnection);
}


SyncServer::~SyncServer(void)
{
    stop();
}


bool SyncServer::start(quint16 port)
{
    if(server->isListening())
        return true;

    if(!server->listen(QHostAddress::Any, port))
        return false;

    listenPort=server->serverPort();

    // Встроенный маршрут живости: версия приложения и протокола sync/1.
    // Клиент по нему же проверяет что говорит с MyTetra, а не с чем-то еще
    addRoute(QStringLiteral("GET /ping"),
             [](const QJsonObject &) -> QJsonObject {
        QJsonObject answer;
        answer[QStringLiteral("service")]=QStringLiteral("mytetrasync");
        answer[QStringLiteral("proto")]=QStringLiteral("sync/1");
        answer[QStringLiteral("app")]=QStringLiteral("1.44");
        return answer;
    });

    return true;
}


void SyncServer::stop(void)
{
    if(server->isListening())
        server->close();

    listenPort=0;
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


void SyncServer::onNewConnection(void)
{
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
{
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

            if(syncDebugEnabled())
                qDebug() << "SyncServer: bad request";

            sendJson(socket, 400, error);
            socket->disconnectFromHost();
        }

        return;
    }

    if(!routes.contains(methodPath))
    {
        QJsonObject error;
        error[QStringLiteral("error")]=QStringLiteral("unknown route");

        if(syncDebugEnabled())
            qDebug() << "SyncServer: unknown route" << methodPath;

        sendJson(socket, 404, error);
        socket->disconnectFromHost();
        return;
    }

    QElapsedTimer timer;
    timer.start();
    const QJsonObject answer=routes.value(methodPath)(body);
    const qint64 elapsed=timer.elapsed();

    if(syncDebugEnabled())
    {
        const QString err=answer.value(QStringLiteral("error")).toString();
        const QString warn=answer.value(QStringLiteral("warning")).toString();

        qDebug() << "SyncServer:" << methodPath << elapsed << "ms"
                 << (err.isEmpty() ? QStringLiteral("ok")
                                   : QStringLiteral("error=")+err)
                 << (warn.isEmpty() ? QString()
                                    : QStringLiteral("warning=")+warn);
    }

    sendJson(socket, 200, answer);
    socket->disconnectFromHost();
}


bool SyncServer::tryParseRequest(QTcpSocket *socket, QString &methodPath,
                                QJsonObject &body, bool &broken)
{
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
{
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

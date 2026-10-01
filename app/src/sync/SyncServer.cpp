#include "SyncServer.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QSettings>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/GlobalParameters.h"
#include "models/tree/KnowTreeModel.h"
#include "views/tree/KnowTreeView.h"

extern GlobalParameters globalParameters;


// Имя файла настроек сервера в рабочей директории
static QString syncConfigFileName(void)
{
    extern GlobalParameters globalParameters;
    return globalParameters.getWorkDirectory()+"/sync.ini";
}


SyncServer::SyncServer(QObject *parent) : QTcpServer(parent)
{
    m_port=0;

    connect(this, &QTcpServer::newConnection, this, &SyncServer::onNewConnection);
}


SyncServer::~SyncServer(void)
{
}


SyncCore *SyncServer::syncCore(void)
{
    return &m_core;
}


bool SyncServer::start(void)
{
    QSettings config(syncConfigFileName(), QSettings::IniFormat);

    bool enabled=config.value("Sync/enabled", false).toBool();
    if(!enabled)
    {
        qDebug() << "SyncServer: disabled in sync.ini, server not started";
        return false;
    }

    m_bind=config.value("Sync/bind", "127.0.0.1").toString();
    m_port=config.value("Sync/port", 8472).toInt();

    m_token=config.value("Sync/token", "").toString();
    if(m_token.isEmpty())
    {
        // Токен генерируется один раз и хранится в sync.ini.
        // Показывается в логе, дальше клиент хранит его сам
        m_token=getUniqueId()+getUniqueId();
        config.setValue("Sync/token", m_token);
        config.sync();
    }

    // Модель резолвится здесь: сервер стартует после главного окна,
    // ядро дальше работает с явным указателем без поиска окон
    KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");
    if(treeView==nullptr)
    {
        qWarning() << "SyncServer: no tree view, server not started";
        return false;
    }

    m_core.setModel(static_cast<KnowTreeModel *>(treeView->model()));
    m_core.store().load();

    if(!listen(QHostAddress(m_bind), static_cast<quint16>(m_port)))
    {
        qWarning() << "SyncServer: can not listen" << m_bind << m_port << errorString();
        return false;
    }

    qDebug() << "SyncServer: listening on" << m_bind << m_port << "proto" << SyncCore::protoVersion();

    return true;
}


int SyncServer::serverPort(void) const
{
    if(isListening())
        return serverPort();
    else
        return 0;
}


void SyncServer::onNewConnection(void)
{
    while(hasPendingConnections())
    {
        QTcpSocket *socket=nextPendingConnection();

        // Зависшее недописанное соединение закрывается через 15 секунд
        QTimer::singleShot(15000, socket, [socket]()
        {
            socket->close();
        });

        connect(socket, &QTcpSocket::readyRead, this, &SyncServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}


void SyncServer::onClientReadyRead(void)
{
    QTcpSocket *socket=qobject_cast<QTcpSocket *>(sender());
    if(socket==nullptr)
        return;

    // Накопленные байты хранятся в свойстве сокета
    QByteArray data=socket->property("requestData").toByteArray();
    data+=socket->readAll();
    socket->setProperty("requestData", data);

    HttpRequest request=parseRequest(data);
    if(!request.complete)
        return; // Ждем остаток заголовков или тела

    socket->setProperty("requestData", QByteArray());

    // Маршрутизация, ядро вызывается по имени операции
    QStringList parts=request.path.split("/", Qt::SkipEmptyParts);

    if(request.method=="GET" && request.path=="/status")
    {
        QJsonObject status;
        status["proto"]=SyncCore::protoVersion();
        status["rev"]=static_cast<int>(m_core.store().rev());

        sendJson(socket, 200, QJsonDocument(status).toJson(QJsonDocument::Compact));
    }
    else if(!checkAuth(request))
    {
        sendJson(socket, 401, "{\"error\":\"unauthorized\"}");
    }
    else if(request.method=="GET" && request.path=="/snapshot")
    {
        sendJson(socket, 200, m_core.snapshot());
    }
    else if(request.method=="GET" && request.path=="/pull")
    {
        sendJson(socket, 200, m_core.pull());
    }
    else if(request.method=="GET" && parts.count()==2 && parts.at(0)=="record")
    {
        QByteArray body=m_core.record(parts.at(1));
        if(body.isEmpty())
            sendJson(socket, 404, "{\"error\":\"record not found\"}");
        else
            sendJson(socket, 200, body);
    }
    else if(request.method=="GET" && parts.count()==3 && parts.at(0)=="blob")
    {
        int code=200;
        QByteArray body=m_core.blob(parts.at(1), parts.at(2), code);
        if(code==200)
            sendBytes(socket, 200, body);
        else
            sendJson(socket, code, "{\"error\":\"blob not found\"}");
    }
    else if(request.method=="POST" && request.path=="/push")
    {
        sendJson(socket, 200, m_core.push(request.body));
    }
    else
    {
        sendJson(socket, 404, "{\"error\":\"unknown route\"}");
    }
}


SyncServer::HttpRequest SyncServer::parseRequest(const QByteArray &data)
{
    HttpRequest request;
    request.complete=false;

    int headerEnd=data.indexOf("\r\n\r\n");
    if(headerEnd==-1)
        return request; // Заголовки еще не целые

    QList<QByteArray> lines=data.left(headerEnd).split('\n');

    if(lines.isEmpty())
        return request;

    // Стартовая строка: METHOD /path?query HTTP/1.1
    QList<QByteArray> startLine=lines.takeFirst().trimmed().split(' ');
    if(startLine.count()<2)
        return request;

    request.method=QString::fromLatin1(startLine.at(0));

    QString fullPath=QString::fromLatin1(startLine.at(1));
    int queryPos=fullPath.indexOf("?");
    if(queryPos==-1)
        request.path=fullPath;
    else
    {
        request.path=fullPath.left(queryPos);

        QStringList pairs=fullPath.mid(queryPos+1).split("&", Qt::SkipEmptyParts);
        foreach(QString pair, pairs)
        {
            int equalPos=pair.indexOf("=");
            if(equalPos==-1)
                request.query[pair]=QString();
            else
                request.query[pair.left(equalPos)]=QUrl::fromPercentEncoding(pair.mid(equalPos+1).toLatin1());
        }
    }

    // Заголовки, имена в нижнем регистре
    foreach(QByteArray line, lines)
    {
        int colonPos=line.indexOf(":");
        if(colonPos==-1)
            continue;

        QString name=QString::fromLatin1(line.left(colonPos).trimmed()).toLower();
        QString value=QString::fromLatin1(line.mid(colonPos+1).trimmed());
        request.headers[name]=value;
    }

    // Тело по Content-Length
    int contentLength=request.headers.value("content-length", "0").toInt();
    QByteArray body=data.mid(headerEnd+4);

    if(body.size()<contentLength)
        return request; // Тело еще не целое

    request.body=body.left(contentLength);
    request.complete=true;

    return request;
}


void SyncServer::sendJson(QTcpSocket *socket, int code, const QByteArray &body)
{
    QByteArray response;
    response+="HTTP/1.1 "+QByteArray::number(code)+" OK\r\n";
    response+="Content-Type: application/json; charset=utf-8\r\n";
    response+="Content-Length: "+QByteArray::number(body.size())+"\r\n";
    response+="Connection: close\r\n";
    response+="\r\n";
    response+=body;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}


void SyncServer::sendBytes(QTcpSocket *socket, int code, const QByteArray &body)
{
    QByteArray response;
    response+="HTTP/1.1 "+QByteArray::number(code)+" OK\r\n";
    response+="Content-Type: application/octet-stream\r\n";
    response+="Content-Length: "+QByteArray::number(body.size())+"\r\n";
    response+="Connection: close\r\n";
    response+="\r\n";
    response+=body;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}


bool SyncServer::checkAuth(const HttpRequest &request) const
{
    QString auth=request.headers.value("authorization", QString());

    return auth=="Bearer "+m_token;
}

#ifndef SYNCSERVER_H
#define SYNCSERVER_H

#include <functional>

#include <QObject>
#include <QMap>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QSslConfiguration>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;

// Минимальный HTTP/1.1 сервер синхронизации (Qt 5.15 без QHttpServer).
// Одно соединение — один запрос — один ответ — закрытие. Тела JSON,
// UTF-8. Маршруты: точное совпадение "METHOD path" (например "GET /ping").
// Неизвестный маршрут — 404 JSON, битый запрос — 400. Ничего лишнего:
// ни keep-alive, ни chunked, ни статики файлов.
class SyncServer : public QObject
{
    Q_OBJECT

public:

    using Handler = std::function<QJsonObject(const QJsonObject &request)>;

    explicit SyncServer(QObject *parent=nullptr);
    ~SyncServer(void) override;

    // Слушать порт на всех интерфейсах. false — порт занят
    bool start(quint16 port);
    void stop(void);
    bool isRunning(void) const;
    quint16 port(void) const;

    // TLS: PEM-сертификат (цепочка) + приватный ключ. Если задан,
    // порт говорит только TLS. false + error — не загрузилось
    bool setTls(const QString &certPath, const QString &keyPath,
                QString *error=nullptr);
    void clearTls(void);
    bool tlsEnabled(void) const;
    QString tlsFingerprint(void) const;

    // Зарегистрировать обработчик маршрута "METHOD path"
    void addRoute(const QString &methodPath, Handler handler);

    // IP пира текущего обрабатываемого запроса (пусто вне обработчика).
    // Нужно hello-маршруту: записать адрес устройства в список пиров,
    // иначе на сервере вечно "(unknown address)" и обратный синк невозможен
    QString lastPeerAddress(void) const;

    // Идентичность для LAN-discovery (ответ на UDP-проб, см. SyncDiscovery).
    // Пусто — на пробы не отвечаем. Порт в ответ добавит сервер сам
    using DiscoveryIdentity = std::function<QJsonObject()>;
    void setDiscoveryIdentity(DiscoveryIdentity identity);

private slots:

    void onNewConnection(void);
    void onReadyRead(void);
    void onDiscoveryDatagram(void);

private:

    // Разобрать запрос из буфера сокета. Данные копятся между вызовами:
    // заголовки и тело могут приехать разными пакетами. Возвращает false,
    // если данных пока не хватает (ждем еще readyRead) или запрос битый
    // (отвечаем 400 и закрываем). Прочитанное не теряется
    bool tryParseRequest(QTcpSocket *socket, QString &methodPath,
                         QJsonObject &body, bool &broken);

    void sendJson(QTcpSocket *socket, int code, const QJsonObject &obj);
    static QByteArray statusLine(int code);

    QTcpServer *server=nullptr;
    quint16 listenPort=0;

    bool tls=false;
    QSslConfiguration sslConfig;
    QString tlsFingerprintText;
    bool discoveryTls=false;
    QString discoveryTlsFingerprint;
    QMap<QString, Handler> routes;

    // LAN-discovery: UDP на том же порту, ответы на пробы скана
    QUdpSocket *udpSocket=nullptr;
    DiscoveryIdentity discoveryIdentity;

    // Недочитанные запросы по сокетам: readAll в onReadyRead забирает все,
    // что приехало, остаток ждет следующего пакета здесь, а не теряется
    QMap<QTcpSocket *, QByteArray> pendingRequests;

    // Адрес пира текущего запроса (для hello: записать IP устройства).
    // Запросы идут строго последовательно в одном потоке, гонки нет.
    // IPv6-mapped IPv4 нормализуется к dotted-quad
    QString currentPeerIp;
};

#endif // SYNCSERVER_H

#ifndef SYNCSERVER_H
#define SYNCSERVER_H

#include <functional>

#include <QObject>
#include <QMap>
#include <QString>
#include <QByteArray>
#include <QJsonObject>

class QTcpServer;
class QTcpSocket;

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

    // Зарегистрировать обработчик маршрута "METHOD path"
    void addRoute(const QString &methodPath, Handler handler);

private slots:

    void onNewConnection(void);
    void onReadyRead(void);

private:

    // Разобрать запрос из буфера сокета. Возвращает false если данных
    // пока не хватает (ждем еще) или запрос битый (отвечаем 400 и закрываем)
    bool tryParseRequest(QTcpSocket *socket, QString &methodPath,
                         QJsonObject &body, bool &broken);

    void sendJson(QTcpSocket *socket, int code, const QJsonObject &obj);
    static QByteArray statusLine(int code);

    QTcpServer *server=nullptr;
    quint16 listenPort=0;
    QMap<QString, Handler> routes;
};

#endif // SYNCSERVER_H

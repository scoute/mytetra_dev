#ifndef _SYNCSERVER_H_
#define _SYNCSERVER_H_

#include <QTcpServer>
#include <QMap>
#include <QString>

#include "sync/SyncCore.h"

class QTcpSocket;

// Сервер синхронизации для MyTetroid. HTTP+JSON поверх QTcpServer,
// сторонних библиотек нет (правило Qt-only).
// Прототип для одного клиента в LAN: авторизация Bearer-токеном,
// ревизии и хеши в sidecar (SyncStore), формат mytetra.xml не меняется.
// Соединения закрываются после каждого ответа, keep-alive нет.

class SyncServer : public QTcpServer
{
    Q_OBJECT

public:

    explicit SyncServer(QObject *parent=nullptr);
    virtual ~SyncServer(void);

    // Чтение sync.ini из рабочей директории и запуск прослушивания,
    // если сервер включен. При пустом токене генерируется новый
    // и записывается в sync.ini. Возвращает true если сервер слушает
    bool start(void);

    // Порт прослушивания, 0 если сервер не запущен
    int serverPort(void) const;

    // Ядро синхронизации, общее для всех транспортов.
    // Готовится к будущему локальному MCP
    SyncCore *syncCore(void);

private slots:

    // Новое входящее соединение
    void onNewConnection(void);

    // Данные от клиента, запрос собирается до конца заголовков и тела
    void onClientReadyRead(void);

private:

    // Структура разобранного HTTP-запроса
    struct HttpRequest
    {
        QString method;
        QString path;
        QMap<QString, QString> query;
        QMap<QString, QString> headers;
        QByteArray body;
        bool complete;
    };

    // Разбор накопленных байтов, complete=true если запрос целый
    static HttpRequest parseRequest(const QByteArray &data);

    // Ответ клиенту с JSON телом и закрытие соединения
    static void sendJson(QTcpSocket *socket, int code, const QByteArray &body);

    // Ответ клиенту с бинарным телом и закрытие соединения
    static void sendBytes(QTcpSocket *socket, int code, const QByteArray &body);

    // Проверка Bearer-токена, /status токена не требует
    bool checkAuth(const HttpRequest &request) const;

    // Токен из sync.ini
    QString m_token;

    // Порт из sync.ini
    int m_port;

    // Адрес прослушивания из sync.ini
    QString m_bind;

    // Ядро синхронизации, общее для всех транспортов
    SyncCore m_core;
};

#endif // _SYNCSERVER_H_

#ifndef SYNCCLIENT_H
#define SYNCCLIENT_H

#include <QString>
#include <QJsonObject>

#include "SyncManifest.h"
#include "SyncHandshake.h"

class AppConfig;

// Клиент синхронизации (спок): блокирующие вызовы поверх QNetworkAccessManager
// с таймаутом. Вызывается из UI по кнопке, не из горячих путей — блокировка
// потока GUI на время сетевого обмена допустима в MVP (прогресс — позже).
class SyncClient
{
public:

    explicit SyncClient(AppConfig *config);

    void setTimeoutMs(int ms);

    // Рукопожатие. Пустой error — успех (см. SyncHelloResult).
    SyncHelloResult hello(const QString &host, quint16 port);

    // Забрать чужой манифест. Пустой (без веток/заметок) при ошибке сети —
    // вызывающий проверяет отдельно через lastError()
    SyncManifest fetchManifest(const QString &host, quint16 port);

    // Забрать заметку целиком: поля, блоб текста (как лежит), аттачи.
    // Пустой объект при ошибке, см. lastError()
    QJsonObject pullNote(const QString &host, quint16 port,
                         const QString &id);

    // Забрать ветку: поля + parentId
    QJsonObject pullBranch(const QString &host, quint16 port,
                           const QString &id);

    QString lastError(void) const;

private:

    QJsonObject postJson(const QString &host, quint16 port,
                         const QString &path, const QJsonObject &body,
                         bool &ok);
    QJsonObject getJson(const QString &host, quint16 port,
                        const QString &path, bool &ok);

    AppConfig *config=nullptr;
    int timeoutMs=15000;
    QString lastErrorText;
};

#endif // SYNCCLIENT_H

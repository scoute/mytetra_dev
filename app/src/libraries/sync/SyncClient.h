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
    // pairRequest: "хочу в пару" — сервер запишет входящий запрос
    // для Accept и ответит pairPending (шаг 1 accept-flow)
    SyncHelloResult hello(const QString &host, quint16 port,
                         bool pairRequest=false);

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

    // Отправить заметку целиком (создание/обновление). Блоб base64,
    // mtime/hash — из локального манифеста (хаб их adopt'ит, не "сейчас")
    bool pushNote(const QString &host, quint16 port,
                  const QJsonObject &note);

    // Отправить ветку целиком (создание/обновление полей)
    bool pushBranch(const QString &host, quint16 port,
                    const QJsonObject &branch);

    // Отправить удаление по tombstone
    bool pushDelete(const QString &host, quint16 port,
                    const QString &id, qint64 deletedAt);

    // Фиксация: сейв + перезагрузка моделей на хабе
    bool commit(const QString &host, quint16 port);

    // Полная замена содержимого хаба (стратегия "заменить серверные"):
    // хаб стирает базу в корзину без tombstone'ов
    bool reset(const QString &host, quint16 port);

    QString lastError(void) const;

    // Последний запрос дошел до сервера и вернулся разобранным
    // (true), либо умер в пути: таймаут/сеть/битый ответ (false).
    // Нужно сессии чтобы отличить фатальное (рвем сессию) от
    // поштучного (пишем в report.errors и идем дальше)
    bool lastTransportOk(void) const;

private:

    QJsonObject postJson(const QString &host, quint16 port,
                         const QString &path, const QJsonObject &body,
                         bool &ok);

    // Подписать тело proof'ом пары (маршрут — часть challenge).
    // Без секрета (до спаривания) тело уходит как есть — сервер ответит
    // not-paired/auth-failed, гейт пары выше по стеку
    QJsonObject withAuth(const QString &route,
                         const QJsonObject &body);
    QJsonObject getJson(const QString &host, quint16 port,
                        const QString &path, bool &ok);

    AppConfig *config=nullptr;
    int timeoutMs=15000;
    QString lastErrorText;
    bool lastTransportOkFlag=true;
};

#endif // SYNCCLIENT_H

#ifndef SYNCCLIENT_H
#define SYNCCLIENT_H

#include <functional>

#include <QString>
#include <QStringList>
#include <QJsonObject>

#include "SyncManifest.h"
#include "SyncHandshake.h"

class AppConfig;
class QSslCertificate;

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

    // Спаривание по одноразовому коду (sync/2). serverId — id сервера
    // из pre-hello; code — 16 base32-символа с экрана сервера.
    // Возвращает долговременный секрет (hex) или пусто; errorCode —
    // машинный код (pair-code-invalid, pair-code-expired, ...)
    QString pairWithCode(const QString &host, quint16 port,
                         const QString &serverId, const QString &code,
                         QString *errorCode=nullptr);

    // Забрать чужой манифест. Пустой (без веток/заметок) при ошибке сети —
    // вызывающий проверяет отдельно через lastError()
    SyncManifest fetchManifest(const QString &host, quint16 port,
                               const QString &scopeRootId=QString());
    // Эхо scope из ответа: пусто/не равно — старый сервер поле ignored
    QString lastManifestScope(void) const;

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

    // Слияние веток по именам на хабе (стратегия "объединение"):
    // ветки из branchIds вливаются в одноименные ветки хаба
    bool mergeByName(const QString &host, quint16 port,
                     const QStringList &branchIds);

    // Транспорт: HTTP / HTTPS. Off — как раньше; Prefer — HTTPS, при
    // явном отсутствии TLS откат на HTTP; Require — только HTTPS
    enum class TlsMode { Off=0, Prefer=1, Require=2 };
    void setTlsMode(TlsMode mode);
    TlsMode tlsMode(void) const;

    // Пин сертификата (SHA256 DER hex). Нужен для self-signed; для
    // CA-подписанного можно не задавать
    void setTlsPin(const QString &fingerprint);
    QString tlsPin(void) const;

    // Спросить пользователя, доверять ли сертификату (TOFU). Вызывается
    // из sslErrors; true — запомнить пин и продолжить. Без колбэка
    // self-signed сертификат отвергается
    using TlsTrustCallback =
        std::function<bool(const QString &host, quint16 port,
                           const QSslCertificate &cert,
                           const QString &fingerprint)>;
    void setTlsTrustCallback(TlsTrustCallback callback);

    bool lastUsedTls(void) const;
    QString lastTlsFingerprint(void) const;

    // Секрет именно под текущий пир (кольцо на 4-5 устройств).
    // hello() перебирает кольцо и запоминает рабочий; сессия может
    // перевыставить явно после adoptPairing (секрет сменился)
    void setPeerSecret(const QString &secret);
    QString peerSecret(void) const;

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

    QJsonObject requestOnce(const QString &method, const QString &host,
                            quint16 port, const QString &path,
                            const QJsonObject &body, const QString &scheme,
                            bool &ok, bool &certError);

    // Зашифрованный data-запрос (enc:1) или старый proof-запрос.
    // Ответ расшифровывается, если сервер прислал _enc
    QJsonObject postData(const QString &host, quint16 port,
                         const QString &route, const QString &path,
                         const QJsonObject &body, bool &ok);

    // Подписать тело proof'ом пары (маршрут — часть challenge).
    // Без секрета (до спаривания) тело уходит как есть — сервер ответит
    // not-paired/auth-failed, гейт пары выше по стеку
    QJsonObject withAuth(const QString &route,
                         const QJsonObject &body);
    QJsonObject getJson(const QString &host, quint16 port,
                        const QString &path, bool &ok);

    AppConfig *config=nullptr;
    int timeoutMs=15000;
    QString peerSecretText;
    QString manifestScopeText;
    QString peerSessionText;   // enc-сессия из hello
    QString peerDeviceIdText;  // id сервера (для AAD/ответа)
    bool peerEnc=false;        // сервер подтвердил enc
    QString lastErrorText;
    bool lastTransportOkFlag=true;

    int tlsModeValue=0;
    QString tlsPinText;
    TlsTrustCallback tlsTrustCallback;
    bool lastUsedTlsFlag=false;
    QString lastTlsFingerprintText;
};

#endif // SYNCCLIENT_H

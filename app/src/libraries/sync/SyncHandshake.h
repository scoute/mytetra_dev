#ifndef SYNCHANDSHAKE_H
#define SYNCHANDSHAKE_H

#include <QString>
#include <QJsonObject>

class SyncServer;
class AppConfig;

// Рукопожатие sync/1 (POST /hello). Порядок проверок — от дешевого к дорогому:
// версии → чужая сеть → часы → подлинность. Первая же неудача — отказ
// с машиночитаемым кодом, чтобы клиент показал внятный текст, а не "не sync".
struct SyncHelloResult
{
    bool ok=false;
    QString errorCode;   // version-mismatch | foreign-network | clock-skew | auth-failed
    QString errorText;   // человекочитаемо, уже переведено сервером
    qint64 skewMs=0;     // оценка clientTime-serverTime (есть и при отказе по часам)
    qint64 serverTime=0;
    bool paired=false;   // у сервера есть секрет под эту пару
    QString remoteDeviceId;   // кто ответил (для фингерпринта и secret'а)
    QString remoteDeviceName;
    QString remoteGroupId;
    QString warning;     // непусто при ok: сервер что-то простил (re-pair)
};

class SyncHandshake
{
public:

    // Собрать тело /hello из локального конфига.
    // proof пуст пока спаривания не было (первое знакомство)
    static QJsonObject buildHello(AppConfig *config, qint64 clientTime);

    // Проверить чужой /hello. Ничего не меняет, только вердикт
    static SyncHelloResult checkHello(const QJsonObject &hello,
                                     AppConfig *config, qint64 serverTime);

    // Зарегистрировать POST /hello на сервере. config жив всю жизнь сервера.
    // pairingPin: предустановленный PIN для headless-спаривания (--sync-pair).
    // Если секрета еще нет, а PIN задан — секрет выводится при первом hello
    // с обеих сторон одинаково, сверка фингерпринта в этом режиме на человеке.
    // Починка пары: секрет есть, но proof чужой (второй стороне сбросили
    // состояние или ввели другой PIN раньше) — при заданном PIN не отказываем,
    // а перевыводим secret и принимаем с warning. Без PIN — строгий отказ:
    // GUI-путь с проверкой фингерпринта — отдельно, через onSyncPairing.
    // Строже (отпечатки, allowlist) — позже, см. долг в sync-mvp
    static void registerRoutes(SyncServer *server, AppConfig *config,
                               const QString &pairingPin=QString());

    // Последний неизвестный гость (hello без секрета с нашей стороны).
    // Для диалога спаривания на сервере: показать имя/фингерпринт,
    // ввести тот же PIN. Очищается при старте сервера и после спаривания
    static QString lastUnknownDeviceId(void);
    static QString lastUnknownDeviceName(void);
    static void clearUnknown(void);

    // Максимальный модуль расхождения часов, мс. Больше — железный блок
    static constexpr qint64 maxClockSkewMs=1000;
};

#endif // SYNCHANDSHAKE_H

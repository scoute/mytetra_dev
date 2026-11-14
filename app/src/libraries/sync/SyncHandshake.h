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

    // Зарегистрировать POST /hello на сервере. config жив всю жизнь сервера
    static void registerRoutes(SyncServer *server, AppConfig *config);

    // Максимальный модуль расхождения часов, мс. Больше — железный блок
    static constexpr qint64 maxClockSkewMs=1000;
};

#endif // SYNCHANDSHAKE_H

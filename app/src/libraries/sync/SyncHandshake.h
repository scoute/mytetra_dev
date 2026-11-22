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
    bool matchedPending=false; // proof сошёлся с pending-секретом
                            // (выдан по коду, ещё не повышен)
    bool pair2=false;    // сервер умеет спаривание по одноразовому коду
    bool enc=false;      // сервер подтвердил end-to-end шифрование данных
    QString session;     // сессия enc (base64), эхо сервера
    QString helloMac;    // MAC ответа hello (hex) для enc
    QString verifiedSecret; // какой секрет сошёлся (не сериализуется)
    bool pairPending=false; // сервер записал наш запрос (wait-accept),
                            // пары пока нет — ждем Accept на той стороне
    bool pairCodeRequired=false; // показан код на той стороне, введите его
    bool pairCodeLocked=false;   // слишком много попыток, код не выдан
    qint64 pairCodeExpiresAt=0;  // когда истечёт код (0 — неизвестно)
    qint64 pairCodeUntil=0;      // при блокировке — до когда ждать
    QString remoteDeviceId;   // кто ответил (для фингерпринта и secret'а)
    QString remoteDeviceName;
    QString remoteGroupId;
    QString warning;     // непусто при ok: сервер что-то простил (re-pair)
};

class SyncHandshake
{
public:

    // Собрать тело /hello из локального конфига.
    // proof пуст пока спаривания не было (первое знакомство).
    // pairRequest: "хочу в пару" — сервер запишет входящий запрос
    // (wait-accept), а не данные; ответ придет с pairPending.
    // peerSecret: секрет именно под этот пир (кольцо); пусто — легаси-одиночка
    static QJsonObject buildHello(AppConfig *config, qint64 clientTime,
                                  bool pairRequest=false,
                                  const QString &peerSecret=QString(),
                                  const QString &sessionB64=QString());

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

    // Доказательство для data-маршрутов: HMAC от (время + id + маршрут).
    // Hello проверяется один раз, а манифест/блобы без proof читал бы
    // любой в LAN пока хаб спарен. Маршрут в challenge — чтобы proof
    // с pull нельзя было переподставить в push.
    // peerSecret: секрет именно под этот пир (кольцо); пусто — легаси-одиночка
    static QJsonObject buildDataAuth(AppConfig *config, const QString &route,
                                     const QJsonObject &body=QJsonObject(),
                                     const QString &peerSecret=QString());

    // Проверить чужой data-запрос. route — тот же ключ что в маршрутах
    // ("POST /push-note"). now — время сервера. detail — человекочитаемо
    static bool checkDataAuth(AppConfig *config, const QString &route,
                              const QJsonObject &body, qint64 now,
                              QString &detail);

    // Последний неизвестный гость (hello без секрета с нашей стороны).
    // Для диалога спаривания на сервере: показать имя/фингерпринт,
    // ввести тот же PIN. Очищается при старте сервера и после спаривания
    static QString lastUnknownDeviceId(void);
    static QString lastUnknownDeviceName(void);
    // Локальный адрес, на который пришёл последний hello (для QR:
    // ровно тот интерфейс, по которому клиент реально достучался)
    static QString lastLocalAddress(void);
    static void clearUnknown(void);

    // Зафиксировать взаимность пары (первый проверенный обмен).
    // Логирует только переход 0 -> now, дальше тихо. Вызывается
    // и сервером (чистый проход hello), и клиентом (answer.paired)
    static void noteMutual(AppConfig *config, const QString &id,
                           const char *context);

    // Максимальный модуль расхождения часов, мс. Больше — железный блок
    static constexpr qint64 maxClockSkewMs=1000;

    // Максимальная давность метки data-запроса, мс. Больше — отказ
    // (защита от переигрывания). Часы уже сведены hello в пределах
    // maxClockSkewMs, здесь окно шире — на джиттер и обработку
    static constexpr qint64 maxDataTsSkewMs=5000;
};

#endif // SYNCHANDSHAKE_H

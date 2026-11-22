#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkInterface>

#include "main.h"
#include "SyncHandshake.h"
#include "SyncServer.h"
#include "SyncIdentity.h"
#include "SyncPairCrypto.h"
#include "SyncDataCrypto.h"
#include "SyncPairQr.h"
#include "SyncTls.h"
#include "SyncPairing.h"
#include "SyncDebug.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"

extern GlobalParameters globalParameters;


namespace
{
QString g_unknownId;
QString g_unknownName;

// NAT-хайрпин: под VirtualBox NAT соединение от виртуалки приходит на
// сервер с НАШИМ же адресом (пакет выходит через NAT-движок хоста).
// Писать такой адрес в строку пира нельзя: кнопка Sync now синкнулась бы
// сама с собой. Loopback оставляем — два экземпляра на одной машине
// это валидный стенд тестирования
bool isNonLoopbackLocalAddress(const QString &ip)
{
    if(ip.isEmpty())
        return false;

    const QHostAddress addr(ip);

    if(addr.isNull() || addr.isLoopback())
        return false;

    for(const QHostAddress &local : QNetworkInterface::allAddresses())
        if(local==addr)
            return true;

    return false;
}
}


// Взаимность — редкое событие (первый обмен после спаривания),
// логируем только переход 0 -> now, дальше тихо
void SyncHandshake::noteMutual(AppConfig *config, const QString &id,
                               const char *context)
{
    if(config==nullptr || id.isEmpty())
        return;

    qint64 before=0;

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
    {
        if(info.id==id)
        {
            before=info.lastMutual;
            break;
        }
    }

    config->set_syncDeviceMutual(id);

    if(before<=0)
    {
        qDebug() << "SyncHandshake: mutual pairing confirmed with"
                 << id << "(" << context << ")";
        syncDbg("handshake: mutual pairing with "+id+" ("+context+")");
    }
}


// Версия приложения мажор.минор для сверки при handshake.
// Микро не сравниваем: багфиксы не ломают протокол
static QString syncAppVersion(void)
{
    return QStringLiteral("%1.%2")
        .arg(APPLICATION_RELEASE_VERSION)
        .arg(APPLICATION_RELEASE_SUBVERSION);
}


// Версия формата базы мажор.минор
static QString syncFormatVersion(void)
{
    return QStringLiteral("%1.%2")
        .arg(CURRENT_FORMAT_VERSION)
        .arg(CURRENT_FORMAT_SUBVERSION);
}



QJsonObject SyncHandshake::buildDataAuth(AppConfig *config,
                                         const QString &route,
                                         const QJsonObject &body,
                                         const QString &peerSecret)
{
    QJsonObject answer=body;

    if(config==nullptr)
        return answer;

    const qint64 ts=QDateTime::currentMSecsSinceEpoch();
    const QString deviceId=config->get_syncDeviceId();

    // Секрет именно под этот пир (кольцо на 4-5 устройств);
    // пусто — легаси-одиночка для совместимости
    const QString secret=peerSecret.isEmpty()
        ? config->get_syncPairingSecret() : peerSecret;

    answer[QStringLiteral("_dev")]=deviceId;
    answer[QStringLiteral("_ts")]=ts;
    answer[QStringLiteral("_proof")]=
        SyncIdentity::hmacProof(secret,
                                QByteArray::number(ts)
                                +deviceId.toLatin1()+route.toLatin1());
    return answer;
}


bool SyncHandshake::checkDataAuth(AppConfig *config, const QString &route,
                                  const QJsonObject &body, qint64 now,
                                  QString &detail)
{
    if(config==nullptr || !config->hasSyncPairing())
    {
        detail=QStringLiteral("not paired");
        return false;
    }

    const QString deviceId=body.value(QStringLiteral("_dev")).toString();
    const qint64 ts=body.value(QStringLiteral("_ts")).toVariant().toLongLong();
    const QString proof=body.value(QStringLiteral("_proof")).toString();

    if(deviceId.isEmpty() || ts<=0 || proof.isEmpty())
    {
        detail=QStringLiteral("missing auth fields");
        return false;
    }

    if(qAbs(now-ts)>maxDataTsSkewMs)
    {
        detail=QStringLiteral("stale request");
        return false;
    }

    // Секрет именно под заявившегося пира (у каждого свой).
    // Неизвестный пир — тоже отказ, но внятным текстом: сервер видит
    // кто стучался (строка пишется) и ждет Pair/Accept
    const QStringList candidates=
        SyncIdentity::candidateSecrets(config, deviceId);

    if(candidates.isEmpty())
    {
        detail=QStringLiteral("unknown device, pair it first");
        return false;
    }

    for(const QString &secret : candidates)
    {
        const QString expected=
            SyncIdentity::hmacProof(secret,
                                    QByteArray::number(ts)
                                    +deviceId.toLatin1()+route.toLatin1());

        if(SyncPairCrypto::constantTimeEquals(proof.toLatin1(),
                                                 expected.toLatin1()))
            return true;
    }

    detail=QStringLiteral("bad proof");
    return false;
}


QJsonObject SyncHandshake::buildHello(AppConfig *config, qint64 clientTime,
                                        bool pairRequest,
                                        const QString &peerSecret,
                                        const QString &sessionB64)
{
    QJsonObject hello;
    hello[QStringLiteral("proto")]=QStringLiteral("sync/1");
    hello[QStringLiteral("pair2")]=1;
    hello[QStringLiteral("app")]=syncAppVersion();
    hello[QStringLiteral("format")]=syncFormatVersion();
    hello[QStringLiteral("deviceId")]=config->get_syncDeviceId();
    hello[QStringLiteral("deviceName")]=config->get_syncDeviceName();
    hello[QStringLiteral("groupId")]=config->get_syncGroupId();
    hello[QStringLiteral("clientTime")]=clientTime;

    // Запрос в пару — явный флаг: сервер отличит "стучусь" от "синк",
    // запишет входящий запрос для Accept и ответит pairPending
    if(pairRequest)
        hello[QStringLiteral("pairRequest")]=true;

    // Доказательство владения секретом: HMAC от (время + id).
    // Это старый proof: его проверяет и старый сервер, поэтому он
    // остаётся для совместимости. Новый proof2 покрывает всё hello
    // (включая enc/session/pairRequest/groupId) и используется, когда
    // обе стороны умеют data-шифрование.
    const QString secret=peerSecret.isEmpty()
        ? config->get_syncPairingSecret() : peerSecret;
    QString proof;
    QString proof2;

    if(!secret.isEmpty())
    {
        proof=SyncIdentity::hmacProof(
            secret,
            QByteArray::number(clientTime)+config->get_syncDeviceId().toLatin1());

        if(!sessionB64.isEmpty())
        {
            hello[QStringLiteral("enc")]=1;
            hello[QStringLiteral("session")]=sessionB64;

            proof2=SyncIdentity::hmacProof(
                secret,
                SyncDataCrypto::helloRequestChallenge(
                    config->get_syncDeviceId(), clientTime, sessionB64,
                    config->get_syncGroupId(),
                    config->get_syncDeviceName(), pairRequest, true));
        }
    }

    hello[QStringLiteral("proof")]=proof;

    if(!proof2.isEmpty())
        hello[QStringLiteral("proof2")]=proof2;

    return hello;
}


SyncHelloResult SyncHandshake::checkHello(const QJsonObject &hello,
                                          AppConfig *config, qint64 serverTime)
{
    SyncHelloResult result;
    result.serverTime=serverTime;

    // 1. Версии: протокол, приложение (мажор.минор), формат базы
    const QString proto=hello.value(QStringLiteral("proto")).toString();
    const QString app=hello.value(QStringLiteral("app")).toString();
    const QString format=hello.value(QStringLiteral("format")).toString();

    if(proto!=QStringLiteral("sync/1") || app!=syncAppVersion() ||
       format!=syncFormatVersion())
    {
        result.errorCode=QStringLiteral("version-mismatch");
        result.errorText=QStringLiteral("Sync versions differ, update both sides. "
                                        "ours app=%1 format=%2, theirs app=%3 format=%4")
            .arg(syncAppVersion(), syncFormatVersion(), app, format);
        return result;
    }

    // 2. Чужая сеть: обе стороны в группах, группы разные — жесткий отказ.
    // Пустая группа с любой стороны — штатно: первое спаривание или клонирование
    const QString theirGroup=hello.value(QStringLiteral("groupId")).toString();
    const QString ourGroup=config->get_syncGroupId();

    if(!theirGroup.isEmpty() && !ourGroup.isEmpty() && theirGroup!=ourGroup)
    {
        result.errorCode=QStringLiteral("foreign-network");
        result.errorText=QStringLiteral("Device belongs to another sync network. "
                                        "ours group=%1, theirs group=%2. "
                                        "Re-pair with --sync-forget on both sides.")
            .arg(ourGroup, theirGroup);
        return result;
    }

    // 3. Часы: |client-server| больше секунды — блок с подсказкой.
    // Оценка грубая (без компенсации RTT), поэтому порог 1с, а не 0.1:
    // иначе ложные блокировки на джиттере Wi-Fi
    const qint64 clientTime=hello.value(QStringLiteral("clientTime")).toVariant().toLongLong();
    result.skewMs=clientTime-serverTime;

    if(qAbs(result.skewMs)>maxClockSkewMs)
    {
        result.errorCode=QStringLiteral("clock-skew");
        result.errorText=QStringLiteral("Clock difference too large, set the time. "
                                        "skew=%1 ms (limit %2 ms). "
                                        "Compare `date +%s` on both sides.")
            .arg(result.skewMs).arg(maxClockSkewMs);
        return result;
    }

    // 4. Подлинность: есть пара (или pending-секрет после спаривания
    // по коду) — требуем верное доказательство секретом именно под этот
    // пир. Pending принимается наравне с текущим: клиент мог получить
    // новый секрет, а сервер ещё не повысил его (ответ потерялся/не доехал).
    // Секретов нет — устройство девственно чистое, впускаем к спариванию.
    const QString deviceId=hello.value(QStringLiteral("deviceId")).toString();
    const QString proof=hello.value(QStringLiteral("proof")).toString();
    const QString proof2=hello.value(QStringLiteral("proof2")).toString();
    const QString session=hello.value(QStringLiteral("session")).toString();
    const bool enc=hello.value(QStringLiteral("enc")).toInt(0)==1
                   && !session.isEmpty() && !proof2.isEmpty();
    const QString pending=config->get_syncDevicePendingSecret(deviceId);

    result.paired=config->hasSyncPairing() || !pending.isEmpty();

    if(result.paired)
    {
        const QStringList candidates=
            SyncIdentity::candidateSecrets(config, deviceId);

        bool matched=false;

        const QByteArray encChallenge=enc
            ? SyncDataCrypto::helloRequestChallenge(
                  deviceId, clientTime, session,
                  hello.value(QStringLiteral("groupId")).toString(),
                  hello.value(QStringLiteral("deviceName")).toString(),
                  hello.value(QStringLiteral("pairRequest")).toBool(false), true)
            : QByteArray();

        for(const QString &secret : candidates)
        {
            if(enc)
            {
                const QString expected=SyncIdentity::hmacProof(
                    secret, encChallenge);

                if(!expected.isEmpty()
                   && SyncPairCrypto::constantTimeEquals(proof2.toLatin1(),
                                                          expected.toLatin1()))
                {
                    matched=true;
                    result.matchedPending=!pending.isEmpty() && secret==pending;
                    result.enc=true;
                    result.session=session;
                    result.verifiedSecret=secret;
                    break;
                }

                continue;
            }

            const QString expected=SyncIdentity::hmacProof(
                secret,
                QByteArray::number(clientTime)+deviceId.toLatin1());

            if(!proof.isEmpty() && !expected.isEmpty()
               && SyncPairCrypto::constantTimeEquals(proof.toLatin1(),
                                                      expected.toLatin1()))
            {
                matched=true;
                result.matchedPending=!pending.isEmpty() && secret==pending;
                break;
            }
        }

        if(!matched)
        {
            result.errorCode=QStringLiteral("auth-failed");
            result.errorText=QStringLiteral("Pairing check failed: no pairing with this device. "
                                            "Pair via Sync devices (use the one-time code shown "
                                            "on the other device) — existing pairings stay intact. "
                                            "(Full reset only via --sync-forget.)");
            return result;
        }
    }

    result.ok=true;
    return result;
}


void SyncHandshake::registerRoutes(SyncServer *server, AppConfig *config,
                                    const QString &pairingPin)
{
    clearUnknown();
    SyncPairing::clearAll();
    SyncDataCrypto::clearAllSessions();

    // Спаривание по одноразовому коду (sync/2). Отдельный маршрут:
    // не зависит от hello, поэтому работает и при auth-failed/foreign-network
    // (клиент сбросил пару, а сервер ещё помнит старую).
    server->addRoute(QStringLiteral("POST /pair-begin"),
                     [server, config](const QJsonObject &request) -> QJsonObject {
        if(config->get_syncPairingBlocked())
            return SyncPairing::disabledAnswer();

        return SyncPairing::handleBegin(
            config->get_syncDeviceId(),
            request,
            QDateTime::currentMSecsSinceEpoch(),
            server->lastPeerAddress(),
            config->get_syncPairBindIp(),
            [config](const QString &id) {
                return config->get_syncDevicePendingSecret(id);
            },
            [config](const QString &id, const QString &secret) {
                config->set_syncDevicePendingSecret(id, secret);
            });
    });

    // Замыкание держит только указатели на долгоживущие объекты
    // (сервер и конфиг переживают все соединения)
    server->addRoute(QStringLiteral("POST /hello"),
                     [server, config, pairingPin](const QJsonObject &body) -> QJsonObject {
        const qint64 now=QDateTime::currentMSecsSinceEpoch();
        SyncHelloResult r=checkHello(body, config, now);

        const QString theirId=body.value(QStringLiteral("deviceId")).toString();
        const QString theirName=body.value(QStringLiteral("deviceName")).toString();

        // Proof сошёлся с pending-секретом (выдан по одноразовому коду):
        // это и есть подтверждение, что клиент его получил. Повышаем
        // pending -> current и гасим код: он больше не нужен
        if(r.ok && r.matchedPending && !theirId.isEmpty()
           && theirId!=config->get_syncDeviceId())
        {
            if(SyncIdentity::promotePendingSecret(config, theirId))
            {
                SyncPairing::consumeCode(theirId);
                qDebug() << "SyncHandshake: pending pair secret promoted for"
                         << theirName << "(" << theirId << ")";
                syncDbg("handshake: pending secret promoted for "+theirId);
            }
        }

        // Hello от самого себя (клиент стукнулся на свой же адрес):
        // ничего не записываем (ни строку, ни группу, ни mutual) и отказываем
        // внятно. Иначе сервер добавляет сам себя в устройства, а клиент
        // "успешно синкается" сам с собой
        if(!theirId.isEmpty() && theirId==config->get_syncDeviceId())
        {
            QJsonObject selfAnswer;
            selfAnswer[QStringLiteral("ok")]=false;
            selfAnswer[QStringLiteral("pair2")]=1;
            selfAnswer[QStringLiteral("serverTime")]=now;
            selfAnswer[QStringLiteral("paired")]=false;
            selfAnswer[QStringLiteral("deviceId")]=config->get_syncDeviceId();
            selfAnswer[QStringLiteral("deviceName")]=config->get_syncDeviceName();
            selfAnswer[QStringLiteral("groupId")]=config->get_syncGroupId();
            selfAnswer[QStringLiteral("error")]=QStringLiteral("self-hello");
            selfAnswer[QStringLiteral("detail")]=QStringLiteral("Sync with itself is not allowed: this server is that device. Check the address.");
            return selfAnswer;
        }

        // Пир постучался — запоминаем для окна Sync devices.
        // Видимость НЕ зависит от успеха hello: иначе админ не видит
        // кто пытался (кейс "клиент пишет secrets differ, а сервер его
        // даже не видит"). Доверия запись не дает: пара/mutual отдельно.
        // IP берем из сокета (в hello его нет). Порт не затираем:
        // слушающий порт пира нам неизвестен, оставляем какой был
        // (или дефолт 47777 у новой строки)
        int peerPort=47777;
        bool knownDevice=false;

        for(const AppConfig::SyncDeviceInfo &known : config->get_syncDevices())
        {
            if(known.id==theirId)
            {
                peerPort=known.port;
                knownDevice=true;
                break;
            }
        }

        const QString peerIp=server->lastPeerAddress();

        // Подтверждённый hello (proof сошёлся) — пишем адрес/имя/порт.
        // Неподтверждённый НЕ должен перезаписывать адрес уже известного
        // устройства: иначе любой в LAN присылает чужой deviceId и уводит
        // «Sync now» на свой IP. Неизвестного всё же показываем в окне
        // спаривания, но без адреса; список ограничен в AppConfig.
        if(r.ok)
            config->touch_syncDevice(theirId, theirName,
                                     isNonLoopbackLocalAddress(peerIp) ? QString()
                                                                       : peerIp,
                                     peerPort);
        else if(!knownDevice)
            config->touch_syncDevice(theirId, theirName.left(64),
                                     isNonLoopbackLocalAddress(peerIp) ? QString()
                                                                       : peerIp,
                                     47777);

        QString warning;

        // Починка пары: proof чужой, но PIN предъявлен — значит оператор
        // осознанно переспаривает (второй стороне сбросили состояние).
        // Предупреждаем, но принимаем; строже — позже.
        // Без PIN здесь делать нечего: строгий отказ уходит как есть
        if(!r.ok && r.errorCode==QStringLiteral("auth-failed")
           && !pairingPin.isEmpty()
           && !theirId.isEmpty()
           && theirId!=config->get_syncDeviceId())
        {
            if(SyncIdentity::adoptPairing(config, theirId, pairingPin))
            {
                r.ok=true;
                r.paired=true;
                r.errorCode.clear();
                r.errorText.clear();

                warning=QStringLiteral("secret mismatch, re-paired via legacy PIN "
                                       "with %1 (id %2), continuing with warning")
                    .arg(theirName, theirId);

                // Видно всегда, не только под дебагом: событие редкое и важное
                qDebug() << "SyncHandshake:" << warning;
                syncDbg("handshake: "+warning);

                SyncHandshake::noteMutual(config, theirId, "re-pair via PIN");
            }
        }

        // Чужая сеть, но PIN предъявлен: раз оператор ввел общий PIN —
        // это согласие, хаб (мы, сервер) — источник правды по группе.
        // Перевыводим secret под эту пару и велим споку взять нашу группу.
        // Без PIN — строгий отказ как был (лечится --sync-forget с одной
        // стороны: пустая группа втягивается автоматически)
        if(!r.ok && r.errorCode==QStringLiteral("foreign-network")
           && !pairingPin.isEmpty()
           && !theirId.isEmpty()
           && theirId!=config->get_syncDeviceId())
        {
            if(SyncIdentity::adoptPairing(config, theirId, pairingPin))
            {
                r.ok=true;
                r.paired=true;
                r.errorCode.clear();
                r.errorText.clear();

                warning=QStringLiteral("different network (ours %1, theirs %2): "
                                       "re-paired via legacy PIN with %3, adopt our group %1")
                    .arg(config->get_syncGroupId(),
                         body.value(QStringLiteral("groupId")).toString(),
                         theirName);

                qDebug() << "SyncHandshake:" << warning;
                syncDbg("handshake: "+warning);

                SyncHandshake::noteMutual(config, theirId, "foreign network join");
            }
        }

        QJsonObject answer;
        answer[QStringLiteral("ok")]=r.ok;
        answer[QStringLiteral("pair2")]=1;
        answer[QStringLiteral("serverTime")]=r.serverTime;
        answer[QStringLiteral("skewMs")]=r.skewMs;
        answer[QStringLiteral("paired")]=r.paired;

        // Себя называем всегда: клиент показывает имя/фингерпринт сервера
        // при спаривании, сверяет группу и считает детерминированный groupId
        answer[QStringLiteral("deviceId")]=config->get_syncDeviceId();
        answer[QStringLiteral("deviceName")]=config->get_syncDeviceName();
        answer[QStringLiteral("groupId")]=config->get_syncGroupId();

        // Запрос в пару: сервер генерирует код и показывает его оператору.
        // Делаем это ДО возврата ошибки: при auth-failed/foreign-network
        // клиент тоже должен получить pairCodeRequired и попросить код,
        // иначе повторное спаривание после сброса одной стороны невозможно.
        const bool pairReq=body.value(QStringLiteral("pairRequest"))
                               .toBool(false);

        if(pairReq && !theirId.isEmpty()
           && theirId!=config->get_syncDeviceId() && !(r.ok && r.paired))
        {
            // Запрет новых пар: не генерируем код и не показываем попап.
            // Уже спаренные устройства сюда не попадают (r.ok && r.paired)
            if(config->get_syncPairingBlocked())
            {
                answer[QStringLiteral("ok")]=false;
                answer[QStringLiteral("error")]=QStringLiteral("pairing-disabled");
                answer[QStringLiteral("detail")]=
                    QStringLiteral("New pairings are blocked on this server. "
                                   "Enable pairing on the server first.");
                return answer;
            }

            qint64 expiresAt=0;
            qint64 lockedUntil=0;
            const QString code=SyncPairing::ensureCode(
                theirId, now, server->lastPeerAddress(),
                config->get_syncPairBindIp(), &expiresAt, &lockedUntil);

            answer[QStringLiteral("pairCodeRequired")]=true;
            answer[QStringLiteral("pairPending")]=true;

            if(!code.isEmpty())
            {
                answer[QStringLiteral("pairCodeExpiresAt")]=expiresAt;

                // Headless-сервер не может показать попап: код печатаем
                // в stdout, чтобы оператор/скрипт прочитал его
                if(globalParameters.getSyncHeadlessMode())
                {
                    // printf, не qInfo: обработчик логов приложения
                    // глотает info при printdebugmessages=false
                    printf("SYNC PAIR CODE %s %s\n",
                           theirId.toLatin1().constData(),
                           SyncPairCrypto::formatCode(code)
                               .toLatin1().constData());

                    QString qrHost=config->get_syncAdvertiseHost();

                    if(qrHost.isEmpty())
                        qrHost=SyncTls::primaryAddress();

                    if(qrHost.isEmpty())
                        qrHost=QHostInfo::localHostName();

                    QString qrFingerprint=config->get_syncTlsFingerprint();

                    if(qrFingerprint.isEmpty()
                       && !config->get_syncTlsCertPath().isEmpty())
                        qrFingerprint=SyncTls::certificateFingerprint(
                            config->get_syncTlsCertPath());

                    const QString uri=SyncPairQr::buildUri(
                        qrHost, server->port(), code,
                        config->get_syncTlsEnabled(), qrFingerprint,
                        config->get_syncDeviceName());

                    printf("SYNC PAIR URI %s\n",
                           uri.toUtf8().constData());
                    fflush(stdout);
                }
            }
            else
            {
                answer[QStringLiteral("pairCodeLocked")]=true;

                if(lockedUntil>0)
                    answer[QStringLiteral("pairCodeUntil")]=lockedUntil;
            }

            // Попап на сервере: показать код и имя гостя
            config->set_syncDeviceOffer(theirId);

            qDebug() << "SyncHandshake: pair request from"
                     << theirName << "(" << theirId
                     << "), code" << (code.isEmpty() ? QStringLiteral("locked")
                                                     : QStringLiteral("shown"));
        }

        if(!r.ok)
        {
            answer[QStringLiteral("error")]=r.errorCode;
            answer[QStringLiteral("detail")]=r.errorText;
            return answer;
        }

        // Автосведение групп: пустые с обеих сторон — детерминированный
        // общий groupId (обе стороны считают одинаково, маршрут не нужен).
        // У нас пусто, у гостя есть — принимаем его. Наоборот — гость
        // примет наш сам по тем же правилам на своей стороне.
        // theirId/theirName взяты выше (нужны и починке пары)
        const QString theirGroup=body.value(QStringLiteral("groupId")).toString();

        if(config->get_syncGroupId().isEmpty())
        {
            if(!theirGroup.isEmpty())
                config->set_syncGroupId(theirGroup);
            else if(!theirId.isEmpty())
                config->set_syncGroupId(
                    SyncIdentity::pairGroupId(config->get_syncDeviceId(),
                                              theirId));
        }

        // Неизвестный гость для диалога спаривания на сервере.
        // Запоминаем всегда (нужен id для вывода секрета по PIN),
        // доверяем — только после сверки фингерпринта человеком
        if(!theirId.isEmpty() && theirId!=config->get_syncDeviceId())
        {
            g_unknownId=theirId;
            g_unknownName=theirName;
        }

        // Headless-спаривание: секрета нет, PIN задан флагом — выводим
        // тот же secret что и клиент (ид симметричны, PIN общий).
        // Флаг — стоящее согласие оператора: взаимность сразу.
        // GUI-путь с проверкой фингерпринта — отдельно, через onSyncPairing
        if(config->get_syncPairingSecret().isEmpty() && !pairingPin.isEmpty()
           && !theirId.isEmpty())
        {
            SyncIdentity::adoptPairing(config, theirId, pairingPin);
            SyncHandshake::noteMutual(config, theirId, "headless standing PIN");
        }

        if(!warning.isEmpty())
            answer[QStringLiteral("warning")]=warning;

        // Запрос в пару с уже сошедшимся proof (клиент давно спарен):
        // это не запрос, а подтверждение взаимности делом
        if(r.ok && r.paired && pairReq && !theirId.isEmpty()
           && theirId!=config->get_syncDeviceId())
            SyncHandshake::noteMutual(config, theirId, "pair-request proof");

        // Чистый проход обычного hello (proof сошелся с давним секретом,
        // без починок и запросов): взаимность подтверждена делом
        if(r.ok && r.paired && !pairReq && !theirId.isEmpty()
           && theirId!=config->get_syncDeviceId())
            SyncHandshake::noteMutual(config, theirId, "clean hello");

        // enc-ответ: эхо сессии + MAC всего ответа. Клиент проверит,
        // что сервер действительно знает секрет и не подменён в пути
        if(r.enc && !r.verifiedSecret.isEmpty())
        {
            answer[QStringLiteral("enc")]=1;
            answer[QStringLiteral("session")]=r.session;
            SyncDataCrypto::setActiveSession(theirId, r.session);

            const QByteArray macInput=SyncDataCrypto::helloResponseMacInput(
                theirId, config->get_syncDeviceId(), r.session,
                body.value(QStringLiteral("clientTime")).toVariant().toLongLong(),
                answer.value(QStringLiteral("serverTime")).toVariant().toLongLong(),
                answer.value(QStringLiteral("ok")).toBool(),
                answer.value(QStringLiteral("paired")).toBool(),
                answer.value(QStringLiteral("groupId")).toString(),
                answer.value(QStringLiteral("deviceName")).toString(),
                answer.value(QStringLiteral("pairCodeRequired")).toBool(),
                answer.value(QStringLiteral("pairCodeLocked")).toBool(),
                answer.value(QStringLiteral("pairCodeExpiresAt")).toVariant().toLongLong(),
                answer.value(QStringLiteral("warning")).toString(),
                answer.value(QStringLiteral("error")).toString(),
                answer.value(QStringLiteral("detail")).toString());

            answer[QStringLiteral("helloMac")]=
                SyncDataCrypto::helloMac(r.verifiedSecret, macInput);
        }

        return answer;
    });
}


QString SyncHandshake::lastUnknownDeviceId(void)
{
    return g_unknownId;
}


QString SyncHandshake::lastUnknownDeviceName(void)
{
    return g_unknownName;
}


void SyncHandshake::clearUnknown(void)
{
    g_unknownId.clear();
    g_unknownName.clear();
}

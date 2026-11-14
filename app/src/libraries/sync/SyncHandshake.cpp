#include <QDateTime>

#include "main.h"
#include "SyncHandshake.h"
#include "SyncServer.h"
#include "SyncIdentity.h"
#include "models/appConfig/AppConfig.h"


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


QJsonObject SyncHandshake::buildHello(AppConfig *config, qint64 clientTime)
{
    QJsonObject hello;
    hello[QStringLiteral("proto")]=QStringLiteral("sync/1");
    hello[QStringLiteral("app")]=syncAppVersion();
    hello[QStringLiteral("format")]=syncFormatVersion();
    hello[QStringLiteral("deviceId")]=config->get_syncDeviceId();
    hello[QStringLiteral("deviceName")]=config->get_syncDeviceName();
    hello[QStringLiteral("groupId")]=config->get_syncGroupId();
    hello[QStringLiteral("clientTime")]=clientTime;

    // Доказательство владения секретом: HMAC от (время + id).
    // Секрета нет (первое знакомство) — поле пустое, сервер это различает
    const QString secret=config->get_syncPairingSecret();
    QString proof;

    if(!secret.isEmpty())
    {
        proof=SyncIdentity::hmacProof(
            secret,
            QByteArray::number(clientTime)+config->get_syncDeviceId().toLatin1());
    }

    hello[QStringLiteral("proof")]=proof;
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
        result.errorText=QStringLiteral("Sync versions differ, update both sides.");
        return result;
    }

    // 2. Чужая сеть: обе стороны в группах, группы разные — жесткий отказ.
    // Пустая группа с любой стороны — штатно: первое спаривание или клонирование
    const QString theirGroup=hello.value(QStringLiteral("groupId")).toString();
    const QString ourGroup=config->get_syncGroupId();

    if(!theirGroup.isEmpty() && !ourGroup.isEmpty() && theirGroup!=ourGroup)
    {
        result.errorCode=QStringLiteral("foreign-network");
        result.errorText=QStringLiteral("Device belongs to another sync network.");
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
        result.errorText=QStringLiteral("Clock difference too large, set the time.");
        return result;
    }

    // 4. Подлинность: у нас есть секрет — требуем верное доказательство.
    // Секрета нет — устройство девственно чистое, впускаем к спариванию
    // (само спаривание с показом фингерпринтов — отдельно, позже)
    const QString secret=config->get_syncPairingSecret();
    result.paired=!secret.isEmpty();

    if(result.paired)
    {
        const QString proof=hello.value(QStringLiteral("proof")).toString();
        const QString deviceId=hello.value(QStringLiteral("deviceId")).toString();
        const QString expected=SyncIdentity::hmacProof(
            secret,
            QByteArray::number(clientTime)+deviceId.toLatin1());

        if(proof.isEmpty() || proof!=expected)
        {
            result.errorCode=QStringLiteral("auth-failed");
            result.errorText=QStringLiteral("Pairing check failed.");
            return result;
        }
    }

    result.ok=true;
    return result;
}


void SyncHandshake::registerRoutes(SyncServer *server, AppConfig *config)
{
    // Замыкание держит только указатели на долгоживущие объекты
    // (сервер и конфиг переживают все соединения)
    server->addRoute(QStringLiteral("POST /hello"),
                     [config](const QJsonObject &body) -> QJsonObject {
        const qint64 now=QDateTime::currentMSecsSinceEpoch();
        const SyncHelloResult r=checkHello(body, config, now);

        QJsonObject answer;
        answer[QStringLiteral("ok")]=r.ok;
        answer[QStringLiteral("serverTime")]=r.serverTime;
        answer[QStringLiteral("skewMs")]=r.skewMs;
        answer[QStringLiteral("paired")]=r.paired;

        if(!r.ok)
        {
            answer[QStringLiteral("error")]=r.errorCode;
            answer[QStringLiteral("detail")]=r.errorText;
        }

        return answer;
    });
}

#include <QDateTime>
#include <QDebug>

#include "main.h"
#include "SyncHandshake.h"
#include "SyncServer.h"
#include "SyncIdentity.h"
#include "models/appConfig/AppConfig.h"


namespace
{
QString g_unknownId;
QString g_unknownName;
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
            result.errorText=QStringLiteral("Pairing check failed: secrets differ. "
                                            "Re-pair both sides with the same PIN "
                                            "(clear old state with --sync-forget).");
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

    // Замыкание держит только указатели на долгоживущие объекты
    // (сервер и конфиг переживают все соединения)
    server->addRoute(QStringLiteral("POST /hello"),
                     [config, pairingPin](const QJsonObject &body) -> QJsonObject {
        const qint64 now=QDateTime::currentMSecsSinceEpoch();
        SyncHelloResult r=checkHello(body, config, now);

        const QString theirId=body.value(QStringLiteral("deviceId")).toString();
        const QString theirName=body.value(QStringLiteral("deviceName")).toString();

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

                warning=QStringLiteral("secret mismatch, re-paired via PIN "
                                       "with %1 (id %2), continuing with warning")
                    .arg(theirName, theirId);

                // Видно всегда, не только под дебагом: событие редкое и важное
                qDebug() << "SyncHandshake:" << warning;
            }
        }

        QJsonObject answer;
        answer[QStringLiteral("ok")]=r.ok;
        answer[QStringLiteral("serverTime")]=r.serverTime;
        answer[QStringLiteral("skewMs")]=r.skewMs;
        answer[QStringLiteral("paired")]=r.paired;

        // Себя называем всегда: клиент показывает имя/фингерпринт сервера
        // при спаривании, сверяет группу и считает детерминированный groupId
        answer[QStringLiteral("deviceId")]=config->get_syncDeviceId();
        answer[QStringLiteral("deviceName")]=config->get_syncDeviceName();
        answer[QStringLiteral("groupId")]=config->get_syncGroupId();

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
        // GUI-путь с проверкой фингерпринта — отдельно, через onSyncPairing
        if(config->get_syncPairingSecret().isEmpty() && !pairingPin.isEmpty()
           && !theirId.isEmpty())
        {
            SyncIdentity::adoptPairing(config, theirId, pairingPin);
        }

        if(!warning.isEmpty())
            answer[QStringLiteral("warning")]=warning;

        // Пир прошел hello — запоминаем для окна Sync devices.
        // IP здесь нет (за транспортом следит SyncServer), допишет клиент
        // при своем синке; серверная строка без IP честна
        config->touch_syncDevice(theirId, theirName);

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

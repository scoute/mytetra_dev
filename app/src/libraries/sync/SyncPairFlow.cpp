#include "SyncPairFlow.h"
#include "SyncClient.h"
#include "SyncIdentity.h"
#include "SyncPairing.h"
#include "SyncPairCrypto.h"
#include "models/appConfig/AppConfig.h"


namespace SyncPairFlow
{

Result ensurePairing(AppConfig *config, SyncClient &client,
                     const QString &host, quint16 port,
                     SyncHelloResult &hello, const QString &input)
{
    Result result;

    if(config==nullptr)
    {
        result.errorCode=QStringLiteral("pair-crypto");
        result.error=QStringLiteral("Internal error: no config");
        return result;
    }

    const QString peerId=hello.remoteDeviceId;

    if(peerId.isEmpty())
    {
        // Не подменяем сетевую ошибку («connection refused», таймаут)
        // на невнятное «сервер не представился»: по ней и так видно,
        // что на той стороне не запущен сервер синхронизации
        result.errorCode=hello.errorCode.isEmpty()
            ? QStringLiteral("no-peer") : hello.errorCode;
        result.error=hello.errorText.isEmpty()
            ? QStringLiteral("The server did not identify itself")
            : hello.errorText;
        return result;
    }

    if(peerId==config->get_syncDeviceId())
    {
        result.errorCode=QStringLiteral("self-hello");
        result.error=hello.errorText;
        return result;
    }

    // Уже есть рабочий секрет и hello прошёл — делать нечего
    if(hello.ok && !SyncIdentity::secretForPeer(config, peerId).isEmpty())
    {
        result.ok=true;
        return result;
    }

    // Чужая сеть решается UI: сначала «войти в сеть» (сброс группы),
    // потом код. Молча стирать текущую сеть здесь нельзя
    if(hello.errorCode==QStringLiteral("foreign-network"))
    {
        result.errorCode=hello.errorCode;
        result.error=hello.errorText;
        return result;
    }

    const QString normalized=SyncPairCrypto::normalizeCode(input);
    const bool codeMode=!normalized.isEmpty();

    if(codeMode)
    {
        QString pairError;
        const QString secret=client.pairWithCode(host, port, peerId,
                                                 normalized, &pairError);

        if(secret.isEmpty())
        {
            result.errorCode=pairError.isEmpty()
                ? QStringLiteral("pair-failed") : pairError;
            result.error=SyncPairing::errorText(result.errorCode);
            return result;
        }

        if(!SyncIdentity::adoptPairingSecret(config, peerId, secret))
        {
            result.errorCode=QStringLiteral("pair-failed");
            result.error=QStringLiteral("Could not save the pairing secret");
            return result;
        }

        result.pairedNow=true;
    }
    else if(!input.isEmpty())
    {
        // Легаси-PIN: headless-стенды и старые клиенты
        if(!SyncIdentity::adoptPairing(config, peerId, input))
        {
            result.errorCode=QStringLiteral("pair-failed");
            result.error=QStringLiteral("Could not save the pairing secret");
            return result;
        }

        result.pairedNow=true;
    }
    else
    {
        result.errorCode=hello.ok ? QStringLiteral("pair-code-required")
                                  : (hello.errorCode.isEmpty()
                                     ? QStringLiteral("not-paired")
                                     : hello.errorCode);
        result.error=hello.ok
            ? QStringLiteral("Not paired: enter the one-time code shown "
                             "on the other device.")
            : hello.errorText;
        return result;
    }

    // Повторный hello уже с новым секретом: сервер проверит proof
    // и повысит pending -> current
    hello=client.hello(host, port);

    if(!hello.ok)
    {
        result.errorCode=hello.errorCode;
        result.error=hello.errorText.isEmpty() ? hello.errorCode
                                               : hello.errorText;
        return result;
    }

    result.ok=true;
    return result;
}

} // namespace SyncPairFlow

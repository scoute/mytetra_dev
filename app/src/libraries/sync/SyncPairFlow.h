#ifndef SYNCPAIRFLOW_H
#define SYNCPAIRFLOW_H

#include <QString>

#include "SyncHandshake.h"

class AppConfig;
class SyncClient;

// Общий путь установки пары для GUI и SyncSession.
//
// input — либо одноразовый код sync/2 (16 base32-символа, можно с
// дефисами), либо легаси-PIN для headless-сценариев. Код имеет
// приоритет: если строка нормализуется в валидный код, PIN не пробуем.
namespace SyncPairFlow
{

struct Result
{
    bool ok=false;
    bool pairedNow=false;  // пара создана/пересоздана этим вызовом
    QString errorCode;
    QString error;
};

// Убедиться, что у нас есть рабочий секрет под hello.remoteDeviceId.
// hello обновляется повторным hello после спаривания.
Result ensurePairing(AppConfig *config, SyncClient &client,
                     const QString &host, quint16 port,
                     SyncHelloResult &hello, const QString &input);

} // namespace SyncPairFlow

#endif // SYNCPAIRFLOW_H

#ifndef SYNCDEBUG_H
#define SYNCDEBUG_H

#include <QtGlobal>
#include <QByteArray>

// Сквозной дебаг синхронизации для живых прогонов (ПК + VM без телефона).
// Включается окружением: MYTETRA_SYNC_DEBUG=1
// По умолчанию выключен: обычный синк остается тихим, в логах только итог.
// Читать один раз (static): флаг смотрят и сервер, и клиент одним процессом
inline bool syncDebugEnabled()
{
    static const bool enabled=(qgetenv("MYTETRA_SYNC_DEBUG")==QByteArray("1"));
    return enabled;
}

#endif // SYNCDEBUG_H

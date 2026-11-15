#ifndef SYNCDEBUG_H
#define SYNCDEBUG_H

#include <QtGlobal>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;

// Сквозной дебаг синхронизации для живых прогонов.
// Включается: настройками (Tools - Preferences - Misc - "Отладка синка")
// или переменной окружения MYTETRA_SYNC_DEBUG=1.
// При включенном вывод идет и в консоль (qDebug), и в журнал
// <workdir>/syncdebug.log (с ротацией на 2 МБ) — журнал можно
// просто прислать для разбора следующей проблемы.

inline bool syncDebugEnabled(void)
{
    static const bool envEnabled=(qgetenv("MYTETRA_SYNC_DEBUG")==QByteArray("1"));
    return envEnabled || mytetraConfig.get_syncDebugEnabled();
}

// Строка в журнал синка (только файл, без консоли)
inline void syncDebugLog(const QString &line)
{
    if(!syncDebugEnabled())
        return;

    const QString path=globalParameters.getWorkDirectory()
                       +QStringLiteral("/syncdebug.log");

    // Ротация: переполненный журнал уходит в .old
    QFile check(path);

    if(check.exists() && check.size()>2*1024*1024)
    {
        QFile::remove(path+QStringLiteral(".old"));
        QFile::rename(path, path+QStringLiteral(".old"));
    }

    QFile file(path);

    if(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(
                   QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
            << QLatin1Char(' ') << line << QLatin1Char('\n');
        file.close();
    }
}

// Одновременно в консоль (qDebug) и в журнал файла
inline void syncDbg(const QString &line)
{
    if(!syncDebugEnabled())
        return;

    qDebug().noquote() << QStringLiteral("Sync:") << line;
    syncDebugLog(line);
}

#endif // SYNCDEBUG_H

#ifndef SYNCDEBUG_H
#define SYNCDEBUG_H

#include <QtGlobal>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QTextStream>
#include <QThread>

#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"

#ifndef Q_OS_WIN
#include <csignal>
#include <cstdio>
#include <execinfo.h>
#include <pthread.h>
#include <unistd.h>
#endif

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

// Одновременно в консоль (stderr напрямую, в обход qDebug-хендлера,
// который в headless/GUI без "print debug messages" глотает qDebug)
// и в журнал файла
inline void syncDbg(const QString &line)
{
    if(!syncDebugEnabled())
        return;

    const QByteArray bytes=(QStringLiteral("Sync: ")+line+QLatin1Char('\n')).toUtf8();
    (void)fwrite(bytes.constData(), 1, (size_t)bytes.size(), stderr);
    fflush(stderr);
    syncDebugLog(line);
}

// --- Расширенная трассировка для отлова сегфолта после синка ---

// Адрес указателя в удобном виде для сопоставления enter/exit
inline QString syncPtr(const void *p)
{
    return QStringLiteral("0x%1")
        .arg(reinterpret_cast<quintptr>(p), QT_POINTER_SIZE*2,
             16, QLatin1Char('0'));
}

// Id текущего потока: синк обязан жить в одном (GUI) потоке;
// перескок tid между enter/exit — красный флаг
inline QString syncTid(void)
{
    return QStringLiteral("0x%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()),
             0, 16);
}

// RAII-трассер: в конструкторе — ">> enter", в деструкторе — "<< exit+мс".
// Ставится первой строкой функции: SYNC_TRACE(); — и дает полную картину
// последнего живого места перед падением даже без пошаговой отладки.
class SyncTracer
{
public:
    SyncTracer(const char *func, const char *file, int line,
               const QString &extra=QString())
        : m_func(QString::fromLatin1(func)),
          m_start(QDateTime::currentMSecsSinceEpoch())
    {
        if(!syncDebugEnabled())
            return;

        syncDbg(QStringLiteral(">> %1 [%2:%3] tid=%4 %5")
            .arg(m_func, QString::fromLatin1(file),
                 QString::number(line), syncTid(), extra));
    }

    ~SyncTracer()
    {
        if(!syncDebugEnabled())
            return;

        const qint64 elapsed=
            QDateTime::currentMSecsSinceEpoch()-m_start;
        syncDbg(QStringLiteral("<< %1 %2ms tid=%3")
            .arg(m_func, QString::number(elapsed), syncTid()));
    }

    void checkpoint(const QString &note)
    {
        if(!syncDebugEnabled())
            return;

        const qint64 elapsed=
            QDateTime::currentMSecsSinceEpoch()-m_start;
        syncDbg(QStringLiteral(".. %1 +%2ms %3")
            .arg(m_func, QString::number(elapsed), note));
    }

private:
    QString m_func;
    qint64 m_start;
};

#define SYNC_TRACE() SyncTracer syncTracerInstance(Q_FUNC_INFO, __FILE__, __LINE__)
#define SYNC_TRACE_EXTRA(extra) SyncTracer syncTracerInstance(Q_FUNC_INFO, __FILE__, __LINE__, (extra))
#define SYNC_CHECKPOINT(note) syncTracerInstance.checkpoint((note))

#ifndef Q_OS_WIN
// Ловушка сегфолта: печатает стек в консоль (stderr) и в syncdebug.log
// БЕЗ проверки флага дебага — падение должно быть видно всегда.
// Ставится один раз на старте: installSyncCrashHandler().
inline void syncCrashHandler(int sig)
{
    const char *name="UNKNOWN";

    switch(sig)
    {
    case SIGSEGV: name="SIGSEGV"; break;
    case SIGABRT: name="SIGABRT"; break;
    case SIGBUS:  name="SIGBUS";  break;
    case SIGILL:  name="SIGILL";  break;
    case SIGFPE:  name="SIGFPE";  break;
    default: break;
    }

    void *frames[64];
    const int count=backtrace(frames, 64);

    // Консоль: напрямую в stderr (async-signal-safe), qDebug тут нельзя
    char head[256];
    const int headLen=snprintf(head, sizeof(head),
        "\nSync CRASH signal=%s(%d) tid=%p, stack:\n",
        name, sig, (void*)pthread_self());
    if(headLen>0)
        (void)write(STDERR_FILENO, head, (size_t)headLen);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
    const char tail[]="--- end crash stack (send console output + syncdebug.log) ---\n";
    (void)write(STDERR_FILENO, tail, sizeof(tail)-1);

    // Файл: best effort (может не сработать внутри хендлера — консоль первична)
    {
        const QString path=globalParameters.getWorkDirectory()
                           +QStringLiteral("/syncdebug.log");
        QFile file(path);

        if(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            QTextStream out(&file);
            out << QDateTime::currentDateTime().toString(
                       QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
                << QStringLiteral(" Sync CRASH signal=") << name
                << QStringLiteral(" tid=") << syncTid()
                << QStringLiteral(" frames=") << count << QLatin1Char('\n');
            char **syms=backtrace_symbols(frames, count);
            if(syms!=nullptr)
            {
                for(int i=0; i<count; ++i)
                    out << QStringLiteral("  #") << i << QStringLiteral(" ")
                        << QString::fromLatin1(syms[i]) << QLatin1Char('\n');
                free(syms);
            }
            file.close();
        }
    }

    signal(sig, SIG_DFL);
    raise(sig);
}

inline void installSyncCrashHandler(void)
{
    static bool installed=false;

    if(installed)
        return;

    installed=true;
    signal(SIGSEGV, syncCrashHandler);
    signal(SIGABRT, syncCrashHandler);
    signal(SIGBUS,  syncCrashHandler);
    signal(SIGILL,  syncCrashHandler);
    signal(SIGFPE,  syncCrashHandler);
}
#else
inline void installSyncCrashHandler(void) {}
#endif // Q_OS_WIN

#endif // SYNCDEBUG_H

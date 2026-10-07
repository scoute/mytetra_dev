#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QDebug>

#include "VisitHistory.h"
#include "libraries/GlobalParameters.h"


extern GlobalParameters globalParameters;


// Сколько последних строк держать в файле при ротации.
// Строка короткая (~60 байт), лимита хватает на годы посещений
#define VISIT_HISTORY_KEEP_LINES 10000


VisitHistory::VisitHistory(void) : loaded(false)
{

}


VisitHistory::~VisitHistory(void)
{

}


QString VisitHistory::filePath(void) const
{
    return globalParameters.getWorkDirectory()+QStringLiteral("/visithistory.log");
}


void VisitHistory::ensureLoaded(void) const
{
    if(loaded)
        return;

    loaded=true;

    QFile historyFile(filePath());
    if(!historyFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QStringList lines=QString::fromUtf8(historyFile.readAll())
                            .split('\n', Qt::SkipEmptyParts);
    historyFile.close();

    for(const QString &line : lines)
    {
        const QJsonObject entry=QJsonDocument::fromJson(line.toUtf8()).object();
        if(entry.isEmpty())
            continue;

        const QString id=entry[QStringLiteral("id")].toString();
        if(id.isEmpty())
            continue;

        VisitEntry visitEntry;
        visitEntry.msecs=entry[QStringLiteral("t")].toVariant().toLongLong();
        visitEntry.id=id;
        entries << visitEntry;
    }
}


// Записать посещение. Пустые id молча отсекаются
void VisitHistory::logVisit(const QString &id)
{
    if(id.isEmpty())
        return;

    ensureLoaded();

    VisitEntry visitEntry;
    visitEntry.msecs=QDateTime::currentMSecsSinceEpoch();
    visitEntry.id=id;
    entries << visitEntry;

    QJsonObject entry;
    entry[QStringLiteral("t")]=visitEntry.msecs;
    entry[QStringLiteral("id")]=id;
    appendLine(QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact)));

    emit visitLogged(id);
}


// Агрегат по журналу: визиты и последнее время для каждого id.
// Без сортировки, порядок раскладывает панель
QList<VisitStat> VisitHistory::stats(void) const
{
    ensureLoaded();

    QMap<QString, VisitStat> aggregate;
    for(const VisitEntry &visitEntry : entries)
    {
        auto it=aggregate.find(visitEntry.id);
        if(it==aggregate.end())
        {
            VisitStat stat;
            stat.id=visitEntry.id;
            stat.count=1;
            stat.lastMsecs=visitEntry.msecs;
            aggregate.insert(visitEntry.id, stat);
        }
        else
        {
            it->count++;
            if(visitEntry.msecs > it->lastMsecs)
                it->lastMsecs=visitEntry.msecs;
        }
    }

    return aggregate.values();
}


// Убрать одну заметку из журнала: память чистится сразу,
// файл переписывается целиком
void VisitHistory::forget(const QString &id)
{
    if(id.isEmpty())
        return;

    ensureLoaded();

    for(int i=entries.size()-1; i>=0; --i)
    {
        if(entries.at(i).id==id)
            entries.removeAt(i);
    }

    rewriteFile();
}


// Очистить журнал на диске и в памяти
void VisitHistory::clear(void)
{
    entries.clear();
    loaded=true;

    QFile historyFile(filePath());
    if(historyFile.exists())
        historyFile.remove();
}


// Переписать файл целиком из памяти
void VisitHistory::rewriteFile(void) const
{
    QFile historyFile(filePath());
    if(!historyFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    QTextStream historyOut(&historyFile);
    for(const VisitEntry &visitEntry : entries)
    {
        QJsonObject entry;
        entry[QStringLiteral("t")]=visitEntry.msecs;
        entry[QStringLiteral("id")]=visitEntry.id;
        historyOut << QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact)) << '\n';
    }
    historyFile.close();
}


void VisitHistory::appendLine(const QString &line)
{
    QFile historyFile(filePath());
    if(!historyFile.open(QIODevice::Append | QIODevice::Text))
        return;

    if(historyFile.size() > 512*1024)
    {
        historyFile.close();

        ensureLoaded();

        if(entries.size() > VISIT_HISTORY_KEEP_LINES)
        {
            entries=entries.mid(entries.size()-VISIT_HISTORY_KEEP_LINES);

            rewriteFile();

            return;
        }
    }

    QTextStream historyOut(&historyFile);
    historyOut << line << '\n';
    historyFile.close();
}

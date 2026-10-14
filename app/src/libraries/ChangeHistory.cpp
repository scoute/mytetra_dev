#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

#include "ChangeHistory.h"
#include "libraries/UnifiedDiff.h"


static QString recordFileName(const QString &recordId)
{
    // Идентификаторы внутренние, но на всякий без путей
    return QFileInfo(recordId).fileName()+QStringLiteral(".html");
}


QString ChangeHistory::historyDir(const QString &tetradir)
{
    return QDir(tetradir).absoluteFilePath(QStringLiteral(".notehistory"));
}


bool ChangeHistory::snapshot(const QString &tetradir, const QString &recordId,
                             const QString &recordName, const QString &html)
{
    if(tetradir.isEmpty() || recordId.isEmpty() || html.isEmpty())
        return false;

    if(!GitHelper::isAvailable())
        return false;

    const QString dir=historyDir(tetradir);

    if(!QDir(dir).exists())
    {
        if(!QDir().mkpath(dir))
            return false;

        if(!GitHelper::initRepo(dir))
            return false;
    }

    const QString fileName=recordFileName(recordId);
    const QString filePath=QDir(dir).absoluteFilePath(fileName);

    // Без изменений не снимается: сравнение с последней версией.
    // Нового файла в HEAD нет значит первое сохранение
    const QString previous=GitHelper::showFile(dir, QStringLiteral("HEAD"), fileName);

    if(!previous.isNull() && previous==html)
        return false;

    QFile file(filePath);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(html.toUtf8());
    file.close();

    if(!GitHelper::addPaths(dir, QStringList() << fileName))
        return false;

    const QString stamp=QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    const QString message=QStringLiteral("%1 (%2)").arg(recordName, stamp);

    if(!GitHelper::commitStaged(dir, message))
        return false;

    qDebug() << "ChangeHistory snapshot" << recordId << recordName;

    return true;
}


QList<ChangeHistory::Version> ChangeHistory::versions(const QString &tetradir,
                                                     const QString &recordId)
{
    QList<Version> result;

    if(tetradir.isEmpty() || recordId.isEmpty())
        return result;

    if(!GitHelper::isAvailable())
        return result;

    const QString dir=historyDir(tetradir);

    if(!GitHelper::isRepo(dir))
        return result;

    const QList<GitHelper::LogEntry> log=
        GitHelper::logDetailed(dir, 0, recordFileName(recordId));

    for(const GitHelper::LogEntry &entry : log)
    {
        Version version;
        version.rev=entry.rev;
        version.date=entry.date;
        version.subject=entry.subject;
        result.append(version);
    }

    return result;
}


QString ChangeHistory::contentAt(const QString &tetradir, const QString &recordId,
                                 const QString &rev)
{
    if(tetradir.isEmpty() || recordId.isEmpty() || rev.isEmpty())
        return QString();

    if(!GitHelper::isAvailable())
        return QString();

    const QString dir=historyDir(tetradir);

    if(!GitHelper::isRepo(dir))
        return QString();

    return GitHelper::showFile(dir, rev, recordFileName(recordId));
}


QString ChangeHistory::diffToCurrent(const QString &tetradir, const QString &recordId,
                                     const QString &rev, const QString &currentHtml,
                                     const QString &title)
{
    const QString oldHtml=contentAt(tetradir, recordId, rev);

    if(oldHtml.isNull())
        return QString();

    return UnifiedDiff::diffLines(UnifiedDiff::htmlToLines(oldHtml),
                                  UnifiedDiff::htmlToLines(currentHtml),
                                  title);
}

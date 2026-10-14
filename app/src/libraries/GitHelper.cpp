#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>

#include "GitHelper.h"


static bool gitAvailableCache=false;
static bool gitAvailableChecked=false;
static QString gitVersionCache;


bool GitHelper::isAvailable(void)
{
    if(!gitAvailableChecked)
    {
        gitAvailableChecked=true;

        QString out;
        QProcess probe;
        probe.start(QStringLiteral("git"), QStringList() << QStringLiteral("--version"));
        probe.waitForFinished(10000);

        if(probe.exitStatus()==QProcess::NormalExit && probe.exitCode()==0)
        {
            out=QString::fromLocal8Bit(probe.readAllStandardOutput()).trimmed();

            // "git version 2.43.0" -> "2.43.0"
            const QString prefix=QStringLiteral("git version ");

            if(out.startsWith(prefix))
            {
                gitAvailableCache=true;
                gitVersionCache=out.mid(prefix.size()).trimmed();
            }
        }

        qDebug() << "GitHelper available" << gitAvailableCache << gitVersionCache;
    }

    return gitAvailableCache;
}


QString GitHelper::version(void)
{
    if(!isAvailable())
        return QString();

    return gitVersionCache;
}


int GitHelper::run(const QString &workDir, const QStringList &args,
                   QString *stdoutText, QString *stderrText, int timeoutMs)
{
    if(!isAvailable())
        return -1;

    QProcess git;
    git.setWorkingDirectory(workDir);
    git.start(QStringLiteral("git"), args);

    if(!git.waitForStarted(10000))
        return -1;

    if(!git.waitForFinished(timeoutMs))
    {
        git.kill();
        git.waitForFinished(5000);

        return -1;
    }

    if(stdoutText!=nullptr)
        *stdoutText=QString::fromLocal8Bit(git.readAllStandardOutput());

    if(stderrText!=nullptr)
        *stderrText=QString::fromLocal8Bit(git.readAllStandardError());

    if(git.exitStatus()!=QProcess::NormalExit)
        return -1;

    return git.exitCode();
}


bool GitHelper::isRepo(const QString &dir)
{
    if(!isAvailable() || dir.isEmpty())
        return false;

    QString out;
    const int code=run(dir, QStringList() << QStringLiteral("rev-parse")
                                          << QStringLiteral("--git-dir"), &out);

    return code==0 && !out.trimmed().isEmpty();
}


QString GitHelper::statusPorcelain(const QString &dir)
{
    QString out;

    if(run(dir, QStringList() << QStringLiteral("status")
                              << QStringLiteral("--porcelain"), &out)!=0)
        return QString();

    return out;
}


QString GitHelper::diffWorkdir(const QString &dir, const QString &path)
{
    QStringList args;
    args << QStringLiteral("diff") << QStringLiteral("--no-color");

    if(!path.isEmpty())
    {
        args << QStringLiteral("--") << path;
    }

    QString out;

    if(run(dir, args, &out)!=0)
        return QString();

    return out;
}


QString GitHelper::diffAgainst(const QString &dir, const QString &rev, const QString &path)
{
    if(rev.isEmpty())
        return QString();

    QStringList args;
    args << QStringLiteral("diff") << QStringLiteral("--no-color") << rev;

    if(!path.isEmpty())
    {
        args << QStringLiteral("--") << path;
    }

    QString out;

    if(run(dir, args, &out)!=0)
        return QString();

    return out;
}


QStringList GitHelper::logOneline(const QString &dir, int limit, const QString &path)
{
    QStringList args;
    args << QStringLiteral("log") << QStringLiteral("--oneline") << QStringLiteral("--no-color");

    if(limit>0)
    {
        args << QStringLiteral("-n") << QString::number(limit);
    }

    if(!path.isEmpty())
    {
        args << QStringLiteral("--") << path;
    }

    QString out;

    if(run(dir, args, &out)!=0)
        return QStringList();

    return out.split(QStringLiteral("\n"), Qt::SkipEmptyParts);
}


QList<GitHelper::LogEntry> GitHelper::logDetailed(const QString &dir, int limit,
                                                 const QString &path)
{
    QList<LogEntry> entries;

    QStringList args;
    args << QStringLiteral("log")
         << QStringLiteral("--format=%H%x01%ad%x01%s")
         << QStringLiteral("--date=iso")
         << QStringLiteral("--no-color");

    if(limit>0)
    {
        args << QStringLiteral("-n") << QString::number(limit);
    }

    if(!path.isEmpty())
    {
        args << QStringLiteral("--") << path;
    }

    QString out;

    if(run(dir, args, &out)!=0)
        return entries;

    const QStringList lines=out.split(QStringLiteral("\n"), Qt::SkipEmptyParts);

    for(const QString &line : lines)
    {
        const QStringList parts=line.split(QLatin1Char('\x01'));

        if(parts.size()<3)
            continue;

        LogEntry entry;
        entry.rev=parts.at(0);
        entry.date=parts.at(1);
        entry.subject=parts.at(2);
        entries.append(entry);
    }

    return entries;
}


QString GitHelper::showFile(const QString &dir, const QString &rev, const QString &path)
{
    if(rev.isEmpty() || path.isEmpty())
        return QString();

    QString out;

    if(run(dir, QStringList() << QStringLiteral("show")
                              << QStringLiteral("%1:%2").arg(rev, path), &out)!=0)
        return QString();

    return out;
}


QString GitHelper::headRev(const QString &dir)
{
    QString out;

    if(run(dir, QStringList() << QStringLiteral("rev-parse")
                              << QStringLiteral("HEAD"), &out)!=0)
        return QString();

    return out.trimmed();
}


bool GitHelper::initRepo(const QString &dir)
{
    if(dir.isEmpty())
        return false;

    return run(dir, QStringList() << QStringLiteral("init"))==0;
}


bool GitHelper::addPaths(const QString &dir, const QStringList &paths)
{
    if(paths.isEmpty())
        return true;

    QStringList args;
    args << QStringLiteral("add") << QStringLiteral("--") << paths;

    return run(dir, args)==0;
}


bool GitHelper::commitStaged(const QString &dir, const QString &message)
{
    QStringList args;
    args << QStringLiteral("-c") << QStringLiteral("user.name=MyTetra")
         << QStringLiteral("-c") << QStringLiteral("user.email=mytetra@local")
         << QStringLiteral("commit") << QStringLiteral("-m") << message;

    return run(dir, args)==0;
}


bool GitHelper::setLocalIdentity(const QString &dir, const QString &name,
                                 const QString &email)
{
    if(!name.isEmpty())
    {
        if(run(dir, QStringList() << QStringLiteral("config")
                                  << QStringLiteral("user.name") << name)!=0)
            return false;
    }

    if(!email.isEmpty())
    {
        if(run(dir, QStringList() << QStringLiteral("config")
                                  << QStringLiteral("user.email") << email)!=0)
            return false;
    }

    return true;
}


bool GitHelper::commitPathIfChanged(const QString &dir, const QString &message,
                                    const QString &relPath, QString *hash)
{
    if(hash!=nullptr)
        hash->clear();

    QString out;
    QStringList statusArgs;
    statusArgs << QStringLiteral("status") << QStringLiteral("--porcelain");

    if(!relPath.isEmpty())
    {
        statusArgs << QStringLiteral("--") << relPath;
    }

    if(run(dir, statusArgs, &out)!=0)
        return false;

    // Изменений нет: коммит не нужен
    if(out.trimmed().isEmpty())
    {
        if(hash!=nullptr)
            *hash=headRev(dir);

        return true;
    }

    QStringList addArgs;
    addArgs << QStringLiteral("add") << QStringLiteral("--");

    if(!relPath.isEmpty())
        addArgs << relPath;
    else
        addArgs << QStringLiteral("-A");

    if(run(dir, addArgs)!=0)
        return false;

    if(!commitStaged(dir, message))
        return false;

    if(hash!=nullptr)
        *hash=headRev(dir);

    return true;
}


bool GitHelper::commitAllIfChanged(const QString &dir, const QString &message,
                                   QString *hash)
{
    return commitPathIfChanged(dir, message, QString(), hash);
}


QString GitHelper::findRepositoryRoot(const QString &dir)
{
    QDir current(dir);

    for(int level=0; level<32; ++level)
    {
        if(QFileInfo::exists(current.absoluteFilePath(".git")))
            return current.canonicalPath();

        if(!current.cdUp())
            return QString();
    }

    return QString();
}


QStringList GitHelper::listFilesInRef(const QString &repo, const QString &ref,
                                      const QString &pathspec)
{
    QStringList args;
    args << QStringLiteral("ls-tree") << QStringLiteral("-r")
         << QStringLiteral("--name-only") << ref;

    if(!pathspec.isEmpty())
    {
        args << QStringLiteral("--") << pathspec;
    }

    QString out;
    run(repo, args, &out);

    QStringList result;
    const QStringList lines=out.split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    for(const QString &line : lines)
        result << line.trimmed();

    return result;
}


bool GitHelper::checkoutFromRef(const QString &repo, const QString &ref,
                                const QString &pathspec, QString *errorMessage)
{
    QStringList args;
    args << QStringLiteral("checkout") << QStringLiteral("-f") << ref
         << QStringLiteral("--") << pathspec;

    QString err;
    const int code=run(repo, args, nullptr, &err);

    if(code!=0 && errorMessage!=nullptr)
        *errorMessage=QStringLiteral("git checkout ")+ref+QStringLiteral(" -- ")+pathspec
                      +QStringLiteral(" failed (exit ")+QString::number(code)+QStringLiteral(")");

    return code==0;
}


QStringList GitHelper::getLogOneline(const QString &repo)
{
    QString out;

    if(run(repo, QStringList() << QStringLiteral("log")
                               << QStringLiteral("--oneline"), &out)!=0)
        return QStringList();

    return out.split(QStringLiteral("\n"), Qt::SkipEmptyParts);
}


bool GitHelper::archiveHistory(const QString &repo, const QString &message,
                               QString *errorMessage)
{
    // Требуется репозиторий минимум с одним коммитом
    if(headRev(repo).isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Journal repository is empty");
        return false;
    }

    QString branch;
    if(run(repo, QStringList() << QStringLiteral("rev-parse")
                               << QStringLiteral("--abbrev-ref")
                               << QStringLiteral("HEAD"), &branch)!=0
       || branch.trimmed().isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot detect journal branch name");
        return false;
    }
    branch=branch.trimmed();

    // Временная ветка с пустым корнем (рабочий каталог не трогается)
    const QString tempBranch=QStringLiteral("snapshot_arch_")+
                             QString::number(QDateTime::currentMSecsSinceEpoch());
    if(run(repo, QStringList() << QStringLiteral("checkout")
                               << QStringLiteral("--orphan") << tempBranch)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git checkout --orphan failed");
        return false;
    }

    // Всё текущее состояние добавляется как единое корневое состояние
    if(run(repo, QStringList() << QStringLiteral("add") << QStringLiteral("--all"))!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git add --all failed");
        return false;
    }

    if(run(repo, QStringList() << QStringLiteral("commit")
                               << QStringLiteral("--allow-empty")
                               << QStringLiteral("-m") << message)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git commit failed during archive");
        return false;
    }

    // Старая ветка получает новое корневое состояние; прежняя история
    // становится недостижимой и удаляется из reflog + уборкой объектов
    if(run(repo, QStringList() << QStringLiteral("branch")
                               << QStringLiteral("-M") << branch)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git branch -M failed during archive");
        return false;
    }

    run(repo, QStringList() << QStringLiteral("reflog") << QStringLiteral("expire")
                            << QStringLiteral("--expire=now") << QStringLiteral("--all"));
    run(repo, QStringList() << QStringLiteral("gc") << QStringLiteral("--prune=now"));

    return true;
}

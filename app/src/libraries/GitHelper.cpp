#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QSet>
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
                   QString *stdoutText, QString *stderrText, int timeoutMs,
                   const QByteArray *stdinData)
{
    if(!isAvailable())
        return -1;

    QProcess git;
    git.setWorkingDirectory(workDir);
    git.start(QStringLiteral("git"), args);

    if(!git.waitForStarted(10000))
        return -1;

    if(stdinData!=nullptr)
    {
        git.write(*stdinData);
        git.closeWriteChannel();
    }

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
    args << QStringLiteral("-c") << QStringLiteral("user.name=SyncTetra")
         << QStringLiteral("-c") << QStringLiteral("user.email=synctetra@local")
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


bool GitHelper::truncateFileHistory(const QString &dir, const QString &file,
                                    const QString &targetRev,
                                    QString *errorMessage)
{
    if(!isRepo(dir))
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Not a git repository");
        return false;
    }

    if(file.isEmpty() || targetRev.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Empty file or revision");
        return false;
    }

    // Целевая ревизия обязана существовать
    QString verifyOut;
    if(run(dir, QStringList() << QStringLiteral("rev-parse") << QStringLiteral("--verify")
                              << targetRev+QStringLiteral("^{commit}"), &verifyOut)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Revision is not found");
        return false;
    }

    // Рабочее дерево обязано быть чистым: иначе reset/cherry-pick
    // может затереть незакоммиченное
    if(!statusPorcelain(dir).trimmed().isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Workdir has uncommitted changes");
        return false;
    }

    const QString origHead=headRev(dir);

    if(origHead.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Journal is empty");
        return false;
    }

    // Коммиты новее целевого, касающиеся файла: кандидаты на снос
    QString droppedOut;
    if(run(dir, QStringList() << QStringLiteral("log") << QStringLiteral("--format=%H")
                              << targetRev+QStringLiteral("..HEAD")
                              << QStringLiteral("--") << file, &droppedOut)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot list file history");
        return false;
    }

    const QStringList dropped=droppedOut.split(QStringLiteral("\n"), Qt::SkipEmptyParts);

    // Сносить нечего: целевая уже голова истории файла
    if(dropped.isEmpty())
        return true;

    // Остальные коммиты диапазона (чужие файлы) сохраняются
    // переигрыванием поверх целевого. Порядок от старых к новым
    QString keptOut;
    if(run(dir, QStringList() << QStringLiteral("log") << QStringLiteral("--reverse")
                              << QStringLiteral("--format=%H")
                              << targetRev+QStringLiteral("..HEAD"), &keptOut)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot list journal");
        return false;
    }

    const QSet<QString> droppedSet=dropped.toSet();
    QStringList replay;

    for(const QString &rev : keptOut.split(QStringLiteral("\n"), Qt::SkipEmptyParts))
    {
        if(!droppedSet.contains(rev.trimmed()))
            replay << rev.trimmed();
    }

    // Точка отката: ветка и workdir встают на целевую.
    // Чужих файлов это не касается: они вернутся переигрыванием
    if(run(dir, QStringList() << QStringLiteral("reset") << QStringLiteral("--hard")
                              << targetRev)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot reset to target revision");
        return false;
    }

    // Переигрывание чужих коммитов. Конфликтов быть не должно:
    // переигрываемые коммиты файл не трогают по построению
    for(const QString &rev : replay)
    {
        QString err;

        if(run(dir, QStringList() << QStringLiteral("cherry-pick") << rev,
               nullptr, &err)!=0)
        {
            run(dir, QStringList() << QStringLiteral("cherry-pick")
                                   << QStringLiteral("--abort"));

            // Откат отката: исходная голова восстанавливается целиком,
            // рабочее дерево было чистым поэтому потерь нет
            run(dir, QStringList() << QStringLiteral("reset") << QStringLiteral("--hard")
                                   << origHead);

            if(errorMessage)
                *errorMessage=QStringLiteral("Cannot replay journal, restored: ")+err.trimmed();

            return false;
        }
    }

    return true;
}


bool GitHelper::squashFileHistory(const QString &dir, const QString &file,
                                  int keepNewest, int mode, QString *errorMessage)
{
    if(!isRepo(dir))
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Not a git repository");
        return false;
    }

    if(file.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Empty file");
        return false;
    }

    // Хранится genesis плюс свежие: меньше двух хранить нечего
    if(keepNewest<2)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Keep at least genesis and one version");
        return false;
    }

    if(!statusPorcelain(dir).trimmed().isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Workdir has uncommitted changes");
        return false;
    }

    const QString origHead=headRev(dir);

    if(origHead.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Journal is empty");
        return false;
    }

    // Коммиты файла от старых к новым
    QString fileLog;
    if(run(dir, QStringList() << QStringLiteral("log") << QStringLiteral("--reverse")
                              << QStringLiteral("--format=%H")
                              << QStringLiteral("--") << file, &fileLog)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot list file history");
        return false;
    }

    const QStringList fileCommits=fileLog.split(QStringLiteral("\n"), Qt::SkipEmptyParts);

    // Укладываемся в лимит: делать нечего
    if(fileCommits.size()<=keepNewest)
        return true;

    const int count=fileCommits.size();

    // Хранимые по стратегии. Genesis (индекс 0) якорь отката,
    // хранится во всех стратегиях
    QSet<QString> keepSet;
    keepSet.insert(fileCommits.first().trimmed());

    if(mode==0)
    {
        // Удалить старые: хранятся genesis плюс свежие.
        // Дефолтная стратегия
        for(int i=count-(keepNewest-1); i<count; ++i)
            keepSet.insert(fileCommits.at(i).trimmed());
    }
    else if(mode==2)
    {
        // Паковать каждую 10-ю исключая свежие 10: свежие целиком,
        // ниже каждая 10-я назад от границы свежей зоны.
        // Перебор сверх бюджета сносится старейшими пакетами
        const int freshCount=qMin(10, keepNewest-1);
        const int boundary=count-freshCount;

        for(int i=boundary; i<count; ++i)
            keepSet.insert(fileCommits.at(i).trimmed());

        QList<int> bundles;
        for(int i=boundary-10; i>0; i-=10)
            bundles.append(i);

        // Сколько пакетов влезает сверх genesis и свежей зоны
        int room=keepNewest-1-freshCount;

        // Свежие пакеты с конца: старые пакеты уходят первыми
        for(int b=bundles.size()-1; b>=0 && room>0; --b)
        {
            keepSet.insert(fileCommits.at(bundles.at(b)).trimmed());
            --room;
        }
    }
    else
    {
        // Склеить похожие соседние: умный отбор сносимых
        // (точки разворота и свежая зона хранятся).
        // При недоступности содержимого тупой отбор
        const int freshSize=qMin(20, keepNewest-1);
        const int firstFresh=count-freshSize;

        QSet<QString> pinned=keepSet;
        for(int i=firstFresh; i<count; ++i)
            pinned.insert(fileCommits.at(i).trimmed());

        const QSet<QString> smartDrop=smartDropSet(dir, file, fileCommits, pinned,
                                                    keepNewest, freshSize);

        for(const QString &rev : fileCommits)
        {
            const QString hash=rev.trimmed();

            if(!smartDrop.contains(hash))
                keepSet.insert(hash);
        }
    }

    // Сносимые: все коммиты файла минус хранимые
    QSet<QString> dropSet;
    for(const QString &rev : fileCommits)
    {
        const QString hash=rev.trimmed();

        if(!keepSet.contains(hash))
            dropSet.insert(hash);
    }

    if(dropSet.isEmpty())
        return true;

    // База переигрывания: родитель старейшего сносимого.
    // Существует всегда: genesis хранится, сносится только середина
    QString oldestDropped;
    for(const QString &rev : fileCommits)
    {
        if(dropSet.contains(rev.trimmed()))
        {
            oldestDropped=rev.trimmed();
            break;
        }
    }

    QString baseOut;
    if(run(dir, QStringList() << QStringLiteral("rev-parse")
                              << oldestDropped+QStringLiteral("^"), &baseOut)!=0 ||
       baseOut.trimmed().isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot find replay base");
        return false;
    }
    const QString base=baseOut.trimmed();

    // Диапазон переигрывания от старых к новым
    QString rangeLog;
    if(run(dir, QStringList() << QStringLiteral("log") << QStringLiteral("--reverse")
                              << QStringLiteral("--format=%H")
                              << base+QStringLiteral("..HEAD"), &rangeLog)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot list journal");
        return false;
    }

    if(run(dir, QStringList() << QStringLiteral("reset") << QStringLiteral("--hard")
                              << base)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot reset to replay base");
        return false;
    }

    // Переигрывание: сносимые пропускаются, хранимые файловые
    // переносятся checkout+commit с метаданными (diff на сдвинутой
    // базе мог не лечь), чужие обычным cherry-pick
    for(const QString &revRaw : rangeLog.split(QStringLiteral("\n"), Qt::SkipEmptyParts))
    {
        const QString rev=revRaw.trimmed();

        if(dropSet.contains(rev))
            continue;

        if(keepSet.contains(rev))
        {
            // Метаданные оригинала: автор, почта, дата, тема, тело.
            // Сепаратор \x1f: NUL (%x00) режется QString-конверсией
            QString metaOut;
            if(run(dir, QStringList() << QStringLiteral("log") << QStringLiteral("-1")
                                      << QStringLiteral("--format=%an%x1f%ae%x1f%aI%x1f%s%x1f%b")
                                      << rev, &metaOut)!=0)
            {
                run(dir, QStringList() << QStringLiteral("reset") << QStringLiteral("--hard")
                                       << origHead);

                if(errorMessage)
                    *errorMessage=QStringLiteral("Cannot read commit metadata, restored");
                return false;
            }

            const QStringList meta=metaOut.split(QLatin1Char('\x1f'));
            const QString author=(meta.size()>0 ? meta.at(0) : QStringLiteral("MyTetra")).trimmed();
            const QString email=(meta.size()>1 ? meta.at(1) : QStringLiteral("mytetra@local")).trimmed();
            const QString date=(meta.size()>2 ? meta.at(2) : QString()).trimmed();
            QString message=(meta.size()>3 ? meta.at(3) : QString());

            // Тело склеивается обратно: теоретический \x1f внутри
            // тела при склейке восстанавливается
            QString body;
            for(int i=4; i<meta.size(); ++i)
            {
                if(i>4)
                    body+=QLatin1Char('\x1f');
                body+=meta.at(i);
            }

            if(!body.trimmed().isEmpty())
                message+=QStringLiteral("\n\n")+body;

            if(run(dir, QStringList() << QStringLiteral("checkout")
                                      << rev << QStringLiteral("--") << file)!=0)
            {
                run(dir, QStringList() << QStringLiteral("reset") << QStringLiteral("--hard")
                                       << origHead);

                if(errorMessage)
                    *errorMessage=QStringLiteral("Cannot restore file version, restored");
                return false;
            }

            // Пустой перенос: содержимое совпало с текущим деревом
            // (дубликат через версию). Коммитить нечего, версия
            // поглощается без ошибки
            if(run(dir, QStringList() << QStringLiteral("diff") << QStringLiteral("--quiet")
                                      << QStringLiteral("HEAD") << QStringLiteral("--")
                                      << file)==0)
                continue;

            QStringList commitArgs;
            commitArgs << QStringLiteral("-c") << QStringLiteral("user.name=MyTetra")
                       << QStringLiteral("-c") << QStringLiteral("user.email=mytetra@local")
                       << QStringLiteral("commit")
                       << QStringLiteral("--author") << author+QStringLiteral(" <")+email+QStringLiteral(">");

            if(!date.isEmpty())
                commitArgs << QStringLiteral("--date") << date;

            commitArgs << QStringLiteral("-m") << message;

            QString commitErr;
            if(run(dir, commitArgs, nullptr, &commitErr)!=0)
            {
                run(dir, QStringList() << QStringLiteral("reset") << QStringLiteral("--hard")
                                       << origHead);

                if(errorMessage)
                    *errorMessage=QStringLiteral("Cannot replay file version, restored: ")+commitErr.trimmed();

                return false;
            }

            continue;
        }

        QString err;
        if(run(dir, QStringList() << QStringLiteral("cherry-pick") << rev,
               nullptr, &err)!=0)
        {
            run(dir, QStringList() << QStringLiteral("cherry-pick")
                                   << QStringLiteral("--abort"));
            run(dir, QStringList() << QStringLiteral("reset") << QStringLiteral("--hard")
                                   << origHead);

            if(errorMessage)
                *errorMessage=QStringLiteral("Cannot replay journal, restored: ")+err.trimmed();

            return false;
        }
    }

    return true;
}


QList<QByteArray> GitHelper::readBlobs(const QString &dir,
                                       const QStringList &revs)
{
    QList<QByteArray> result;

    if(!isAvailable() || revs.isEmpty())
        return result;

    QByteArray input;
    for(const QString &rev : revs)
        input+=rev.toLatin1()+QByteArrayLiteral("\n");

    // Сырой QProcess: QString-конверсия run() режет NUL-байты,
    // а содержимое обязано дойти побайтово
    QProcess git;
    git.setWorkingDirectory(dir);
    git.start(QStringLiteral("git"),
              QStringList() << QStringLiteral("cat-file") << QStringLiteral("--batch"));

    if(!git.waitForStarted(10000))
        return QList<QByteArray>();

    git.write(input);
    git.closeWriteChannel();

    if(!git.waitForFinished(60000) || git.exitCode()!=0)
        return QList<QByteArray>();

    const QByteArray out=git.readAllStandardOutput();

    // Разбор: "<blob-hash> blob <size>\n" + ровно size байт + "\n".
    // Порядок гарантирован протоколом batch (строго по запросам),
    // имя в заголовке это резолвленный хеш, а не строка запроса
    int pos=0;
    for(int n=0; n<revs.size(); ++n)
    {
        const int eol=out.indexOf('\n', pos);

        if(eol<0)
            return QList<QByteArray>();

        const QList<QByteArray> header=out.mid(pos, eol-pos).split(' ');

        if(header.size()<3 || header.at(1)!=QByteArrayLiteral("blob"))
            return QList<QByteArray>();

        bool ok=false;
        const int size=header.at(2).toInt(&ok);

        if(!ok || size<0 || eol+1+size>out.size())
            return QList<QByteArray>();

        result.append(out.mid(eol+1, size));
        pos=eol+1+size;

        // Завершающий перенос после содержимого
        if(pos<out.size() && out.at(pos)=='\n')
            ++pos;
    }

    if(result.size()!=revs.size())
        return QList<QByteArray>();

    return result;
}


GitHelper::StepDirection GitHelper::stepDirection(const QSet<QString> &before,
                                                  const QSet<QString> &after)
{
    bool added=false;
    bool removed=false;

    for(const QString &line : after)
    {
        if(!before.contains(line))
        {
            added=true;
            break;
        }
    }

    for(const QString &line : before)
    {
        if(!after.contains(line))
        {
            removed=true;
            break;
        }
    }

    if(added && !removed)
        return StepAdd;

    if(!added && removed)
        return StepDelete;

    return StepMixed;
}


QSet<QString> GitHelper::smartDropSet(const QString &dir, const QString &file,
                                      const QStringList &fileCommits,
                                      const QSet<QString> &pinned,
                                      int keepNewest, int freshSize)
{
    Q_UNUSED(freshSize)

    const int count=fileCommits.size();

    // Кандидаты: все кроме пинов (genesis, свежая зона)
    QList<int> candidates;
    for(int i=0; i<count; ++i)
    {
        if(!pinned.contains(fileCommits.at(i).trimmed()))
            candidates.append(i);
    }

    if(candidates.isEmpty())
        return QSet<QString>();

    // Направления шагов по содержимому одним процессом.
    // Облом в любом месте: тупой отбор (все кандидаты под снос)
    QSet<QString> dropSet;
    QStringList blobRevs;
    for(int i=0; i<count; ++i)
        blobRevs << fileCommits.at(i).trimmed()+QStringLiteral(":")+file;

    const QList<QByteArray> blobs=readBlobs(dir, blobRevs);

    if(blobs.size()!=count)
    {
        for(int i : candidates)
            dropSet.insert(fileCommits.at(i).trimmed());

        return dropSet;
    }

    QList<QSet<QString>> lineSets;
    for(const QByteArray &blob : blobs)
    {
        QSet<QString> lines;
        for(const QByteArray &line : blob.split('\n'))
            lines.insert(QString::fromUtf8(line));

        lineSets.append(lines);
    }

    // Класс каждого шага между соседними версиями
    QList<StepDirection> stepClass;
    for(int i=0; i+1<count; ++i)
        stepClass.append(stepDirection(lineSets.at(i), lineSets.at(i+1)));

    // Границы серий: версии на стыке направлений хранятся
    // (точки разворота информативнее середины серий)
    QSet<int> protectedIdx;
    for(int i=1; i+1<count; ++i)
    {
        if(stepClass.at(i-1)!=stepClass.at(i))
            protectedIdx.insert(i);
    }

    // Фаза A: длинные серии (10+ шагов) прореживаются:
    // внутри хранится каждая 10-я версия
    {
        int runStart=0;

        for(int s=1; s<=stepClass.size(); ++s)
        {
            if(s<stepClass.size() && stepClass.at(s)==stepClass.at(runStart))
                continue;

            // Серия шагов [runStart..s): версии [runStart..s].
            // Внутри длинной хранится каждая 10-я, остальные сносимы
            if(s-runStart>=10)
            {
                for(int j=runStart+1; j<s; ++j)
                {
                    if((j-runStart)%10==0)
                        protectedIdx.insert(j);
                }
            }

            runStart=s;
        }
    }

    // Фаза B: сколько надо снести до бюджета, старейшими
    // внутренними (не границы, не пины)
    int need=count-keepNewest;

    for(int i=0; i<count && need>0; ++i)
    {
        if(pinned.contains(fileCommits.at(i).trimmed()))
            continue;

        if(protectedIdx.contains(i))
            continue;

        dropSet.insert(fileCommits.at(i).trimmed());
        --need;
    }

    // Последний рубеж: склеивать нечего (все точки разворота),
    // а бюджет превышен. Сносится старейшее кроме genesis,
    // иначе лимит недостижим в принципе
    if(dropSet.isEmpty() && need>0)
    {
        for(int i=1; i<count && need>0; ++i)
        {
            dropSet.insert(fileCommits.at(i).trimmed());
            --need;
        }
    }

    return dropSet;
}

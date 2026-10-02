#include "GitWrapper.h"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include "views/consoleEmulator/CommandRunner.h"


// Проверка наличия git в PATH
bool GitWrapper::isGitAvailable(void)
{
    QStringList arguments;
    arguments << "--version";

    int result=GitWrapper::runGit("", arguments);

    return (result==0);
}


// Инициализация репозитория в указанном каталоге
bool GitWrapper::initRepository(const QString &repoPath)
{
    QStringList arguments;
    arguments << "init";

    int result=GitWrapper::runGit(repoPath, arguments);

    if(result!=0)
    {
        qDebug() << "GitWrapper: git init failed in " << repoPath;
    }

    return (result==0);
}


// Установка локальной идентичности репозитория
bool GitWrapper::setLocalIdentity(const QString &repoPath,
                                  const QString &userName,
                                  const QString &userEmail)
{
    QStringList nameArguments;
    nameArguments << "config" << "user.name" << GitWrapper::shellQuote(userName);

    QStringList emailArguments;
    emailArguments << "config" << "user.email" << GitWrapper::shellQuote(userEmail);

    int nameResult=GitWrapper::runGit(repoPath, nameArguments);
    int emailResult=GitWrapper::runGit(repoPath, emailArguments);

    return (nameResult==0 && emailResult==0);
}


// Добавление всех изменений и создание коммита
bool GitWrapper::commitAll(const QString &repoPath,
                           const QString &commitMessage,
                           const QString &pathspec)
{
    QStringList addArguments;
    addArguments << "add" << "--all";
    if(!pathspec.isEmpty())
    {
        addArguments << "--" << pathspec;
    }
    int addResult=GitWrapper::runGit(repoPath, addArguments);
    if(addResult!=0)
    {
        return false;
    }

    QStringList commitArguments;
    commitArguments << "commit" << "--allow-empty" << "-m" << GitWrapper::shellQuote(commitMessage);
    int commitResult=GitWrapper::runGit(repoPath, commitArguments);

    if(commitResult!=0)
    {
        qDebug() << "GitWrapper: git commit failed in " << repoPath;
    }

    return (commitResult==0);
}


// Добавление всех изменений и создание коммита, только если область
// pathspec действительно изменилась
bool GitWrapper::commitIfChanged(const QString &repoPath,
                                 const QString &commitMessage,
                                 const QString &pathspec,
                                 QString *commitHash)
{
    QStringList addArguments;
    addArguments << "add" << "--all";
    if(!pathspec.isEmpty())
    {
        addArguments << "--" << pathspec;
    }
    if(GitWrapper::runGit(repoPath, addArguments)!=0)
    {
        return false;
    }

    // Проверка наличия staged-изменений области (0 — изменений нет)
    QStringList diffArguments;
    diffArguments << "diff" << "--cached" << "--quiet";
    if(!pathspec.isEmpty())
    {
        diffArguments << "--" << pathspec;
    }
    const int diffExit=GitWrapper::runGit(repoPath, diffArguments);

    if(diffExit==0)
    {
        // Изменений нет — снимаем застейдженное, коммит не создаём.
        // reset тихий (-q): его отчёт «Unstaged changes after reset»
        // иначе сыплется в консоль пользователя при каждом пустом снимке
        GitWrapper::runGit(repoPath, QStringList() << "reset" << "-q");

        if(commitHash)
            commitHash->clear();
        return false;
    }

    if(diffExit!=1)
    {
        // Код, отличный от 0/1 — ошибка выполнения
        if(commitHash)
            commitHash->clear();
        return false;
    }

    QStringList commitArguments;
    commitArguments << "commit" << "--allow-empty" << "-m" << GitWrapper::shellQuote(commitMessage);
    if(GitWrapper::runGit(repoPath, commitArguments)!=0)
    {
        qDebug() << "GitWrapper: git commit failed in " << repoPath;
        return false;
    }

    if(commitHash)
        *commitHash=GitWrapper::getHead(repoPath);

    return true;
}


// Восстановление файлов области pathspec из указанной ссылки журнала
bool GitWrapper::checkoutFromRef(const QString &repoPath,
                                 const QString &ref,
                                 const QString &pathspec,
                                 QString *errorMessage)
{
    QStringList arguments;
    arguments << "checkout" << "-f" << GitWrapper::shellQuote(ref) << "--"
              << GitWrapper::shellQuote(pathspec);

    const int result=GitWrapper::runGit(repoPath, arguments);

    if(result!=0 && errorMessage)
    {
        *errorMessage=QStringLiteral("git checkout ")+ref+QStringLiteral(" -- ")+pathspec
                       +QStringLiteral(" failed (exit ")+QString::number(result)+QStringLiteral(")");
    }

    return (result==0);
}


// Список файлов в ссылке журнала
QStringList GitWrapper::listFilesInRef(const QString &repoPath,
                                       const QString &ref,
                                       const QString &pathspec)
{
    QStringList arguments;
    arguments << "ls-tree" << "-r" << "--name-only" << GitWrapper::shellQuote(ref);
    if(!pathspec.isEmpty())
    {
        arguments << "--" << GitWrapper::shellQuote(pathspec);
    }

    QString output;
    GitWrapper::runGit(repoPath, arguments, &output);

    QStringList result;
    const QStringList lines=output.split("\n", Qt::SkipEmptyParts);
    for(const QString &line : lines)
        result << line.trimmed();

    return result;
}


// Имя текущей ветки журнала
QString GitWrapper::currentBranchName(const QString &repoPath)
{
    QStringList arguments;
    arguments << "rev-parse" << "--abbrev-ref" << "HEAD";

    QString output;
    const int result=GitWrapper::runGit(repoPath, arguments, &output);

    if(result!=0 || output.contains("HEAD"))
    {
        return QString();
    }

    return output.trimmed();
}


// «Архивирование истории»: новое корневое состояние, старые коммиты удаляются
bool GitWrapper::archiveHistory(const QString &repoPath,
                                const QString &commitMessage,
                                QString *errorMessage)
{
    // Требуется репозиторий минимум с одним коммитом
    if(GitWrapper::getHead(repoPath).isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Journal repository is empty");
        return false;
    }

    const QString originalBranch=GitWrapper::currentBranchName(repoPath);
    if(originalBranch.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Cannot detect journal branch name");
        return false;
    }

    // Временная ветка с пустым корнем (рабочий каталог не трогается)
    const QString tempBranch=QStringLiteral("snapshot_arch_")+QString::number(QDateTime::currentMSecsSinceEpoch());
    if(GitWrapper::runGit(repoPath, QStringList() << "checkout" << "--orphan" << tempBranch)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git checkout --orphan failed");
        return false;
    }

    // Всё текущее состояние добавляется как единое корневое состояние
    if(GitWrapper::runGit(repoPath, QStringList() << "add" << "--all")!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git add --all failed");
        return false;
    }

    if(GitWrapper::runGit(repoPath, QStringList() << "commit" << "--allow-empty" << "-m"
                             << GitWrapper::shellQuote(commitMessage))!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git commit failed during archive");
        return false;
    }

    // Старая ветка получает новое корневое состояние; прежняя история
    // становится недостижимой и удаляется из reflog + уборкой объектов
    if(GitWrapper::runGit(repoPath, QStringList() << "branch" << "-M" << originalBranch)!=0)
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git branch -M failed during archive");
        return false;
    }

    GitWrapper::runGit(repoPath, QStringList() << "reflog" << "expire" << "--expire=now" << "--all");
    GitWrapper::runGit(repoPath, QStringList() << "gc" << "--prune=now");

    return true;
}


// Корень единого журнала (каталог обмена) для каталога публикации
QString GitWrapper::findRepositoryRoot(const QString &publicationDir)
{
    QDir dir(publicationDir);

    // Подъём до ближайшего предка, содержащего каталог sync/
    for(int i=0; i<8; ++i)
    {
        if(QFileInfo::exists(dir.absolutePath()+"/sync"))
            return dir.absolutePath();

        if(!dir.cdUp())
            break;
    }

    return QString();
}


// Текущий HEAD репозитория
QString GitWrapper::getHead(const QString &repoPath)
{
    QStringList arguments;
    arguments << "rev-parse" << "HEAD";

    QString output;
    int result=GitWrapper::runGit(repoPath, arguments, &output);

    if(result!=0)
    {
        return QString();
    }

    return output.trimmed();
}


// Список коммитов
QStringList GitWrapper::getLogOneline(const QString &repoPath, const QString &range)
{
    QStringList arguments;
    arguments << "log" << "--oneline";

    if(!range.isEmpty())
    {
        arguments << range;
    }

    QString output;
    int result=GitWrapper::runGit(repoPath, arguments, &output);

    if(result!=0)
    {
        return QStringList();
    }

    return output.split("\n", Qt::SkipEmptyParts);
}


// Содержимое файла в указанной ссылке репозитория
QString GitWrapper::getShow(const QString &repoPath, const QString &ref, const QString &pathInRepository)
{
    QStringList arguments;
    arguments << "show" << (ref+":"+pathInRepository);

    QString output;
    int result=GitWrapper::runGit(repoPath, arguments, &output);

    if(result!=0)
    {
        return QString();
    }

    return output;
}


// Выполнение git-команды в каталоге репозитория
int GitWrapper::runGit(const QString &repoPath,
                       const QStringList &arguments,
                       QString *output)
{
    CommandRunner runner;

    // Команда собирается целиком и передается CommandRunner,
    // который запускает её через shell.
    // Путь с пробелами и спецсимволами экранируется кавычками
    QStringList fullArguments;
    if(!repoPath.isEmpty())
    {
        fullArguments << "-C" << GitWrapper::shellQuote(repoPath);
    }
    fullArguments << arguments;

    QString command="git "+fullArguments.join(' ');

    runner.setCommand(command);
    runner.setWindowTitle("git");
    runner.setMessageText("git");

    if(output)
    {
        *output=runner.runSimpleAndGetOutput();
        return 0;
    }

    // Без захвата вывода — тихо: внутренности git (commit/checkout/reset)
    // не должны попадать в консоль пользователя
    return runner.runSimpleQuiet();
}


// Заключение аргумента в кавычки для безопасной передачи в shell.
// Используются одинарные кавычки: команда целиком уже обёрнута в двойные
// (sh -c "..."), поэтому двойные кавычки внутри рвали бы строку и сообщения
// git обрезались по первому пробелу. Внутри одинарных кавычек shell ничего
// не интерпретирует; сам апостроф экранируется как '\''
QString GitWrapper::shellQuote(const QString &value)
{
    QString result=value;
    result.replace("'", "'\\''");
    return "'"+result+"'";
}
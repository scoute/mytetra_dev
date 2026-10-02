#ifndef _GITWRAPPER_H_
#define _GITWRAPPER_H_

#include <QString>
#include <QStringList>

// Модуль вызова внешнего git через CommandRunner.
// Все операции выполняются синхронно (с ожиданием завершения),
// консоль не показывается.
//
// Новый макет (ADR-015): единый журнал в каталоге обмена — shared/.git,
// в котором хранятся все публикации дерева shared/sync/. Устаревшие
// публикации (с per-branch .git внутри) продолжают поддерживаться.
// Git не знает формат данных MyTetra: семантическую разность
// строит отдельный модуль (BranchDiffEngine)

class GitWrapper
{
public:
    GitWrapper() = delete;

    // Проверка наличия git в PATH
    static bool isGitAvailable(void);

    // Инициализация репозитория в указанном каталоге (git init)
    static bool initRepository(const QString &repoPath);

    // Установка локальной идентичности репозитория (git config user.name/email)
    static bool setLocalIdentity(const QString &repoPath,
                                 const QString &userName,
                                 const QString &userEmail);

    // Добавление всех изменений и создание коммита
    // (git add --all [-- pathspec]; git commit -m "<message>")
    // pathspec ограничивает область коммита (например "sync" для единого
    // журнала); пустой pathspec — весь рабочий каталог репозитория
    static bool commitAll(const QString &repoPath, const QString &commitMessage,
                          const QString &pathspec=QString());

    // Коммит только при фактическом изменении области pathspec
    // (git add --all [-- pathspec]; git diff --cached --quiet).
    // Возвращает true, если было что коммитить; commitHash заполняется
    // хешем нового коммита, иначе пустой строкой. Рассиндиксирование
    // при отсутствии изменений не оставляет область staged
    static bool commitIfChanged(const QString &repoPath,
                                const QString &commitMessage,
                                const QString &pathspec=QString(),
                                QString *commitHash=nullptr);

    // Восстановление файлов области pathspec из указанной ссылки журнала
    // (git checkout -f <ref> -- <pathspec>). При пустых/утерянных каталогах
    // занимает недостающие файлы из истории
    static bool checkoutFromRef(const QString &repoPath,
                                const QString &ref,
                                const QString &pathspec,
                                QString *errorMessage=nullptr);

    // Список файлов в ссылке журнала (git ls-tree -r --name-only <ref> [-- pathspec])
    static QStringList listFilesInRef(const QString &repoPath,
                                      const QString &ref,
                                      const QString &pathspec=QString());

    // Имя текущей ветки журнала (git rev-parse --abbrev-ref HEAD)
    static QString currentBranchName(const QString &repoPath);

    // «Архивирование истории»: свежее корневое состояние текущего рабочего
    // каталога (git checkout --orphan + commit), старое имя ветки сохраняется,
    // прежние коммиты удаляются (reflog expire --all; gc --prune=now).
    // Требует репозиторий минимум с одним коммитом
    static bool archiveHistory(const QString &repoPath,
                               const QString &commitMessage,
                               QString *errorMessage=nullptr);

    // Корень единого журнала для каталога публикации: ближайший предок,
    // содержащий каталог sync/ (это сам каталог обмена shared/).
    // Пустая строка, если такой предок не найден (устаревший per-branch макет)
    static QString findRepositoryRoot(const QString &publicationDir);

    // Текущий HEAD репозитория (git rev-parse HEAD).
    // Возвращает пустую строку, если репозиторий пуст или произошла ошибка
    static QString getHead(const QString &repoPath);

    // Список коммитов (git log --oneline <range>).
    // range в формате "baseline..HEAD";
    // при пустом range выводятся все коммиты
    static QStringList getLogOneline(const QString &repoPath, const QString &range="");

    // Содержимое файла в указанной ссылке репозитория
    // (git show <ref>:<pathInRepository>)
    static QString getShow(const QString &repoPath, const QString &ref, const QString &pathInRepository);

private:
    // Выполнение git-команды в каталоге репозитория.
    // Если указатель output не нулевой — возвращает выведенный текст,
    // иначе код возврата
    static int runGit(const QString &repoPath,
                      const QStringList &arguments,
                      QString *output=nullptr);

    // Заключение аргумента в кавычки для безопасной передачи в shell
    static QString shellQuote(const QString &value);
};

#endif // _GITWRAPPER_H_
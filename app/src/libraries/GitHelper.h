#ifndef _GITHELPER_H_
#define _GITHELPER_H_

#include <QString>
#include <QStringList>
#include <QList>

// Центральная обертка над внешним git для тяжелой линии.
// Все вызовы идут через один run с таймаутом, без шелла.
// Кэшируется только факт наличия и версия: репозиторий
// меняется извне, состояние не кэшируется

class GitHelper
{
public:

    // Есть ли git в PATH. Результат кэшируется на процесс
    static bool isAvailable(void);

    // Строка версии ("2.43.0"), пусто если git нет
    static QString version(void);

    // Запуск git с аргументами в каталоге. Возвращает код,
    // вывод забирается по указателям. Таймаут 60с по умолчанию
    static int run(const QString &workDir, const QStringList &args,
                   QString *stdoutText=nullptr, QString *stderrText=nullptr,
                   int timeoutMs=60000);

    // Это репозиторий (есть .git выше по дереву)
    static bool isRepo(const QString &dir);

    // Короткий статус фарфором, пусто если чисто или не репо
    static QString statusPorcelain(const QString &dir);

    // Unified diff рабочих изменений (не в индексе)
    static QString diffWorkdir(const QString &dir, const QString &path=QString());

    // Unified diff закоммиченного: rev против рабочих файлов
    static QString diffAgainst(const QString &dir, const QString &rev,
                              const QString &path=QString());

    // Однострочный журнал: "хеш тема". limit<=0 значит без лимита
    static QStringList logOneline(const QString &dir, int limit=0,
                                  const QString &path=QString());

    // Подробный журнал для истории: ревизия, дата, тема
    struct LogEntry
    {
        QString rev;
        QString date;
        QString subject;
    };

    static QList<LogEntry> logDetailed(const QString &dir, int limit=0,
                                       const QString &path=QString());

    // Содержимое файла в ревизии, пусто если нет
    static QString showFile(const QString &dir, const QString &rev,
                            const QString &path);

    // HEAD ревизии, пусто если коммитов нет
    static QString headRev(const QString &dir);

    // Инициализация пустого репозитория
    static bool initRepo(const QString &dir);

    // Добавить пути в индекс
    static bool addPaths(const QString &dir, const QStringList &paths);

    // Закоммитить индекс с сообщением
    static bool commitStaged(const QString &dir, const QString &message);

    // Локальная подпись репозитория (не трогает глобальный конфит git)
    static bool setLocalIdentity(const QString &dir, const QString &name,
                                 const QString &email);

    // Закоммитить путь если он менялся. Хеш в hash (пусто при неудаче).
    // Без изменений коммит не делается, возвращается истина
    static bool commitPathIfChanged(const QString &dir, const QString &message,
                                    const QString &relPath, QString *hash=nullptr);

    // Закоммитить всё если есть изменения. Без изменений истина
    static bool commitAllIfChanged(const QString &dir, const QString &message,
                                   QString *hash=nullptr);

    // Корень репозитория вверх по дереву (по .git), пусто если нет.
    // Для diff/детекта подписок: единый журнал shared/.git
    static QString findRepositoryRoot(const QString &dir);

    // Файлы ссылки журнала (ls-tree), только под путём при заданном
    static QStringList listFilesInRef(const QString &repo, const QString &ref,
                                      const QString &pathspec=QString());

    // checkout пути из ссылки (восстановление). Текст ошибки опционально
    static bool checkoutFromRef(const QString &repo, const QString &ref,
                                const QString &pathspec,
                                QString *errorMessage=nullptr);

    // Однострочный журнал всего репозитория
    static QStringList getLogOneline(const QString &repo);

    // Архивация истории: текущее состояние новым корнем, старое
    // чистится. Текст ошибки опционально
    static bool archiveHistory(const QString &repo, const QString &message,
                               QString *errorMessage=nullptr);

    // Усечь историю файла до целевой ревизии: все коммиты новее
    // targetRev, касающиеся file, уничтожаются, остальные
    // (других файлов) переигрываются поверх. Рабочее дерево
    // должно быть чистым, иначе отказ без изменений.
    // Текст ошибки опционально
    static bool truncateFileHistory(const QString &dir, const QString &file,
                                    const QString &targetRev,
                                    QString *errorMessage=nullptr);
};

#endif /* _GITHELPER_H_ */

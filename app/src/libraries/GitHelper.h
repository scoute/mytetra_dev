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
    // вывод забирается по указателям. Таймаут 60с по умолчанию.
    // stdinData (если задан) пишется в stdin процесса
    static int run(const QString &workDir, const QStringList &args,
                   QString *stdoutText=nullptr, QString *stderrText=nullptr,
                   int timeoutMs=60000, const QByteArray *stdinData=nullptr);

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

    // Усечь историю файла до целевой ревизии: все коммиты новее
    // targetRev, касающиеся file, уничтожаются, остальные
    // (других файлов) переигрываются поверх. Рабочее дерево
    // должно быть чистым, иначе отказ без изменений.
    // Текст ошибки опционально
    static bool truncateFileHistory(const QString &dir, const QString &file,
                                    const QString &targetRev,
                                    QString *errorMessage=nullptr);

    // Умное сжатие истории файла снизу до бюджета keepNewest.
    // Направление шагов (добавление/удаление/смешанное) считается
    // по множествам строк; соседние однонаправленные шаги склеиваются
    // (добавление с добавлением), точки разворота хранятся всегда.
    // Пины: genesis (якорь отката) и свежая зона. keepNewest<2 отказ.
    // При недоступности содержимого деградирует к тупому отбору
    // (genesis плюс свежие). Текст ошибки опционально
    static bool squashFileHistory(const QString &dir, const QString &file,
                                  int keepNewest,
                                  QString *errorMessage=nullptr);

    // Содержимое blob-объектов одним процессом (cat-file --batch).
    // Порядок как в revs. Пусто при любой ошибке
    static QList<QByteArray> readBlobs(const QString &dir,
                                       const QStringList &revs);

    // Умный отбор сносимых коммитов файла (от старых к новым):
    // пины и границы однонаправленных серий хранятся, длинные серии
    // прореживаются по каждой 10-й, остаток добирается старейшими
    // внутренними до бюджета. Пусто если сносить нечего
    static QSet<QString> smartDropSet(const QString &dir, const QString &file,
                                      const QStringList &fileCommits,
                                      const QSet<QString> &pinned,
                                      int keepNewest, int freshSize);

    // Направление шага между соседними версиями по множествам строк:
    // только добавления, только удаления, иначе смешанное.
    // Эвристика для склейки однонаправленных шагов, turning points
    // ею не размываются (сомнительное классифицируется смешанным)
    enum StepDirection
    {
        StepAdd,        // Строки только добавились
        StepDelete,     // Строки только удалились
        StepMixed       // Правки внутри строк и прочее
    };

    static StepDirection stepDirection(const QSet<QString> &before,
                                       const QSet<QString> &after);
};

#endif /* _GITHELPER_H_ */

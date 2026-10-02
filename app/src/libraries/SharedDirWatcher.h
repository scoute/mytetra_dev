#ifndef _SHAREDDIRWATCHER_H_
#define _SHAREDDIRWATCHER_H_

#include <QObject>
#include <QSet>
#include <QString>

class QFileSystemWatcher;
class QTimer;

// Объект слежения за каталогом обмена shared/.
// Следит за появлением, изменением и исчезновением публикаций
// (подкаталогов) в каталоге обмена.
//
// Основной механизм — QFileSystemWatcher на область данных sync/,
// страховочный механизм — периодический таймер, сравнивающий
// сигнатуру содержимого каталога (защита от пропущенных событий
// файловой системы и от изменений на вложенных уровнях).
//
// Локальный журнал (7.3): при устоявшемся изменении sync/ делается
// снимок области sync/ в единый shared/.git (guard от пустых коммитов).
// Снимок записывается только когда в sync/ есть хотя бы одна полностью
// доехавшая публикация — журнал хранит только целостные состояния и
// служит точкой восстановления. Также поддерживаются восстановление
// sync/ из журнала и «архивирование истории» (обрезка старых снимков)

class SharedDirWatcher : public QObject
{
    Q_OBJECT

public:
    SharedDirWatcher(QObject *parent=nullptr);
    ~SharedDirWatcher();

    // Запуск слежения за фактическим каталогом обмена
    void start(void);
    void stop(void);

    // Повторное чтение настроек и обновление наблюдаемого каталога.
    // Вызывается при изменении настроек каталога обмена
    void updateWatchedPath(void);

    // Фактический наблюдаемый каталог обмена
    QString getWatchedPath(void);

    // Признак, что доступно восстановление sync/ из журнала:
    // единый shared/.git существует с коммитами, а хотя бы одна публикация
    // области sync/, присутствующая в журнале, на диске отсутствует
    // (либо область sync/ отсутствует целиком). Проверка по наличию, а не
    // по целостности: публикации до Фазы 7.2 не имеют manifest.json
    bool isRecoveryAvailable(void) const;

    // Признак, что доступно восстановление sync/ из local/ (без git):
    // авторитет local/ содержит публикацию, которой нет в sync/ либо чья
    // версия в sync/ неполна
    bool isLocalRestoreAvailable(void) const;

    // Восстановление sync/ из local-авторитета (перепромоут, без git).
    // Возвращает число восстановленных публикаций
    int restoreSyncFromLocal(QString *errorMessage=nullptr);

    // Восстановление sync/ из журнала (HEAD единого shared/.git).
    // restoreOnlyMissing=true — только публикации, которых нет на диске;
    // false — полное восстановление области sync/ (checkout HEAD -- sync)
    bool restoreSyncFromJournal(bool restoreOnlyMissing,
                                QString *errorMessage=nullptr);

    // «Архивирование истории»: текущее состояние sync/ становится новым
    // корнем журнала, старые снимки удаляются (требует согласия пользователя)
    bool archiveJournalHistory(QString *errorMessage=nullptr);

    // Число снимков в журнале (0 — журнала нет или git недоступен)
    int journalSnapshotCount(void) const;

    // Размер каталога единого журнала shared/.git в байтах
    // (0 — журнала нет). Для предупреждения о разрастании истории
    quint64 journalDirSizeBytes(void) const;

    // Немедленный снимок sync/ в журнал после легитимного изменения
    // приложением (публикация/обновление/отзыв публикации). Журнал сразу
    // отражает новое легитимное состояние, чтобы «восстановление из журнала»
    // не предлагалось при осознанном отзыве публикации
    void requestImmediateSnapshot(void);

signals:

    // Сигнал о том, что каталог обмена изменился
    // (появилась, изменилась или исчезла публикация)
    void sharedDirChanged(void);

private slots:

    // Событие изменения каталога от QFileSystemWatcher
    void onDirectoryChanged(const QString &path);

    // Страховочная проверка по таймеру
    void onTimerTick(void);

    // Отложенный снимок: вызов после устаканивания изменений
    void onSnapshotDebounce(void);

private:

    // Откладывание снимка журнала (после очередного изменения)
    void scheduleSnapshot(void);

    // Выполнение снимка области sync/ в единый shared/.git.
    // allowEmpty=true — разрешает зафиксировать переход sync/ к пустой
    // области (легитимный отзыв последней публикации); автоматические
    // снимки пустые состояния не фиксируют
    void commitJournalSnapshot(bool allowEmpty=false);

    // Каталог обмена shared/ (родитель наблюдаемой области sync/)
    QString sharedDirPath(void) const;

    // Корни публикаций области sync/ из среза журнала
    QSet<QString> journalPublicationRoots(const QString &repo, const QString &ref) const;

    // Корни публикаций области sync/ на диске
    QSet<QString> diskPublicationRoots(const QString &repo) const;

    // Корень единого журнала shared/.git, если он существует
    QString journalRepoPath(void) const;

    // Настройка наблюдения за указанным каталогом
    void watchPath(const QString &path);

    // Создание каталога обмена, если его нет
    void ensureSharedDirExists(const QString &path);

    // Строка-сигнатура содержимого каталога обмена
    // (для страховочного контроля изменений)
    QString calculateDirSignature(void) const;

    QFileSystemWatcher *fileWatcher;
    QTimer *timer;
    QTimer *snapshotDebounceTimer;

    QString watchedPath;
    QString lastSignature;

    // Сигнатура, с которой сверяется устойчивость перед снимком
    QString snapshotStableSignature;
};

#endif // _SHAREDDIRWATCHER_H_
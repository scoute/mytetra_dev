#include "SharedDirWatcher.h"

#include <QFileSystemWatcher>
#include <QTimer>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

#include "libraries/BranchPublisher.h"
#include "libraries/GitWrapper.h"
#include "models/teamProfile/TeamProfile.h"


extern TeamProfile teamProfile;


// Интервал страховочной проверки содержимого каталога обмена
const int SharedDirWatcherTimerPeriodMs=5000;

// Интервал отложенного снимка журнала (после последнего изменения)
const int SnapshotDebounceMs=6000;


SharedDirWatcher::SharedDirWatcher(QObject *parent) : QObject(parent)
{
    fileWatcher=new QFileSystemWatcher(this);
    connect(fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &SharedDirWatcher::onDirectoryChanged);

    timer=new QTimer(this);
    timer->setInterval(SharedDirWatcherTimerPeriodMs);
    connect(timer, &QTimer::timeout, this, &SharedDirWatcher::onTimerTick);

    snapshotDebounceTimer=new QTimer(this);
    snapshotDebounceTimer->setSingleShot(true);
    snapshotDebounceTimer->setInterval(SnapshotDebounceMs);
    connect(snapshotDebounceTimer, &QTimer::timeout,
            this, &SharedDirWatcher::onSnapshotDebounce);
}


SharedDirWatcher::~SharedDirWatcher()
{

}


// Запуск слежения за каталогом обмена
void SharedDirWatcher::start(void)
{
    this->updateWatchedPath();

    if(!timer->isActive())
    {
        timer->start();
    }
}


void SharedDirWatcher::stop(void)
{
    timer->stop();
    snapshotDebounceTimer->stop();
    fileWatcher->removePaths(fileWatcher->directories());
    watchedPath.clear();
    lastSignature.clear();
    snapshotStableSignature.clear();
}


// Повторное чтение настроек и обновление наблюдаемого каталога
void SharedDirWatcher::updateWatchedPath(void)
{
    QString path=teamProfile.getSharedDir();

    if(path.isEmpty())
    {
        return;
    }

    this->ensureSharedDirExists(path);

    // Авторитет владельца и сборка — вне опасной зоны (ADR-016)
    this->ensureSharedDirExists(path+"/local");
    this->ensureSharedDirExists(path+"/.tmp");

    // Наблюдается область данных sync/ (единственная папка Syncthing;
    // единый журнал shared/.git и устаревшие публикации вне области данных)
    QString syncPath=path+"/sync";
    this->ensureSharedDirExists(syncPath);
    BranchPublisher::ensureSyncIgnore(syncPath);
    this->watchPath(syncPath);
}


QString SharedDirWatcher::getWatchedPath(void)
{
    return watchedPath;
}


void SharedDirWatcher::onDirectoryChanged(const QString &path)
{
    qDebug() << "SharedDirWatcher: directory changed: " << path;

    this->lastSignature=this->calculateDirSignature();

    this->scheduleSnapshot();

    emit sharedDirChanged();
}


// Страховочная проверка: если таймер обнаружил изменение,
// которое могло быть пропущено наблюдателем файловой системы
void SharedDirWatcher::onTimerTick(void)
{
    if(watchedPath.isEmpty())
    {
        return;
    }

    QString signature=this->calculateDirSignature();

    if(signature!=lastSignature)
    {
        qDebug() << "SharedDirWatcher: timer detected change";

        lastSignature=signature;

        this->scheduleSnapshot();

        emit sharedDirChanged();
    }
}


// Настройка наблюдения за указанным каталогом
void SharedDirWatcher::watchPath(const QString &path)
{
    if(watchedPath==path)
    {
        return;
    }

    // Убирается наблюдение за предыдущим каталогом
    if(!watchedPath.isEmpty())
    {
        fileWatcher->removePath(watchedPath);
    }

    watchedPath=path;

    // Добавляется наблюдение за новым каталогом.
    // Если наблюдатель не смог добавить каталог (например,
    // каталог ещё не создан), настройка продублируется по таймеру
    fileWatcher->addPath(watchedPath);

    // Формируется эталонная сигнатура содержимого
    lastSignature=this->calculateDirSignature();

    qDebug() << "SharedDirWatcher: watching path: " << watchedPath;
}


// Создание каталога обмена, если его нет
void SharedDirWatcher::ensureSharedDirExists(const QString &path)
{
    QDir dir;

    if(!dir.exists(path))
    {
        dir.mkpath(path);
    }
}


// Строка-сигнатура содержимого каталога обмена:
// имена подкаталогов (публикаций) с меткой времени изменения,
// для страховочного контроля изменений
QString SharedDirWatcher::calculateDirSignature(void) const
{
    QString signature;

    QDir dir(watchedPath);
    QFileInfoList entries=dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    for(const QFileInfo &entry : entries)
    {
        signature+=entry.fileName()+"|"+entry.lastModified().toString(Qt::ISODate)+"\n";
    }

    return signature;
}


// Каталог обмена shared/ (родитель области данных sync/)
QString SharedDirWatcher::sharedDirPath(void) const
{
    if(watchedPath.isEmpty())
        return QString();

    QDir dir(watchedPath);
    dir.cdUp();

    return dir.absolutePath();
}


// Корень единого журнала shared/.git (пусто, если журнал не создан)
QString SharedDirWatcher::journalRepoPath(void) const
{
    const QString sharedDir=this->sharedDirPath();
    if(sharedDir.isEmpty())
        return QString();

    if(!QFileInfo::exists(sharedDir+"/.git"))
        return QString();

    return sharedDir;
}


// Корни публикаций области sync/ из среза журнала (sync/<owner>/<branch>).
// Корнем считается только каталог с meta.json в срезе: файлы корня sync/
// (sync/.stignore, sync/README.md и т. п.) публикациями не являются.
// Иначе каждый такой файл давал фантомный корень, которого нет на диске
// в виде публикации, — предложение восстановления не исчезало никогда,
// а само восстановление ничего не чинило
QSet<QString> SharedDirWatcher::journalPublicationRoots(const QString &repo,
                                                        const QString &ref) const
{
    QSet<QString> roots;
    const QStringList files=GitWrapper::listFilesInRef(repo, ref, QStringLiteral("sync"));

    // Каталоги, содержащие meta.json в этом срезе журнала
    QSet<QString> withMeta;
    for(const QString &filePath : files)
    {
        if(!filePath.endsWith(QStringLiteral("/meta.json")))
            continue;
        withMeta.insert(filePath.left(filePath.length()-QStringLiteral("/meta.json").length()));
    }

    for(const QString &filePath : files)
    {
        const QStringList parts=filePath.split('/');
        if(parts.size()<2)
            continue;

        // sync/<owner>/<branch>/...  (новый макет) или sync/<branch>/... (плоская схема)
        QString candidate;
        if(parts.size()>=3)
            candidate=QStringLiteral("sync/")+parts.at(1)+QStringLiteral("/")+parts.at(2);
        else
            candidate=QStringLiteral("sync/")+parts.at(1);

        if(withMeta.contains(candidate))
            roots.insert(candidate);
    }
    return roots;
}


// Корни публикаций области sync/ на диске (относительно repo)
QSet<QString> SharedDirWatcher::diskPublicationRoots(const QString &repo) const
{
    QSet<QString> roots;
    const QString syncPrefix=repo+QStringLiteral("/sync/");
    const QList<PublicationMeta> pubs=BranchPublisher::listPublications(repo);
    for(const PublicationMeta &m : pubs)
    {
        if(m.dirPath.startsWith(syncPrefix))
            roots.insert(QDir(repo).relativeFilePath(m.dirPath));
    }
    return roots;
}


// Откладывание снимка журнала после очередного изменения области sync/
void SharedDirWatcher::scheduleSnapshot(void)
{
    // Отсчёт устойчивости начинается заново (данные Syncthing могли
    // прийти частями)
    snapshotStableSignature.clear();
    snapshotDebounceTimer->start();
}


// Немедленный снимок sync/ в журнал после легитимного изменения
// приложением (публикация/обновление/отзыв публикации). Не ждём debounce:
// приложение само записало данные, пришедших «частями» состояний нет.
// Отзыв последней публикации разрешается зафиксировать (allowEmpty=true),
// чтобы предложение восстановления не всплывало при осознанном отзыве
void SharedDirWatcher::requestImmediateSnapshot(void)
{
    snapshotDebounceTimer->stop();
    this->commitJournalSnapshot(true);
}


// По истечении debounce: снимок выполняется после двух подряд одинаковых
// сигнатур (изменения устаканились)
void SharedDirWatcher::onSnapshotDebounce(void)
{
    if(watchedPath.isEmpty())
        return;

    const QString signature=this->calculateDirSignature();

    if(snapshotStableSignature.isEmpty() || snapshotStableSignature!=signature)
    {
        snapshotStableSignature=signature;
        snapshotDebounceTimer->start();
        return;
    }

    snapshotStableSignature.clear();
    this->commitJournalSnapshot();
}


// Снимок области sync/ в единый shared/.git (guard от пустых коммитов).
// Автоматический снимок (allowEmpty=false) пишется только при наличии хотя
// бы одной целостной публикации: журнал хранит последние известные
// состояния, на которые можно вернуться. Немедленный снимок (allowEmpty
// =true) после легитимного действия приложения может зафиксировать и
// переход sync/ к пустой области
void SharedDirWatcher::commitJournalSnapshot(bool allowEmpty)
{
    const QString repo=this->journalRepoPath();
    if(repo.isEmpty())
        return;

    if(!GitWrapper::isGitAvailable())
        return;

    // Все публикации области данных должны быть целостными — частично
    // доехавшие состояния (Syncthing) в журнал не попадают. Область данных —
    // только sync/; устаревшие per-branch публикации в журнал не попадают
    const QString syncPrefix=repo+QStringLiteral("/sync/");

    QList<PublicationMeta> syncPublications;
    const QList<PublicationMeta> publications=BranchPublisher::listPublications(repo);
    for(const PublicationMeta &meta : publications)
    {
        if(meta.dirPath.startsWith(syncPrefix))
            syncPublications.append(meta);
    }

    if(syncPublications.isEmpty() && !allowEmpty)
        return;

    for(const PublicationMeta &meta : syncPublications)
    {
        if(!BranchPublisher::publicationVersionComplete(meta.dirPath, meta.publishVersion))
            return;
    }

    const QString message=QStringLiteral("Снимок sync ")+QDateTime::currentDateTime().toString(Qt::ISODate);

    QString commitHash;
    if(GitWrapper::commitIfChanged(repo, message, QStringLiteral("sync"), &commitHash) && !commitHash.isEmpty())
        qDebug() << "SharedDirWatcher: journal snapshot" << commitHash.left(8);
}


// Доступно ли восстановление sync/ из local/ (без git)
bool SharedDirWatcher::isLocalRestoreAvailable(void) const
{
    const QString sharedDir=this->sharedDirPath();
    if(sharedDir.isEmpty())
        return false;

    const QList<PublicationMeta> localPubs=BranchPublisher::listLocalPublications(sharedDir);
    if(localPubs.isEmpty())
        return false;

    for(const PublicationMeta &meta : localPubs)
    {
        // sync-зеркало: local/<rel> -> sync/<rel>
        const QString localPrefix=sharedDir+QStringLiteral("/local/");
        if(!meta.dirPath.startsWith(localPrefix))
            continue;
        const QString syncPubDir=sharedDir+QStringLiteral("/sync/")
                                 +meta.dirPath.mid(localPrefix.length());

        if(!QFileInfo::exists(syncPubDir+"/meta.json"))
            return true;

        const int syncVersion=BranchPublisher::readPublishVersion(syncPubDir);
        if(syncVersion!=meta.publishVersion)
            return true;

        if(!BranchPublisher::publicationVersionComplete(syncPubDir, meta.publishVersion))
            return true;
    }

    return false;
}


// Восстановление sync/ из local-авторитета (перепромоут, без git)
int SharedDirWatcher::restoreSyncFromLocal(QString *errorMessage)
{
    const QString sharedDir=this->sharedDirPath();
    if(sharedDir.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Shared directory is not defined");
        return 0;
    }

    const QList<PublicationMeta> localPubs=BranchPublisher::listLocalPublications(sharedDir);

    int restoredCount=0;
    const QString localPrefix=sharedDir+QStringLiteral("/local/");
    for(const PublicationMeta &meta : localPubs)
    {
        if(!meta.dirPath.startsWith(localPrefix))
            continue;
        const QString syncPubDir=sharedDir+QStringLiteral("/sync/")
                                 +meta.dirPath.mid(localPrefix.length());

        bool needRestore=false;
        if(!QFileInfo::exists(syncPubDir+"/meta.json"))
            needRestore=true;
        else if(BranchPublisher::readPublishVersion(syncPubDir)!=meta.publishVersion)
            needRestore=true;
        else if(!BranchPublisher::publicationVersionComplete(syncPubDir, meta.publishVersion))
            needRestore=true;

        if(!needRestore)
            continue;

        QString promoteError;
        if(!BranchPublisher::promoteToSync(meta.dirPath, syncPubDir, &promoteError))
        {
            if(errorMessage)
                *errorMessage=promoteError;
            continue;
        }

        restoredCount++;
    }

    if(restoredCount>0)
    {
        lastSignature=this->calculateDirSignature();
        this->scheduleSnapshot();
        emit sharedDirChanged();
    }

    return restoredCount;
}


// Доступно ли восстановление sync/ из журнала.
// Ориентир — НАЛИЧИЕ публикаций (а не их целостность): публикации из журнала
// (HEAD shared/.git), которых нет на диске, предлагается восстановить.
// Полноту (manifest.json) здесь не проверяем: публикации старого макета
// (до Фазы 7.2) манифестов не имеют и вечно считались бы «неполными»,
// вызывая бесконечное предложение восстановления при полном наборе данных
bool SharedDirWatcher::isRecoveryAvailable(void) const
{
    const QString repo=this->journalRepoPath();
    if(repo.isEmpty())
        return false;

    if(!GitWrapper::isGitAvailable())
        return false;

    const QString head=GitWrapper::getHead(repo);
    if(head.isEmpty())
        return false;

    // В журнале должно быть хотя бы одно состояние области sync/
    const QSet<QString> journalRoots=this->journalPublicationRoots(repo, head);
    if(journalRoots.isEmpty())
        return false;

    // Область sync/ отсутствует целиком — восстановление обязательно
    if(!QDir().exists(watchedPath))
        return true;

    // Есть ли публикации из журнала, отсутствующие на диске
    const QSet<QString> diskRoots=this->diskPublicationRoots(repo);
    for(const QString &root : journalRoots)
    {
        if(!diskRoots.contains(root))
            return true;
    }

    return false;
}


// Восстановление sync/ из журнала (HEAD единого shared/.git)
bool SharedDirWatcher::restoreSyncFromJournal(bool restoreOnlyMissing,
                                              QString *errorMessage)
{
    const QString repo=this->journalRepoPath();
    if(repo.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Журнал shared/.git не найден");
        return false;
    }

    if(!GitWrapper::isGitAvailable())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git недоступен в PATH");
        return false;
    }

    const QString head=GitWrapper::getHead(repo);
    if(head.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Журнал пуст (нет снимков)");
        return false;
    }

    if(!restoreOnlyMissing)
    {
        // Полное восстановление области sync/ (checkout HEAD -- sync)
        if(!GitWrapper::checkoutFromRef(repo, head, QStringLiteral("sync"), errorMessage))
            return false;
    }
    else
    {
        // Восстановление только публикаций, отсутствующих на диске
        const QSet<QString> journalRoots=this->journalPublicationRoots(repo, head);
        const QSet<QString> diskRoots=this->diskPublicationRoots(repo);

        bool restoredAny=false;
        const QStringList missingRoots=QStringList(journalRoots.values());
        for(const QString &root : missingRoots)
        {
            if(diskRoots.contains(root))
                continue;

            if(!GitWrapper::checkoutFromRef(repo, head, root, errorMessage))
                return false;

            restoredAny=true;
        }

        if(!restoredAny && errorMessage)
            errorMessage->clear();
    }

    // Сигнатура и журнал устарели после восстановления
    lastSignature=this->calculateDirSignature();
    this->scheduleSnapshot();

    emit sharedDirChanged();

    return true;
}


// «Архивирование истории» журнала: обрезка старых снимков
bool SharedDirWatcher::archiveJournalHistory(QString *errorMessage)
{
    const QString repo=this->journalRepoPath();
    if(repo.isEmpty())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("Журнал shared/.git не найден");
        return false;
    }

    if(!GitWrapper::isGitAvailable())
    {
        if(errorMessage)
            *errorMessage=QStringLiteral("git недоступен в PATH");
        return false;
    }

    const QString message=QStringLiteral("Архив журнала ")+QDateTime::currentDateTime().toString(Qt::ISODate);

    const bool result=GitWrapper::archiveHistory(repo, message, errorMessage);

    if(result)
        emit sharedDirChanged();

    return result;
}


// Число снимков в журнале
int SharedDirWatcher::journalSnapshotCount(void) const
{
    const QString repo=this->journalRepoPath();
    if(repo.isEmpty())
        return 0;

    if(!GitWrapper::isGitAvailable())
        return 0;

    return GitWrapper::getLogOneline(repo).size();
}


// Размер каталога единого журнала shared/.git в байтах
quint64 SharedDirWatcher::journalDirSizeBytes(void) const
{
    const QString repo=this->journalRepoPath();
    if(repo.isEmpty())
        return 0;

    quint64 totalBytes=0;
    QDirIterator it(repo+"/.git", QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while(it.hasNext())
    {
        it.next();
        totalBytes+=static_cast<quint64>(it.fileInfo().size());
    }

    return totalBytes;
}
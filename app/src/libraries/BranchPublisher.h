#ifndef _BRANCHPUBLISHER_H_
#define _BRANCHPUBLISHER_H_

#include <QString>
#include <QSet>
#include <QMap>
#include <QByteArray>

class KnowTreeModel;
class TreeItem;
class TeamProfile;

// Метаданные публикации (из meta.json)
struct PublicationMeta
{
    QString dirPath;            // Каталог публикации
    QString branchId;           // Машинный ключ ветки
    QString title;              // Название ветки на момент публикации
    QString ownerId;            // Идентификатор владельца
    QString ownerName;          // Имя владельца
    QString ownerEmail;         // Электронная почта владельца
    int publishVersion=0;       // Номер версии публикации
    QString publishedAt;        // Время публикации (ISO)
    bool encryptedSource=false; // Признак: источник был зашифрован
};

// Публикация ветки в общий каталог (shareddir).
//
// Макет (ADR-015 + ADR-016):
//   shared/
//     .git/        — единый локальный журнал истории всех публикаций (вне Syncthing)
//     local/       — авторитетная зона владельца (НЕ синхронизируется, дубль 2x);
//                    публикации собираются здесь до выкладки в sync/
//       <ownerId>_<ownerName>/<branchId>_<branchName>/   (если профиль настроен)
//       <branchId>_<branchName>/                         (плоская схема)
//     .tmp/        — сборка версий и .rev-бэкапы (вне sync/, та же файловая система)
//     sync/        — зеркало local/ (copy-changed promotion); единственная папка Syncthing
//       <ownerId>_<ownerName>/<branchId>_<branchName>/
//       <branchId>_<branchName>/
//         branch.xml    — фрагмент ветки (синтаксис mytetra.xml), актуальный срез
//         records/      — копии записей ветки
//         meta.json     — метаданные публикации (владелец, publishVersion)
//         changelog.json— append-only история версий (пишется владельцем, едет в sync/)
//         manifest.json — завершающий маркер версии (пишется последним)
//
// Машинный ключ публикации — часть имени каталога до первого '_'
// (branchId). Переименование ветки/владельца имя каталога не меняет;
// актуальный заголовок всегда в meta.json.
//
// Поток: сборка в .tmp/ -> rename в local/ -> коммит 1 (local/) ->
// promotion copy-changed в sync/ -> коммит 2 (sync/, с гардом целостности).
// mv local -> sync запрещён (теряет авторитет без git, фрагментирует историю,
// не убирает swap-окно в sync/).
//
// Устаревшие публикации (с per-branch .git внутри, прямо в shared/)
// продолжают читаться и обновляться в своём макете.

class BranchPublisher
{
public:
    // Вид операции публикации
    enum class Operation { Publish, Update };

    // Результат операции публикации
    struct Result
    {
        bool success=false;            // Успешность операции
        bool unchanged=false;          // Контент публикации не менялся (коммит не делался)
        QString publicationDir;        // Фактический каталог публикации
        int publishVersion=0;          // Номер версии публикации после операции
        bool largeContent=false;       // Признак большого объёма данных (рост журнала .git)
        bool journalDisabled=false;    // Журнал недоступен (нет git в PATH): данные записаны, снимок пропущен
        QString errorMessage;          // Текст ошибки при неудаче
    };

    // Имя каталога публикации: <branchId>_<санитизированное имя ветки>
    static QString buildBranchDirName(const QString &branchId, const QString &branchName);

    // Имя каталога владельца: <ownerId>_<санитизированное имя владельца>.
    // Пустая строка, если профиль не настроен
    static QString buildOwnerDirName(TeamProfile &profile);

    // Каталог данных: shareddir/sync (зеркало для Syncthing)
    static QString syncDir(const QString &sharedDir);

    // Авторитетная зона владельца: shareddir/local (не синхронизируется)
    static QString localDir(const QString &sharedDir);

    // Каталог сборки версий: shareddir/.tmp (вне sync/, та же файловая система)
    static QString tmpDir(const QString &sharedDir);

    // Проверка целостности публикации на конкретную версию (завершающий маркер).
    // Файл manifest.json внутри публикации (пишется последним) содержит карту
    // путь -> sha256 всех файлов данных версии. Версия считается полной, если
    // манифест наличествует, его publishVersion совпадает с аргументом и все
    // файлы присутствуют с совпадающими хешами.
    //   - устаревшая per-branch публикация (с .git внутри): целостность
    //     гарантируется коммитом — возвращается true без манифеста;
    //   - новый макет без манифеста (не доехал/утерян): false — дифф по
    //     фактическим файлам строить нельзя (Syncthing мог доставить часть).
    // Полная проверка делает полный проход по файлам (для гигантских веток
    // дорого); штатно UI-детект использует только версию манифеста, а полная
    // сверка выполняется в момент диффа/импорта (см. SubscriptionImportEngine)
    static bool publicationVersionComplete(const QString &publicationDir,
                                           int publishVersion);

    // Версия публикации из заголовка manifest.json (0 — манифеста нет/не читается).
    // Дешёвый маркер для детекта обновлений без полного прохода по файлам
    static int manifestPublishVersion(const QString &publicationDir);

    // Признак «данные версии ещё в пути» (дешёвый, только заголовок манифеста):
    // манифест актуальной версии отсутствует либо не совпал. Устаревшая
    // per-branch публикация (.git внутри) без манифеста — штатная, не ожидание:
    // её целостность гарантирует коммит, манифестов старый макет не имеет
    static bool isPublicationDataPending(const QString &publicationDir,
                                         int publishVersion);

    // Чтение манифеста: карта «относительный путь -> sha256 (hex)» всех
    // файлов данных и версия публикации. Пустая карта, если манифест отсутствует
    static void readManifest(const QString &publicationDir,
                             QMap<QString, QString> &fileShas,
                             int *publishVersionOut);

    // Запись файла manifest.json в каталог публикации/сборки (после meta.json).
    // Карта: относительный путь -> sha256 содержимого всех файлов данных.
    // Используется до moveTempToFinal; для автотестов — публично.
    static bool writePublicationManifest(const QString &dirPath,
                                         const QString &branchId,
                                         int publishVersion);

    // Фактический каталог публикации ветки в shareddir.
    // Ищется сначала в sync/ (новый макет), затем в самом shareddir
    // (устаревший макет). Пустая строка, если публикация не найдена.
    // Для операций владельца есть findLocalPublicationDir (авторитет local/).
    static QString findPublicationDir(const QString &sharedDir,
                                      const QString &ownerDirName,
                                      const QString &branchDirName);

    // Каталог публикации в авторитете local/ (для операций владельца).
    // Пустая строка, если в local/ публикации нет
    static QString findLocalPublicationDir(const QString &sharedDir,
                                           const QString &ownerDirName,
                                           const QString &branchDirName);

    // Чтение метаданных публикации из meta.json каталога публикации
    static PublicationMeta readPublication(const QString &publicationDir);

    // Список всех публикаций в shareddir (sync/ и устаревшая плоская схема;
    // два уровня: каталог владельца и плоская схема)
    static QList<PublicationMeta> listPublications(const QString &sharedDir);

    // Набор ключей веток, уже опубликованных в shareddir
    // (части имён каталогов публикаций до первого '_')
    static QSet<QString> listPublishedBranchKeys(const QString &sharedDir);

    // Проверка вложенности: ветка находится внутри уже опубликованной
    // или сама содержит опубликованную подветку
    static bool hasNestedPublication(TreeItem *startItem,
                                     const QSet<QString> &publishedBranchKeys);

    // Текущая версия публикации из meta.json (0 — публикации нет/не читается)
    static int readPublishVersion(const QString &publicationDir);

    // Основная операция публикации/обновления ветки.
    // Поток (ADR-016): сборка в shared/.tmp/ -> rename в local/ ->
    // коммит 1 (local/) -> promotion copy-changed в sync/ ->
    // коммит 2 (sync/, с гардом целостности)
    static Result publishBranch(KnowTreeModel *model, TreeItem *startItem,
                                TeamProfile &profile, Operation operation);

    // Отзыв публикации: удаляются в корзину каталоги и в local/, и в sync/
    static bool revokePublication(const QString &publicationDir);

    // Promotion зеркала: copy-changed из local-публикации в sync-публикацию.
    // Копируются новые/изменённые файлы данных (по sha256 манифеста local/),
    // удаляются убранные; meta.json, changelog.json и manifest.json — последними.
    // Каталог sync/ на месте всё время (без swap-окна). Возвращает false при ошибке
    static bool promoteToSync(const QString &localPublicationDir,
                              const QString &syncPublicationDir,
                              QString *errorMessage=nullptr);

    // Дописать запись версии в changelog.json публикации (создаёт файл при отсутствии).
    // changesJson — готовый JSON-массив изменений версии (из BranchDiffEngine)
    static bool appendChangelog(const QString &publicationDir,
                                int publishVersion,
                                const QString &ownerName,
                                const QString &changesJson);

    // Восстановить sync-публикацию из local-авторитета (перепромоут без git).
    // Возвращает false, если local-публикации нет или promotion неуспешен
    static bool restoreSyncFromLocal(const QString &sharedDir,
                                     const QString &syncPublicationDir,
                                     QString *errorMessage=nullptr);

    // Список авторитетных публикаций владельца в shared/local/
    static QList<PublicationMeta> listLocalPublications(const QString &sharedDir);

    // Гарантировать наличие .stignore в sync/ (генерируется кодом)
    static void ensureSyncIgnore(const QString &syncPath);

private:
    // Санитизация части имени каталога (имя ветки/владельца)
    static QString sanitizeNamePart(const QString &value);

    // Поиск публикации в одном каталоге-корне (sync/ или устаревший shareddir)
    static QString findPublicationInRoot(const QString &rootDir,
                                         const QString &ownerDirName,
                                         const QString &branchDirName);

    // Каталог публикации по имени ветки в указанном каталоге-родителе
    // (поиск по префиксу-ключу, чтобы пережить переименование ветки)
    static QString findPublicationByNameKey(const QString &parentDir,
                                            const QString &branchIdKey);

    // Список публикаций в одном каталоге-корне
    static QList<PublicationMeta> listPublicationsInRoot(const QString &rootDir);

    // Ключи веток в одном каталоге-корне (sync/ или устаревший shareddir)
    static QSet<QString> listPublishedBranchKeysInRoot(const QString &rootDir);

    // Сбор branch.xml из mytetra.xml (результат KnowTreeModel::exportBranchToDirectory)
    static bool buildBranchXmlFromMytetraXml(const QString &tempDir);

    // Запись meta.json
    static bool writeMetaJson(const QString &tempDir, TreeItem *startItem,
                              TeamProfile &profile, int publishVersion,
                              bool encryptedSource);

    // Размер содержимого каталога (для предупреждения о росте .git)
    static quint64 calculateDirSizeBytes(const QString &dirPath);

    // Перемещение собранного временного каталога в конечный с заменой (temp+rename)
    static bool moveTempToFinal(const QString &tempDir, const QString &finalDir,
                                Operation operation);

    // Рекурсивное копирование каталога целиком (для посева local/ из sync/)
    static bool copyDirectoryRecursively(const QString &sourceDir,
                                         const QString &destDir);

    // Обновление устаревшей per-branch публикации (с .git внутри) на месте
    static Result publishLegacyBranch(KnowTreeModel *model, TreeItem *startItem,
                                      TeamProfile &profile, Operation operation,
                                      const QString &publicationDir,
                                      const QString &ownerDirName,
                                      const QString &branchDirName,
                                      Result &result);

    // Сравнение содержимого публикации (branch.xml + records/) без служебных
    // файлов (meta.json, .git и временных каталогов) — для пропуска пустых
    // обновлений при автопубликации
    static bool isPublicationContentEqual(const QString &dirA, const QString &dirB);

    // Карта «относительный путь -> sha256 содержимого» содержимого публикации
    static QMap<QString, QByteArray> publicationContentDigest(const QString &dir);
};

#endif // _BRANCHPUBLISHER_H_
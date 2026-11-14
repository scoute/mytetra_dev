#ifndef SYNCMANIFEST_H
#define SYNCMANIFEST_H

#include <QString>
#include <QMap>
#include <QStringList>
#include <QJsonObject>

class KnowTreeModel;
class SyncSidecar;

// Манифест базы для синхронизации: что есть с обеих сторон и что новее.
// Строится из модели + сайдкара; недостающие mtime/hash досчитываются
// живьем (текст/аттачи читаются с диска, ветки — по полям), чтобы первый
// синк старой базы отработал без предыстории.
// Порядок веток НЕ синхронизируется (ограничение MVP): только состав,
// родители и содержимое.
struct SyncBranchInfo
{
    QString id;
    QString parentId;
    qint64 mtime=0;
    QString hash;
    QMap<QString, QString> fields;
};

struct SyncAttachInfo
{
    QString id;
    QString fileName;
    qint64 size=0;
    QString hash;
};

struct SyncNoteInfo
{
    QString id;
    QString branchId;
    qint64 mtime=0;
    QString hash;
    bool crypt=false;
    QMap<QString, QString> fields;
    QList<SyncAttachInfo> attaches;
};

struct SyncManifest
{
    QMap<QString, SyncBranchInfo> branches; // без корневого псевдо-элемента
    QMap<QString, SyncNoteInfo> notes;
    QMap<QString, qint64> tombstones; // id -> deletedAt (заметки и ветки)

    QJsonObject toJson(void) const;
    static SyncManifest fromJson(const QJsonObject &obj);
};

// План сверки локального манифеста с удаленным.
// upload/download — id заметок/веток целиком; deleteLocal — применить
// чужие tombstone'ы у себя; conflicts — равные mtime при разных хешах
// (решает хаб вручную/авто по правилу, проигравший в бэкап).
struct SyncPlan
{
    QStringList uploadNotes;
    QStringList downloadNotes;
    QStringList uploadBranches;
    QStringList downloadBranches;
    QStringList deleteLocal; // id с любой стороны tombstone
    QStringList conflicts;
    bool empty(void) const;
};

class SyncManifestBuilder
{
public:

    // Построить локальный манифест. tetradir — корень базы (для чтения
    // текстов и аттачей при досчете хешей). isHub ни на что не влияет
    // на построении, нужен только на сверке
    static SyncManifest build(KnowTreeModel *model, SyncSidecar &sidecar,
                              const QString &tetradir);

    // Сверить локальный с удаленным. isHub=true — мы хаб пары и наши
    // равные mtime побеждают (проигравший все равно в бэкап и в отчет)
    static SyncPlan compare(const SyncManifest &local,
                            const SyncManifest &remote,
                            bool isHub);

    static QString sha256File(const QString &path);
    static QString sha256Bytes(const QByteArray &data);
};

#endif // SYNCMANIFEST_H

#ifndef SYNCTOMBSTONES_H
#define SYNCTOMBSTONES_H

#include <QMap>
#include <QSet>
#include <QString>

// Покрытие tombstone'ом по цепочке родителей.
// Вынесено из SyncManifest.cpp в header без зависимостей от модели:
// правило «tombstone ветки покрывает поддерево» должно быть одинаковым
// для веток и заметок и покрываться тестом.
namespace SyncTombstones
{

// id покрыт, если tombstone стоит на нём самом или на любом предке.
// parents: id -> parentId ("" — корень). Циклы безопасны (seen).
inline bool coveredByAncestor(const QString &id,
                              const QMap<QString, QString> &parents,
                              const QMap<QString, qint64> &tombstones)
{
    QString cur=id;
    QSet<QString> seen;

    while(!cur.isEmpty() && !seen.contains(cur))
    {
        seen.insert(cur);

        if(tombstones.contains(cur))
            return true;

        cur=parents.value(cur);
    }

    return false;
}

} // namespace SyncTombstones

#endif // SYNCTOMBSTONES_H

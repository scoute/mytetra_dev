#ifndef SYNCCOUNTS_H
#define SYNCCOUNTS_H

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include "SyncManifest.h"

// Подсчёт «сколько элементов исчезнет вместе с id»: сама ветка, все
// подветки и все их заметки (заметка = 1). Индексы строятся один раз —
// раньше на каждый tombstone/удаление заново сканировался весь манифест
// (O(n²) перед диалогом подтверждения на больших удалениях).
class SyncSubtreeCounter
{
public:
    SyncSubtreeCounter(const QMap<QString, SyncBranchInfo> &branches,
                       const QMap<QString, SyncNoteInfo> &notes);

    int count(const QString &id) const;

private:
    QMap<QString, QStringList> m_children; // parentId -> child ids
    QMap<QString, int> m_noteCounts;       // branchId -> notes
    QSet<QString> m_notes;
    QSet<QString> m_branches;
};

#endif // SYNCCOUNTS_H

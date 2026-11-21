#include <QList>
#include <QSet>

#include "SyncCounts.h"


SyncSubtreeCounter::SyncSubtreeCounter(
    const QMap<QString, SyncBranchInfo> &branches,
    const QMap<QString, SyncNoteInfo> &notes)
{
    for(auto it=branches.constBegin(); it!=branches.constEnd(); ++it)
    {
        m_branches.insert(it.key());
        m_children[it.value().parentId] << it.key();
    }

    for(auto it=notes.constBegin(); it!=notes.constEnd(); ++it)
    {
        m_notes.insert(it.key());
        m_noteCounts[it.value().branchId] += 1;
    }
}


int SyncSubtreeCounter::count(const QString &id) const
{
    int total=0;
    QStringList queue;
    QSet<QString> seen;

    queue << id;

    while(!queue.isEmpty())
    {
        const QString cur=queue.takeFirst();

        if(cur.isEmpty() || seen.contains(cur))
            continue;

        seen.insert(cur);

        // Заметка (в т.ч. переданная как корень) считается за 1
        if(m_notes.contains(cur))
        {
            total+=1;
            continue;
        }

        if(!m_branches.contains(cur))
            continue;

        total+=1+m_noteCounts.value(cur);
        queue << m_children.value(cur);
    }

    return total;
}

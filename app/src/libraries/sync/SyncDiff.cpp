#include <QVector>

#include "SyncDiff.h"

namespace SyncDiff
{

namespace
{
// Потолок для LCS: слишком длинные середины дают грубый diff
// (все удалены + все добавлены) — дешевле, чем считать гигантскую матрицу
const int kMaxLcsSide=1500;

Line makeLine(Line::Kind kind, const QString &text)
{
    Line line;
    line.kind=kind;
    line.text=text;
    return line;
}
}


QList<Line> diff(const QString &before, const QString &after)
{
    const QStringList a=before.split(QLatin1Char('\n'));
    const QStringList b=after.split(QLatin1Char('\n'));

    // Общий префикс и суффикс отрезаются: реальные правки заметок
    // меняют маленький кусок большого текста, и LCS нужен только на нем
    int prefix=0;

    while(prefix<a.size() && prefix<b.size() && a.at(prefix)==b.at(prefix))
        prefix++;

    int suffix=0;

    while(suffix<(a.size()-prefix) && suffix<(b.size()-prefix)
          && a.at(a.size()-1-suffix)==b.at(b.size()-1-suffix))
        suffix++;

    const int aMid=a.size()-prefix-suffix;
    const int bMid=b.size()-prefix-suffix;

    QList<Line> out;

    for(int i=0; i<prefix; ++i)
        out.append(makeLine(Line::Equal, a.at(i)));

    if(aMid>kMaxLcsSide || bMid>kMaxLcsSide)
    {
        for(int i=0; i<aMid; ++i)
            out.append(makeLine(Line::Removed, a.at(prefix+i)));

        for(int i=0; i<bMid; ++i)
            out.append(makeLine(Line::Added, b.at(prefix+i)));
    }
    else if(aMid>0 || bMid>0)
    {
        const QStringList am=a.mid(prefix, aMid);
        const QStringList bm=b.mid(prefix, bMid);

        // dp[i][j] — длина LCS суффиксов am[i..) и bm[j..)
        QVector<QVector<int>> dp(aMid+1, QVector<int>(bMid+1, 0));

        for(int i=aMid-1; i>=0; --i)
            for(int j=bMid-1; j>=0; --j)
                dp[i][j]=(am.at(i)==bm.at(j))
                    ? dp[i+1][j+1]+1
                    : qMax(dp[i+1][j], dp[i][j+1]);

        int i=0, j=0;

        while(i<aMid && j<bMid)
        {
            if(am.at(i)==bm.at(j))
            {
                out.append(makeLine(Line::Equal, am.at(i)));
                ++i;
                ++j;
            }
            else if(dp[i+1][j]>=dp[i][j+1])
            {
                out.append(makeLine(Line::Removed, am.at(i)));
                ++i;
            }
            else
            {
                out.append(makeLine(Line::Added, bm.at(j)));
                ++j;
            }
        }

        while(i<aMid)
        {
            out.append(makeLine(Line::Removed, am.at(i)));
            ++i;
        }

        while(j<bMid)
        {
            out.append(makeLine(Line::Added, bm.at(j)));
            ++j;
        }
    }

    for(int i=0; i<suffix; ++i)
        out.append(makeLine(Line::Equal, a.at(a.size()-suffix+i)));

    return out;
}


QString summary(const QList<Line> &lines)
{
    int added=0;
    int removed=0;

    for(const Line &line : lines)
    {
        if(line.kind==Line::Added)
            added++;
        else if(line.kind==Line::Removed)
            removed++;
    }

    return QStringLiteral("+%1 -%2").arg(added).arg(removed);
}

} // namespace SyncDiff

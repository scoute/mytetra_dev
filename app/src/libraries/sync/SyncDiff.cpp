#include <QRegularExpression>
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


QString decodeEntities(QString s)
{
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&apos;"), QStringLiteral("'"));

    // &#123; и &#x1F;
    static const QRegularExpression entityRe(
        QStringLiteral("&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]{1,8});"));

    QString out;
    QRegularExpressionMatchIterator it=entityRe.globalMatch(s);
    int pos=0;

    while(it.hasNext())
    {
        const QRegularExpressionMatch m=it.next();
        out+=s.mid(pos, m.capturedStart()-pos);
        pos=m.capturedEnd();

        const QString body=m.captured(1);
        bool ok=false;
        int code=0;

        if(body.startsWith(QStringLiteral("#x"), Qt::CaseInsensitive))
            code=body.mid(2).toInt(&ok, 16);
        else if(body.startsWith(QLatin1Char('#')))
            code=body.mid(1).toInt(&ok, 10);

        if(ok && code>0 && code<=0x10FFFF
           && !(code>=0xD800 && code<=0xDFFF))
            out+=QString::fromUcs4(reinterpret_cast<const uint *>(&code), 1);
        else
            out+=m.captured(0);
    }

    out+=s.mid(pos);
    return out;
}


QStringList visibleLines(const QString &text)
{
    QString s=text;

    // <style>/<script>/<head> — целиком, вместе с содержимым
    static const QRegularExpression headRe(
        QStringLiteral("<(style|script|head|title)[^>]*>.*?</\\1>"),
        QRegularExpression::CaseInsensitiveOption
        | QRegularExpression::DotMatchesEverythingOption);
    s.remove(headRe);

    // <img> — в отдельные строки-заглушки с именем файла
    static const QRegularExpression imgRe(
        QStringLiteral("<img[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression srcRe(
        QStringLiteral("src\\s*=\\s*[\"']?([^\"'\\s>]+)"));

    QRegularExpressionMatchIterator it=imgRe.globalMatch(s);
    QString rebuilt;
    int pos=0;

    while(it.hasNext())
    {
        const QRegularExpressionMatch m=it.next();
        rebuilt+=s.mid(pos, m.capturedStart()-pos);
        pos=m.capturedEnd();

        const QRegularExpressionMatch sm=srcRe.match(m.captured(0));
        QString src=sm.hasMatch() ? sm.captured(1) : QString();
        QString name=src.section(QLatin1Char('/'), -1);
        name=name.section(QLatin1Char('\\'), -1);

        if(name.isEmpty())
            name=QStringLiteral("картинка");

        rebuilt+=QStringLiteral("\n\n[картинка: ")+name+QStringLiteral("]\n\n");
    }

    rebuilt+=s.mid(pos);
    s=rebuilt;

    // блочные теги — в переносы, остальные теги — прочь
    static const QRegularExpression blockRe(
        QStringLiteral("</?(p|div|li|ul|ol|h[1-6]|br|tr|table|blockquote|hr)[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tagRe(QStringLiteral("<[^>]*>"));
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));

    s.replace(blockRe, QStringLiteral("\n"));
    s.remove(tagRe);
    s=decodeEntities(s);

    QStringList out;

    for(const QString &raw : s.split(QLatin1Char('\n')))
    {
        QString line=raw.trimmed();
        line.replace(wsRe, QStringLiteral(" "));

        if(!line.isEmpty())
            out << line;
    }

    return out;
}


QList<Line> diff(const QString &before, const QString &after)
{
    return diffLines(before.split(QLatin1Char('\n')),
                     after.split(QLatin1Char('\n')));
}


QList<Line> diffLines(const QStringList &a, const QStringList &b)
{
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

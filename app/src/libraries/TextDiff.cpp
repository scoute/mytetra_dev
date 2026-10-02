#include "TextDiff.h"

#include <QMap>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <QXmlStreamReader>


// Блочные элементы, начинающие новую строку при извлечении текста
static QSet<QString> blockElements()
{
    static QSet<QString> elements;
    if(elements.isEmpty())
    {
        elements << "p" << "div" << "li" << "ul" << "ol"
                 << "h1" << "h2" << "h3" << "h4" << "h5" << "h6"
                 << "tr" << "table" << "blockquote" << "hr" << "br";
    }
    return elements;
}


QStringList TextDiff::htmlToTextLines(const QString &html)
{
    // Строгий разбор: точный, с именами картинок
    QStringList lines;
    QString current;
    bool hasContent=false;
    bool parseError=false;

    // Оборачиваем: текст заметки может быть фрагментом без единого корня
    QXmlStreamReader reader(QStringLiteral("<_root>")+html+QStringLiteral("</_root>"));
    const QSet<QString> blocks=blockElements();

    // Содержимое style/script — мусор для сравнения и подсказок, пропускаем
    int skipDepth=0;

    auto flushLine=[&]()
    {
        // Пустые строки сохраняем (структура текста), но схлопываем дубли
        if(hasContent || !lines.isEmpty())
            lines.append(current);
        current.clear();
        hasContent=false;
    };

    while(!reader.atEnd())
    {
        reader.readNext();

        if(reader.hasError())
        {
            parseError=true;
            break;
        }

        if(reader.isStartElement())
        {
            const QString name=reader.name().toString().toLower();

            if(name=="_root")
                continue;

            if(name=="style" || name=="script")
            {
                skipDepth++;
                continue;
            }

            if(skipDepth>0)
                continue;

            if(name=="img")
            {
                flushLine();
                const QString src=reader.attributes().value("src").toString();
                lines.append(QStringLiteral("[изображение: %1]").arg(src.isEmpty() ? "?" : src));
                hasContent=true;
                continue;
            }

            if(blocks.contains(name))
            {
                if(hasContent || !current.isEmpty())
                    flushLine();
                else if(name=="br" || name=="hr")
                    flushLine();
                continue;
            }
        }
        else if(reader.isCharacters())
        {
            if(skipDepth>0)
                continue;
            const QString text=reader.text().toString();
            if(!text.trimmed().isEmpty())
            {
                current+=text;
                hasContent=true;
            }
        }
        else if(reader.isEndElement())
        {
            const QString name=reader.name().toString().toLower();
            if((name=="style" || name=="script") && skipDepth>0)
            {
                skipDepth--;
                continue;
            }
            if(skipDepth>0)
                continue;
            if(blocks.contains(name) && (hasContent || !current.isEmpty()))
                flushLine();
        }
    }

    if(!parseError)
    {
        if(hasContent || !current.isEmpty())
            lines.append(current);

        // Убираем хвостовые пустые строки
        while(!lines.isEmpty() && lines.last().trimmed().isEmpty())
            lines.removeLast();

        return lines;
    }

    return TextDiff::regexToTextLines(html);
}


QStringList TextDiff::regexToTextLines(const QString &html)
{
    QString text=html;

    // Блоки style/script — мусор, вырезаются целиком
    static const QRegularExpression styleRe("<\\s*(style|script)[^>]*>.*?</\\s*\\1\\s*>",
                                            QRegularExpression::CaseInsensitiveOption
                                            | QRegularExpression::DotMatchesEverythingOption);
    text.replace(styleRe, "\n");

    // Картинки — в плейсхолдеры до счистки тегов
    static const QRegularExpression imgRe("<img[^>]*src\\s*=\\s*\"([^\"]*)\"[^>]*>",
                                          QRegularExpression::CaseInsensitiveOption);
    text.replace(imgRe, "\n[изображение: \\1]\n");
    static const QRegularExpression imgRe2("<img[^>]*>",
                                           QRegularExpression::CaseInsensitiveOption);
    text.replace(imgRe2, "\n[изображение: ?]\n");

    // <br> и блочные — в переносы
    static const QRegularExpression brRe("<\\s*br\\s*/?\\s*>",
                                         QRegularExpression::CaseInsensitiveOption);
    text.replace(brRe, "\n");
    static const QRegularExpression blockRe("</\\s*(p|div|li|h[1-6]|tr|table|blockquote)\\s*>",
                                            QRegularExpression::CaseInsensitiveOption);
    text.replace(blockRe, "\n");

    // Остальные теги — долой
    static const QRegularExpression tagRe("<[^>]*>");
    text.replace(tagRe, QString());

    // HTML-сущности
    text.replace("&nbsp;", " ");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&amp;", "&");
    text.replace("&quot;", "\"");

    QStringList raw=text.split('\n');
    QStringList lines;
    for(QString line : raw)
    {
        line=line.trimmed();
        if(!line.isEmpty() || !lines.isEmpty())
            lines.append(line);
    }
    while(!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    return lines;
}


// Внутреннее представление операции Myers
struct MyersOp
{
    // true — взять строку из старого (Del) или нового (Add) текста
    bool takeOld=false;
    bool takeNew=false;
};

QList<TextDiffLine> TextDiff::diffLines(const QStringList &oldLines,
                                        const QStringList &newLines)
{
    QList<TextDiffLine> result;

    const int n=oldLines.size();
    const int m=newLines.size();

    if(n==0 && m==0)
        return result;

    if(n+m>MaxTotalLines)
    {
        TextDiffLine stub;
        stub.kind=TextDiffLine::Same;
        stub.text=QStringLiteral("<too-large>");
        result.append(stub);
        return result;
    }

    // Myers O(ND): жадный проход по диагоналям k = x - y
    const int max=n+m;
    const int offset=max+1;
    QVector<int> v(2*max+3, -1);
    v[offset+1]=0;

    // След для обратного прохода: копии V на каждом D
    QList<QVector<int>> trace;
    trace.reserve(qMin(max+1, MaxDiffCost+1));

    int foundD=-1;
    for(int d=0; d<=max; ++d)
    {
        if(d>MaxDiffCost)
            break;

        for(int k=-d; k<=d; k+=2)
        {
            int x;
            if(k==-d || (k!=d && v[offset+k-1]<v[offset+k+1]))
                x=v[offset+k+1];
            else
                x=v[offset+k-1]+1;

            int y=x-k;
            while(x<n && y<m && oldLines.at(x)==newLines.at(y))
            {
                x++;
                y++;
            }
            v[offset+k]=x;

            if(x>=n && y>=m)
            {
                trace.append(v);
                foundD=d;
                break;
            }
        }

        if(foundD>=0)
            break;

        trace.append(v);
    }

    if(foundD<0)
    {
        TextDiffLine stub;
        stub.kind=TextDiffLine::Same;
        stub.text=QStringLiteral("<too-large>");
        result.append(stub);
        return result;
    }

    // Обратный проход: восстановление скрипта
    QList<MyersOp> script;
    int x=n;
    int y=m;
    for(int d=foundD; d>0; --d)
    {
        const QVector<int> &prev=trace.at(d-1);
        const int k=x-y;

        int prevK;
        if(k==-d || (k!=d && prev[offset+k-1]<prev[offset+k+1]))
            prevK=k+1;
        else
            prevK=k-1;

        const int prevX=prev[offset+prevK];
        const int prevY=prevX-prevK;

        while(x>prevX && y>prevY)
        {
            MyersOp op;
            op.takeOld=true;
            op.takeNew=true;
            script.prepend(op);
            x--;
            y--;
        }

        if(x==prevX)
        {
            MyersOp op;
            op.takeNew=true;
            script.prepend(op);
        }
        else
        {
            MyersOp op;
            op.takeOld=true;
            script.prepend(op);
        }

        x=prevX;
        y=prevY;
    }

    // Начальная диагональ (общие строки)
    while(x>0 && y>0)
    {
        MyersOp op;
        op.takeOld=true;
        op.takeNew=true;
        script.prepend(op);
        x--;
        y--;
    }

    // Скрипт в строки diff
    int oi=0;
    int ni=0;
    for(const MyersOp &op : script)
    {
        TextDiffLine line;
        if(op.takeOld && op.takeNew)
        {
            line.kind=TextDiffLine::Same;
            line.text=oldLines.at(oi);
            oi++;
            ni++;
        }
        else if(op.takeOld)
        {
            line.kind=TextDiffLine::Del;
            line.text=oldLines.at(oi);
            oi++;
        }
        else
        {
            line.kind=TextDiffLine::Add;
            line.text=newLines.at(ni);
            ni++;
        }
        result.append(line);
    }

    return result;
}


bool TextDiff::isTruncated(const QList<TextDiffLine> &diff)
{
    return diff.size()==1
           && diff.first().kind==TextDiffLine::Same
           && (diff.first().text==QStringLiteral("<too-large>"));
}


QString TextDiff::diffToHtml(const QList<TextDiffLine> &diff, int context)
{
    if(diff.isEmpty())
        return QString();

    if(isTruncated(diff))
        return QObject::tr("Текст слишком большой для построчного сравнения.");

    // Hunks: группы изменений с контекстом
    struct Hunk { int start=0; int end=0; };
    QList<Hunk> hunks;

    int i=0;
    const int total=diff.size();
    while(i<total)
    {
        if(diff.at(i).kind==TextDiffLine::Same)
        {
            i++;
            continue;
        }

        Hunk hunk;
        hunk.start=qMax(0, i-context);

        int j=i;
        while(j<total)
        {
            if(diff.at(j).kind!=TextDiffLine::Same)
            {
                j++;
                continue;
            }

            // Сколько подряд идущих Same впереди
            int run=j;
            while(run<total && diff.at(run).kind==TextDiffLine::Same)
                run++;

            if(run-j>2*context)
                break;

            j=run;
        }
        hunk.end=qMin(total, j+context);

        hunks.append(hunk);
        i=hunk.end;
    }

    QStringList html;
    html << QStringLiteral("<div style=\"font-family: monospace;\">");

    // Номера строк старого/нового текста для заголовков hunks
    QVector<int> oldNo(total+1, 0);
    QVector<int> newNo(total+1, 0);
    {
        int o=0;
        int nn=0;
        for(int k=0; k<total; ++k)
        {
            oldNo[k]=o+1;
            newNo[k]=nn+1;
            if(diff.at(k).kind!=TextDiffLine::Add)
                o++;
            if(diff.at(k).kind!=TextDiffLine::Del)
                nn++;
        }
        oldNo[total]=o;
        newNo[total]=nn;
    }

    for(const Hunk &hunk : hunks)
    {
        html << QStringLiteral("<div style=\"color: #888;\">@@ %1–%2 → %3–%4 @@</div>")
                .arg(oldNo[hunk.start]).arg(oldNo[hunk.end])
                .arg(newNo[hunk.start]).arg(newNo[hunk.end]);

        for(int k=hunk.start; k<hunk.end; ++k)
        {
            const TextDiffLine &line=diff.at(k);
            const QString safeText=line.text.toHtmlEscaped();

            if(line.kind==TextDiffLine::Del)
                html << QStringLiteral("<div style=\"background: #ffd7d7;\">− %1</div>").arg(safeText);
            else if(line.kind==TextDiffLine::Add)
                html << QStringLiteral("<div style=\"background: #d7ffd7;\">+ %1</div>").arg(safeText);
            else
                html << QStringLiteral("<div style=\"color: #666;\">&nbsp;&nbsp;%1</div>").arg(
                        safeText.isEmpty() ? QStringLiteral(" ") : safeText);
        }
    }

    html << QStringLiteral("</div>");
    return html.join(QString());
}


QString TextDiff::diffHtml(const QString &oldHtml, const QString &newHtml)
{
    const QStringList oldLines=TextDiff::htmlToTextLines(oldHtml);
    const QStringList newLines=TextDiff::htmlToTextLines(newHtml);

    const QList<TextDiffLine> diff=TextDiff::diffLines(oldLines, newLines);

    if(diff.isEmpty())
        return QString();

    // Только Same — текстовых различий нет (разница лишь в разметке)
    bool onlySame=true;
    for(const TextDiffLine &line : diff)
    {
        if(line.kind!=TextDiffLine::Same)
        {
            onlySame=false;
            break;
        }
    }
    if(onlySame)
        return QString();

    return TextDiff::diffToHtml(diff);
}

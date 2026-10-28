#include <QFile>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextImageFormat>

#include "UnifiedDiff.h"
#include "libraries/GitHelper.h"


// Построчное представление HTML: картинки подменяются читаемым
// маркером [image: имя] (toPlainText давал невидимый U+FFFC),
// stray-объекты без формата картинки - маркером [object]
QStringList UnifiedDiff::htmlToLines(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);

    QStringList lines;
    QString current;

    for(QTextBlock block=document.begin(); block!=document.end(); block=block.next())
    {
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment=it.fragment();

            if(!fragment.isValid())
                continue;

            if(fragment.charFormat().isImageFormat())
            {
                QString name=fragment.charFormat().toImageFormat().name();

                if(name.isEmpty())
                    name=QStringLiteral("image");

                current+=QStringLiteral("[image: %1]").arg(name);
                continue;
            }

            QString text=fragment.text();
            text.replace(QChar::ObjectReplacementCharacter,
                         QStringLiteral("[object]"));

            // Явные разрывы внутри фрагмента режут строку
            int start=0;

            for(int i=0; i<text.size(); ++i)
            {
                if(text.at(i)==QLatin1Char('\n') || text.at(i)==QChar(0x2028))
                {
                    current+=text.mid(start, i-start);
                    lines.append(current);
                    current.clear();
                    start=i+1;
                }
            }

            current+=text.mid(start);
        }

        lines.append(current);
        current.clear();
    }

    return lines;
}


QString UnifiedDiff::diffLines(const QStringList &before,
                               const QStringList &after,
                               const QString &title)
{
    if(!GitHelper::isAvailable())
        return QString();

    QTemporaryDir tempDir;

    if(!tempDir.isValid())
        return QString();

    const QString beforePath=tempDir.path()+QStringLiteral("/before.txt");
    const QString afterPath=tempDir.path()+QStringLiteral("/after.txt");

    QFile beforeFile(beforePath);

    if(!beforeFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();

    beforeFile.write(before.join(QStringLiteral("\n")).toUtf8());
    beforeFile.close();

    QFile afterFile(afterPath);

    if(!afterFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();

    afterFile.write(after.join(QStringLiteral("\n")).toUtf8());
    afterFile.close();

    QStringList args;
    args << QStringLiteral("--no-pager")
         << QStringLiteral("diff")
         << QStringLiteral("--no-index")
         << QStringLiteral("--no-color")
         << QStringLiteral("--no-ext-diff")
         << QStringLiteral("--no-textconv")
         << QStringLiteral("-U3")
         << QStringLiteral("--")
         << beforePath
         << afterPath;

    QString diff;
    const int code=GitHelper::run(tempDir.path(), args, &diff);

    // Код 0 - различий нет, 1 - есть. Остальное - ошибка git
    if(code!=0 && code!=1)
        return QString();

    if(diff.isEmpty())
        return QString();

    diff.replace(QStringLiteral("a/")+beforePath, QStringLiteral("a/")+title);
    diff.replace(QStringLiteral("b/")+afterPath, QStringLiteral("b/")+title);
    diff.replace(beforePath, title);
    diff.replace(afterPath, title);

    return diff;
}


QString UnifiedDiff::toColoredHtml(const QString &diff, bool dark)
{
    QString html=QStringLiteral("<pre style=\"white-space: pre-wrap;\">\n");

    const QStringList lines=diff.split(QStringLiteral("\n"));

    for(const QString &line : lines)
    {
        QString color=dark ? QStringLiteral("#DFE1E2") : QStringLiteral("#000000");
        QString background=QStringLiteral("transparent");

        if(line.startsWith(QStringLiteral("@@")))
        {
            color=dark ? QStringLiteral("#8fa8bf") : QStringLiteral("#666666");
            background=dark ? QStringLiteral("#263646") : QStringLiteral("#f0f0f0");
        }
        else if(line.startsWith(QStringLiteral("+++")) ||
                line.startsWith(QStringLiteral("---")))
        {
            color=dark ? QStringLiteral("#8fa8bf") : QStringLiteral("#666666");
        }
        else if(line.startsWith(QLatin1Char('+')))
        {
            color=dark ? QStringLiteral("#7ee787") : QStringLiteral("#006400");
            background=dark ? QStringLiteral("#1d3a28") : QStringLiteral("#e6ffed");
        }
        else if(line.startsWith(QLatin1Char('-')))
        {
            color=dark ? QStringLiteral("#ff9e9e") : QStringLiteral("#8b0000");
            background=dark ? QStringLiteral("#3d2226") : QStringLiteral("#ffeef0");
        }
        else if(line.startsWith(QStringLiteral("diff ")))
        {
            color=dark ? QStringLiteral("#8fa8bf") : QStringLiteral("#666666");
        }

        html+=QStringLiteral("<div style=\"color: %1; background: %2;\">%3</div>\n")
              .arg(color, background, line.toHtmlEscaped());
    }

    html+=QStringLiteral("</pre>");

    return html;
}

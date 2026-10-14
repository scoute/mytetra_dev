#include <QFile>
#include <QTemporaryDir>
#include <QTextDocument>

#include "UnifiedDiff.h"
#include "libraries/GitHelper.h"


QStringList UnifiedDiff::htmlToLines(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);

    return document.toPlainText().split(QStringLiteral("\n"));
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


QString UnifiedDiff::toColoredHtml(const QString &diff)
{
    QString html=QStringLiteral("<pre style=\"white-space: pre-wrap;\">\n");

    const QStringList lines=diff.split(QStringLiteral("\n"));

    for(const QString &line : lines)
    {
        QString color=QStringLiteral("#000000");
        QString background=QStringLiteral("transparent");

        if(line.startsWith(QStringLiteral("@@")))
        {
            color=QStringLiteral("#666666");
            background=QStringLiteral("#f0f0f0");
        }
        else if(line.startsWith(QStringLiteral("+++")) ||
                line.startsWith(QStringLiteral("---")))
        {
            color=QStringLiteral("#666666");
        }
        else if(line.startsWith(QLatin1Char('+')))
        {
            color=QStringLiteral("#006400");
            background=QStringLiteral("#e6ffed");
        }
        else if(line.startsWith(QLatin1Char('-')))
        {
            color=QStringLiteral("#8b0000");
            background=QStringLiteral("#ffeef0");
        }
        else if(line.startsWith(QStringLiteral("diff ")))
        {
            color=QStringLiteral("#666666");
        }

        html+=QStringLiteral("<div style=\"color: %1; background: %2;\">%3</div>\n")
              .arg(color, background, line.toHtmlEscaped());
    }

    html+=QStringLiteral("</pre>");

    return html;
}

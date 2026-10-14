#include "VoiceNoteHelper.h"

#include <QDir>
#include <QTextDocument>
#include <QTextCursor>
#include <QRegularExpression>

static const char *voiceSchemePrefix="voicenote://";


QString VoiceNoteHelper::makeHref(const QString &fileName)
{
    return QString::fromLatin1(voiceSchemePrefix)+fileName;
}


bool VoiceNoteHelper::isVoiceHref(const QString &href)
{
    return href.startsWith(QString::fromLatin1(voiceSchemePrefix));
}


QString VoiceNoteHelper::fileNameFromHref(const QString &href)
{
    if(href.startsWith(QString::fromLatin1(voiceSchemePrefix)))
        return href.mid(static_cast<int>(sizeof("voicenote://")-1));

    return QString();
}


QString VoiceNoteHelper::formatDuration(qint64 ms)
{
    if(ms<0)
        ms=0;

    const qint64 totalSeconds=ms/1000;

    return QString::number(totalSeconds/60)+QStringLiteral(":")+
           QString::number(totalSeconds%60).rightJustified(2, QLatin1Char('0'));
}


QString VoiceNoteHelper::linkText(qint64 durationMs)
{
    return QStringLiteral("[voice ")+formatDuration(durationMs)+QStringLiteral("]");
}


qint64 VoiceNoteHelper::parseDuration(const QString &text)
{
    // Обратное к linkText: "[voice 2:05]" или "2:05" в мс
    static const QRegularExpression pattern(QStringLiteral("(\\d+):(\\d\\d)"));
    const QRegularExpressionMatch match=pattern.match(text);

    if(!match.hasMatch())
        return -1;

    return (match.captured(1).toLongLong()*60+match.captured(2).toLongLong())*1000;
}


QString VoiceNoteHelper::resolveFilePath(const QString &workDir, const QString &href)
{
    const QString fileName=fileNameFromHref(href);

    if(fileName.isEmpty() || workDir.isEmpty())
        return QString();

    return QDir(workDir).absoluteFilePath(fileName);
}


QTextCharFormat VoiceNoteHelper::bubbleCharFormat(const QString &href,
                                                 qint64 durationMs,
                                                 const QString &fileName)
{
    QTextCharFormat format;
    format.setObjectType(BubbleObjectType);
    format.setProperty(BubblePropHref, href);
    format.setProperty(BubblePropDurationMs, durationMs);
    format.setProperty(BubblePropFileName, fileName);

    return format;
}


bool VoiceNoteHelper::isBubbleFormat(const QTextCharFormat &format)
{
    return format.objectType()==BubbleObjectType;
}


QString VoiceNoteHelper::bubbleHrefAt(QTextDocument *doc, int pos)
{
    if(doc==nullptr)
        return QString();

    // Формат смотрится на символе слева от курсора: принадлежность
    // символа i проверяется с позиции i+1. Смотрятся позиция клика
    // и сосед слева: курсор встает с любой стороны бабла
    const int count=doc->characterCount();

    for(int index=0; index<2; ++index)
    {
        const int checkPos=(index==0) ? pos : pos-1;

        if(checkPos<0 || checkPos>=count-1)
            continue;

        QTextCursor cursor(doc);
        cursor.setPosition(checkPos+1);

        const QTextCharFormat format=cursor.charFormat();

        if(isBubbleFormat(format))
            return format.property(BubblePropHref).toString();
    }

    return QString();
}

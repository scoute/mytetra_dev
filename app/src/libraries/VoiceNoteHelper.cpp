#include "VoiceNoteHelper.h"

#include <QDir>

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


QString VoiceNoteHelper::resolveFilePath(const QString &workDir, const QString &href)
{
    const QString fileName=fileNameFromHref(href);

    if(fileName.isEmpty() || workDir.isEmpty())
        return QString();

    return QDir(workDir).absoluteFilePath(fileName);
}

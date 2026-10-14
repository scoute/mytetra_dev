#include "VoiceNoteHelper.h"

#include <QDir>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QRegularExpression>

static const char *voiceSchemePrefix="voicenote://";

// MIME для переноса голосовых между заметками: пары строк
// "ссылка \n абсолютный путь файла". По нему вставка в другую
// заметку копирует файл в ее каталог
static const char *voiceMimeType="application/x-mytetra-voice";


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


const char *VoiceNoteHelper::voiceMimeTypeName(void)
{
    return voiceMimeType;
}


bool VoiceNoteHelper::hasVoiceLinks(QTextDocument *doc)
{
    if(doc==nullptr)
        return false;

    for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment=it.fragment();

            if(fragment.isValid() && isVoiceHref(fragment.charFormat().anchorHref()))
                return true;
        }

    return false;
}


int VoiceNoteHelper::materializeVoiceLinks(QTextDocument *doc)
{
    if(doc==nullptr)
        return 0;

    struct VoiceLinkRange
    {
        int start;
        int end;
        QString href;
        QString text;
    };

    QList<VoiceLinkRange> ranges;

    // Собираются все голосовые ссылки: идущие подряд фрагменты
    // с одним href склеиваются в один диапазон
    for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment=it.fragment();

            if(!fragment.isValid())
                continue;

            const QString href=fragment.charFormat().anchorHref();

            if(!isVoiceHref(href))
                continue;

            const int start=fragment.position();
            const int end=start+fragment.length();

            if(!ranges.isEmpty() && ranges.last().href==href && ranges.last().end==start)
            {
                ranges.last().end=end;
                ranges.last().text+=fragment.text();
            }
            else
                ranges.append({start, end, href, fragment.text()});
        }

    if(ranges.isEmpty())
        return 0;

    // Замена с конца чтобы позиции не съезжали
    QTextCursor cursor(doc);

    for(int i=ranges.size()-1; i>=0; --i)
    {
        const VoiceLinkRange range=ranges.at(i);
        const QString fileName=fileNameFromHref(range.href);
        qint64 durationMs=parseDuration(range.text);

        if(durationMs<0)
            durationMs=0;

        cursor.setPosition(range.start);
        cursor.setPosition(range.end, QTextCursor::KeepAnchor);
        cursor.insertText(QString(QChar::ObjectReplacementCharacter),
                          bubbleCharFormat(range.href, durationMs, fileName));
    }

    return ranges.size();
}


int VoiceNoteHelper::dematerializeBubbles(QTextDocument *doc)
{
    if(doc==nullptr)
        return 0;

    struct BubbleRange
    {
        int pos;
        QString href;
        qint64 durationMs;
    };

    QList<BubbleRange> ranges;

    for(QTextBlock block=doc->firstBlock(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment=it.fragment();

            if(!fragment.isValid())
                continue;

            const QTextCharFormat format=fragment.charFormat();

            if(!isBubbleFormat(format))
                continue;

            ranges.append({fragment.position(),
                           format.property(BubblePropHref).toString(),
                           format.property(BubblePropDurationMs).toLongLong()});
        }

    if(ranges.isEmpty())
        return 0;

    QTextCursor cursor(doc);

    for(int i=ranges.size()-1; i>=0; --i)
    {
        const BubbleRange range=ranges.at(i);
        const QString text=linkText(range.durationMs);

        cursor.setPosition(range.pos);
        cursor.setPosition(range.pos+1, QTextCursor::KeepAnchor);
        cursor.insertHtml(QStringLiteral("<a href=\"")+range.href.toHtmlEscaped()+
                          QStringLiteral("\">")+text.toHtmlEscaped()+
                          QStringLiteral("</a>"));
    }

    return ranges.size();
}

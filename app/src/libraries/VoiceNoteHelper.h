#ifndef _VOICENOTEHELPER_H_
#define _VOICENOTEHELPER_H_

#include <QString>
#include <QTextFormat>
#include <QTextCharFormat>

class QTextDocument;

// Общее для голосовых заметок: формат ссылки в тексте,
// запись живет файлом в каталоге записи (как картинки),
// клик по ссылке открывает плеер. Новые записи вставляются
// инлайн-баблом, в файле хранится та же ссылка voicenote://

class VoiceNoteHelper
{
public:

    // Ссылка вида "voicenote://voice-20261015-003000.opus"
    static QString makeHref(const QString &fileName);

    // Это голосовая ссылка (а не обычная)
    static bool isVoiceHref(const QString &href);

    // Имя файла из ссылки, пусто если ссылка не голосовая
    static QString fileNameFromHref(const QString &href);

    // Абсолютный путь файла: каталог записи + имя из ссылки.
    // Пусто если ссылка не голосовая. Абсолютный чтобы смена
    // рабочего каталога процесса ничего не ломала
    static QString resolveFilePath(const QString &workDir, const QString &href);

    // Длительность мс в "m:ss"
    static QString formatDuration(qint64 ms);

    // Длительность "m:ss" в мс, -1 если не разобрать
    static qint64 parseDuration(const QString &text);

    // Текст ссылки для вставки: "[voice 0:07]"
    static QString linkText(qint64 durationMs);

    // --- Инлайн-бабл ---

    // Тип текстового объекта бабла, свойства его формата
    enum
    {
        BubbleObjectType=QTextFormat::UserObject+1,
        BubblePropHref=QTextFormat::UserProperty+1,
        BubblePropDurationMs=QTextFormat::UserProperty+2,
        BubblePropFileName=QTextFormat::UserProperty+3
    };

    // Формат символа бабла: тип плюс свойства
    static QTextCharFormat bubbleCharFormat(const QString &href,
                                            qint64 durationMs,
                                            const QString &fileName);

    // Это формат бабла (а не обычный текст или картинка)
    static bool isBubbleFormat(const QTextCharFormat &format);

    // Ссылка бабла в позиции документа, пусто если там не бабл.
    // Проверяются символ под позицией и слева от нее
    static QString bubbleHrefAt(QTextDocument *doc, int pos);
};

#endif /* _VOICENOTEHELPER_H_ */

#ifndef _VOICENOTEHELPER_H_
#define _VOICENOTEHELPER_H_

#include <QString>

// Общее для голосовых заметок: формат ссылки в тексте,
// запись живет файлом в каталоге записи (как картинки),
// клик по ссылке открывает плеер

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

    // Текст ссылки для вставки: "[voice 0:07]"
    static QString linkText(qint64 durationMs);
};

#endif /* _VOICENOTEHELPER_H_ */

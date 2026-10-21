#ifndef _OPUSFILEREADER_H_
#define _OPUSFILEREADER_H_

#include <QString>
#include <ogg/ogg.h>

// Чтение .opus (Ogg-контейнер по RFC 7845) через вендоренные libogg+libopus.
// Пара к OpusFileWriter, но терпит и чужие файлы: моно/стерео, любая частота
// из поддерживаемых декодером. Выход всегда PCM 16 бит; частота выхода
// задается при открытии декодера (48 кГц), preskip из OpusHead отбрасывается.

class QFile;
struct OpusDecoder;

class OpusFileReader
{
public:

    OpusFileReader(void);
    virtual ~OpusFileReader(void);

    // Открыть файл: разбор заголовков, длительность сразу доступна.
    // outputRate 8000/12000/16000/24000/48000, обычно 48000
    bool open(const QString &filePath, int outputRate=48000);

    // Декодировать до maxFrames фреймов в out (моно/стерео по channels()).
    // Возвращает число фреймов, 0 в конце файла, -1 при ошибке чтения
    int readPcm(short *out, int maxFrames);

    // Перейти к позиции мс с начала (без preskip): переоткрытие потока
    // с пропуском. Для коротких голосовых дешевле честного сика
    bool seekMs(qint64 ms);

    int channels(void) const;
    int outputRate(void) const;

    // Полная длительность в мс (granulepos последней страницы минус preskip)
    qint64 durationMs(void) const;

    // Декодировано с момента open/seek, в мс
    qint64 positionMs(void) const;

    bool isOpen(void) const;

    QString errorString(void) const;

    void close(void);

private:

    // Инициализация Ogg-потока и пропуск двух заголовков.
    // Возвращает preskip из OpusHead или -1 при ошибке
    int openStream(void);

    // Выкачать следующий готовый пакет из файла в packet.
    // Возвращает 1 (пакет есть), 0 (конец файла), -1 (ошибка)
    int nextPacket(ogg_packet &packet);

    // Длительность сканированием granulepos последней страницы с конца
    qint64 scanDuration(void);

    QFile *file;
    OpusDecoder *decoder;
    ogg_sync_state syncState;
    ogg_stream_state streamState;
    bool streamInited;
    int streamSerial;

    int channelCount;
    int outRate;
    int preskip;

    // Остаток preskip, который еще надо отбросить из выхода декодера
    int preskipLeft;

    long long decodedFrames;
    qint64 totalMs;

    // Остаток кадра, не влезший в буфер вызывателя readPcm
    short spill[5760*2];
    int spillCount;

    QString filePath;
    QString errorText;
};

#endif /* _OPUSFILEREADER_H_ */

#ifndef _OPUSFILEWRITER_H_
#define _OPUSFILEWRITER_H_

#include <QString>
#include <ogg/ogg.h>

// Запись голосовой заметки в .opus (Ogg-контейнер по RFC 7845).
// Свой энкодер поверх вендоренных libopus+libogg: бэкенду Qt здесь
// делать нечего, поэтому работает везде, включая Wine и Windows 7.
// Параметры фиксированы под голос: 48 кГц наружу, моно, CBR 32 кбит.
// Входная частота любая из поддерживаемых энкодером (8/12/16/24/48 кГц),
// энкодер ресемплит сам, в OpusHead пишется исходная частота.

class QFile;
struct OpusEncoder;

class OpusFileWriter
{
public:

    OpusFileWriter(void);
    virtual ~OpusFileWriter(void);

    // Открыть файл на запись. Вход: частота дискретизации PCM (8000,
    // 12000, 16000, 24000 или 48000), всегда моно, 16 бит.
    // Возвращает false, текст причины в errorString()
    bool open(const QString &filePath, int inputSampleRate);

    // Дописать PCM-сэмплы (моно, 16 бит). Кадры режутся внутри по 20 мс,
    // остаток копится до следующего вызова или close()
    bool writePcm(const short *pcm, int sampleCount);

    // Уже записанная длительность в мс (без учета остатка в буфере)
    qint64 durationMs(void) const;

    // Дописать остаток (добив нулями до целого кадра), сбросить
    // страницы, закрыть файл. После close объект можно открывать заново
    bool close(void);

    bool isOpen(void) const;

    QString errorString(void) const;

private:

    // Заголовки Ogg Opus: OpusHead (19 байт) и OpusTags
    void writeHeaders(void);

    // Вытолкнуть готовые страницы из потока в файл
    bool flushPages(bool force);

    // Отправить готовый кадр в поток (счетчики, granulepos, по желанию EOS)
    bool submitFrame(const unsigned char *data, int len, bool eos);

    QFile *file;
    OpusEncoder *encoder;
    ogg_stream_state *streamState;

    // Накопленный необработанный PCM (добивка до кадра 20 мс)
    short pending[2880];
    int pendingCount;

    // Придержанный готовый кадр: EOS ставится на него в close(),
    // пустой финальный пакет не создается (PLC-шум в декодерах)
    unsigned char heldData[4000];
    int heldLen;
    bool heldValid;

    int inputRate;
    int frameSize;
    int preskip;

    long long totalFrames;

    // Декодированная шкала в единицах 48 кГц (granulepos и длительность)
    long long outputFrames;

    long long nextPacketNo;
    bool headersWritten;
    bool eosWritten;

    QString errorText;
};

#endif /* _OPUSFILEWRITER_H_ */

#ifndef _OPUSPLAYER_H_
#define _OPUSPLAYER_H_

#include <QObject>
#include <QAudio>

// Проигрыватель .opus через собственный декодер (OpusFileReader)
// и QAudioOutput. Нужен там, где бэкенд Qt opus не умеет:
// Windows, Wine, Windows 7. На Linux поведение то же, что у QMediaPlayer.

class OpusFileReader;
class QAudioOutput;
class QIODevice;

class OpusPlayer : public QObject
{
    Q_OBJECT

public:

    explicit OpusPlayer(QObject *parent=nullptr);
    virtual ~OpusPlayer(void);

    // Открыть файл. Длительность сразу доступна через durationMs()
    bool setFile(const QString &filePath);

    void play(void);
    void pause(void);
    void stop(void);

    // Перемотка в мс (переоткрытие с пропуском, для минутных файлов дешево)
    void setPosition(qint64 ms);

    qint64 positionMs(void) const;
    qint64 durationMs(void) const;

    bool isPlaying(void) const;

    // На паузе (приостановлен, позиция сохранена)
    bool isPaused(void) const;

signals:

    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);

    // Доиграло до конца файла само
    void finished(void);

    // Непоправимое: файл не открылся, формат не поддержан, звука нет
    void error(const QString &message);

private slots:

    void onNotify(void);
    void onStateChanged(QAudio::State state);

private:

    OpusFileReader *reader;
    QAudioOutput *audioOutput;
    QIODevice *feedDevice;

    qint64 totalMs;
    qint64 lastPosition;

    bool playing;
    bool endReached;
};

#endif /* _OPUSPLAYER_H_ */

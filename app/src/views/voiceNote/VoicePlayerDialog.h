#ifndef _VOICEPLAYERDIALOG_H_
#define _VOICEPLAYERDIALOG_H_

#include <QDialog>
#include <QMediaPlayer>

class QLabel;
class QPushButton;
class QSlider;
class OpusPlayer;

// Небольшой проигрыватель голосовой заметки: играть/пауза,
// ползунок позиции, время. Открывается кликом по ссылке [voice]
class VoicePlayerDialog : public QDialog
{
    Q_OBJECT

public:

    explicit VoicePlayerDialog(const QString &filePath, const QString &title,
                               QWidget *parent=nullptr);

private slots:

    void onPlayToggled(bool checked);
    void onPositionChanged(qint64 positionMs);
    void onDurationChanged(qint64 durationMs);
    void onStateChanged(QMediaPlayer::State state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error);
    void onSliderMoved(int positionMs);

    // Финал собственного Opus-проигрывателя
    void onOpusFinished(void);

private:

    static QString formatTime(qint64 ms, qint64 totalMs);

    QMediaPlayer *player;

    // Собственный Opus-проигрыватель для .opus: бэкенд Qt opus
    // на Windows/Wine не умеет, здесь декодер свой
    OpusPlayer *opusPlayer;
    bool useOpusPlayer;

    QLabel *titleLabel;
    QPushButton *playButton;
    QSlider *positionSlider;
    QLabel *timeLabel;
    QLabel *statusLabel;
    QPushButton *closeButton;

    qint64 totalMs;
};

#endif /* _VOICEPLAYERDIALOG_H_ */

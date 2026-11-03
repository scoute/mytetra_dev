#ifndef _VOICEPLAYERDIALOG_H_
#define _VOICEPLAYERDIALOG_H_

#include <QDialog>
#include <QList>
#include <QMediaPlayer>

class QLabel;
class QPushButton;
class QSlider;
class OpusPlayer;
class MtComboBox;

// Проигрыватель голосовой заметки: играть/пауза, скорость 0.5x..2x,
// ползунок, время. Скорость работает только на бэкенде Qt (MP3):
// у QMediaPlayer нет темпа с компенсацией тона, а собственный
// Opus-декодер (нужен для Wine) темпа не имеет вовсе. Для opus
// выпадашка заблокирована. Открывается кликом по ссылке [voice]
class VoicePlayerDialog : public QDialog
{
    Q_OBJECT

public:

    explicit VoicePlayerDialog(const QString &filePath, const QString &title,
                               QWidget *parent=nullptr);

    // Ступени выпадашки: 0.5..2.0 шаг 0.25
    static QList<double> speedSteps(void);

    // Ближайшая ступень к значению
    static double snapSpeed(double speed);

private slots:

    void onPlayToggled(bool checked);
    void onSpeedChanged(int index);
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
    QLabel *speedLabel;
    MtComboBox *speedCombo;
    QSlider *positionSlider;
    QLabel *timeLabel;
    QLabel *statusLabel;
    QPushButton *closeButton;

    // Скорость из выпадашки (дефолт из конфига), применяется
    // к бэкенду Qt; opus всегда 1x
    double currentSpeed=1.0;

    qint64 totalMs;
};

#endif /* _VOICEPLAYERDIALOG_H_ */

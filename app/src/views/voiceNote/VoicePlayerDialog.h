#ifndef _VOICEPLAYERDIALOG_H_
#define _VOICEPLAYERDIALOG_H_

#include <QDialog>

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QTimer;

// Проигрыватель голосовой заметки: играть/пауза, скорость
// 0.5x..2x с сохранением тона, ползунок, время. Открывается
// кликом по ссылке [voice]. Свой gstreamer-пайплайн вместо
// QMediaPlayer осознанно: у Qt нет темпа с компенсацией.
// GStreamer дергается через QLibrary (заголовков gst нет,
// чтобы не тащить libgstreamer-dev в сборку): резолвятся
// только стабильные ABI-функции 1.x. Нет tempo-элемента —
// обычное проигрывание, выпадашка гаснет
class VoicePlayerDialog : public QDialog
{
    Q_OBJECT

public:

    // sinkOverride только для стенда ("fakesink"): в проде
    // всегда autoaudiosink
    explicit VoicePlayerDialog(const QString &filePath, const QString &title,
                               QWidget *parent=nullptr,
                               const QString &sinkOverride=QString());
    virtual ~VoicePlayerDialog(void);

    // Ступени выпадашки: 0.5..2.0 шаг 0.25
    static QList<double> speedSteps(void);

    // Ближайшая ступень к значению
    static double snapSpeed(double speed);

private slots:

    void onPlayToggled(bool checked);
    void onSpeedChanged(int index);
    void onSliderMoved(int positionMs);
    void onTick(void);

    // Финал проигрывания и фатальная ошибка шины
    void onEosReached(void);
    void onPlaybackError(void);

private:

    static QString formatTime(qint64 ms, qint64 totalMs);

    // Пайплайн filesrc ! decodebin ! audioconvert ! audioresample
    // ! tempo ! sink. tempo это pitch (тон фиксирован) или
    // scaletempo, без обоих элемент пропускается
    void buildPipeline(void);
    void closePipeline(void);

    // Текущая скорость в tempo-элемент. Нет элемента: нечего делать
    void applySpeed(void);

    // Разобрать очередь шины: EOS и ошибки. Остальное мимо
    void pollBus(void);

    // Позиция и длительность в мс, -1 если не спрашивается
    qint64 queryPositionMs(void) const;
    qint64 queryDurationMs(void) const;

    QLabel *titleLabel;
    QPushButton *playButton;
    QComboBox *speedCombo;
    QSlider *positionSlider;
    QLabel *timeLabel;
    QLabel *statusLabel;
    QPushButton *closeButton;
    QTimer *tick;

    QString sinkName;
    QString fileAbsPath;
    double currentSpeed;

    void *gstPipeline=nullptr; // GstElement*, void чтобы без заголовков
    void *gstTempo=nullptr;    // GstElement* pitch/scaletempo или null
    void *gstBus=nullptr;      // GstBus*, живет с пайплайном

    bool eosReached=false;
    qint64 positionMs=0;
    qint64 totalMs=0;
};

#endif /* _VOICEPLAYERDIALOG_H_ */

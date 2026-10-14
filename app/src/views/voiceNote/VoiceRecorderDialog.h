#ifndef _VOICERECORDERDIALOG_H_
#define _VOICERECORDERDIALOG_H_

#include <QDialog>
#include <QWidget>
#include <QElapsedTimer>
#include <QMediaRecorder>

class QLabel;
class QPushButton;
class QComboBox;
class QRadioButton;
class QLineEdit;
class QTimer;
class QAudioRecorder;
class QAudioProbe;
class QAudioBuffer;

// Полоса звуковой волны с микрофона. Хранит последние пики,
// рисует столбики от центра. Для теста видны счетчик и максимум
class WaveformWidget : public QWidget
{
    Q_OBJECT

public:

    explicit WaveformWidget(QWidget *parent=nullptr);

    // Уровень 0..1 за текущий кадр буфера
    void pushLevel(qreal level);
    void clearLevels(void);

    int levelCount(void) const;
    qreal maxLevel(void) const;

protected:

    void paintEvent(QPaintEvent *event) override;

private:

    QList<qreal> levels;

    static const int maxLevels=120;
};


// Небольшое окно записи: формат Opus/MP3 на выбор, волна
// с микрофона, таймер, запись/стоп, вставка/отмена. Файл
// сразу в каталоге записи
class VoiceRecorderDialog : public QDialog
{
    Q_OBJECT

public:

    explicit VoiceRecorderDialog(const QString &workDir, QWidget *parent=nullptr);
    virtual ~VoiceRecorderDialog(void);

    // Путь записанного файла и длительность, валидны при Accepted
    QString recordedFilePath(void) const;
    qint64 recordedDurationMs(void) const;

private slots:

    void onFormatRadio(bool checked);
    void onQualityRadio(bool checked);
    void onFileNameEdited(const QString &text);
    void onRecordToggled(bool checked);
    void onDeviceChanged(int index);
    void onStateChanged(QMediaRecorder::State state);
    void onRecorderError(QMediaRecorder::Error error);
    void onProbeBuffer(const QAudioBuffer &buffer);
    void onTick(void);
    void onInsert(void);

private:

    // Формат записи, выбирается из поддерживаемых бэкендом
    enum VoiceFormat
    {
        VoiceFormatOpus,
        VoiceFormatMp3,
        VoiceFormatWavFallback
    };

    // Качество записи low/middle/high, средний по умолчанию
    enum VoiceQuality
    {
        VoiceQualityLow,
        VoiceQualityMedium,
        VoiceQualityHigh
    };

    // Завершить запись и решить про вставку. Вызывается из стопа
    // и из смены состояния: идемпотентно
    void finishRecording(void);

    // Применить выбранный формат: контейнер, кодек, частота.
    // Вызывается перед стартом записи
    void applyFormat(VoiceFormat format);

    // Обновить подсказку с параметрами выбранного качества
    void updateQualityHint(void);

    // Расширение текущего формата
    QString currentExtension(void) const;

    // Метка времени для предложенного имени
    QString autoStem(void) const;

    // Свободное имя в каталоге записи: основа плюс расширение,
    // при занятости дописывается -2, -3 и так далее
    QString uniqueFileName(const QString &stem, const QString &extension) const;

    // Имя из строки пользователя: без путей и запретных символов,
    // с расширением текущего формата
    QString userFileName(void) const;

    // Пик первого канала буфера 0..1
    static qreal bufferPeak(const QAudioBuffer &buffer);

    QString workDir;

    QLabel *statusLabel;
    WaveformWidget *waveform;
    QLabel *timeLabel;
    QRadioButton *opusRadio;
    QRadioButton *mp3Radio;
    QRadioButton *qualityLowRadio;
    QRadioButton *qualityMediumRadio;
    QRadioButton *qualityHighRadio;
    QLabel *qualityHintLabel;
    QLineEdit *fileNameEdit;
    QComboBox *deviceCombo;
    QPushButton *recordButton;
    QPushButton *insertButton;
    QPushButton *cancelButton;

    QAudioRecorder *recorder;
    QAudioProbe *probe;
    QTimer *timer;

    VoiceFormat currentFormat;
    VoiceQuality currentQuality;
    bool opusAvailable;
    bool mp3Available;
    bool wavAvailable;

    // Пользователь правил имя руками: расширение при смене
    // формата меняется, основа сохраняется
    bool fileNameEdited;

    // Длительность меряется своим секундомером: duration() бэкенда
    // на части систем ноль при живом файле записи
    QElapsedTimer recordElapsed;
    bool elapsedRunning;
    qint64 durationMs;
    bool hasRecording;

    QString filePath;
};

#endif /* _VOICERECORDERDIALOG_H_ */

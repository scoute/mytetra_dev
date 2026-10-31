#ifndef _VOICERECORDERDIALOG_H_
#define _VOICERECORDERDIALOG_H_

#include <QDialog>
#include <QWidget>
#include <QElapsedTimer>
#include <QAudio>

#include "libraries/OpusFileWriter.h"
#include "libraries/SpectralNoiseSuppress.h"
#include <QMediaRecorder>
#include <QAudioFormat>
#include <QAudioDeviceInfo>

class QLabel;
class QPushButton;
class MtComboBox;
class QRadioButton;
class QLineEdit;
class QTimer;
class QAudioRecorder;
class QAudioProbe;
class QAudioBuffer;
class QAudioInput;
class QIODevice;

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
    void onFileNameEdited(const QString &text);
    void onRecordToggled(bool checked);
    void onDeviceChanged(int index);
    void onNoiseSuppressToggled(bool checked);
    void onOpusQualityChanged(int index);
    void onStateChanged(QMediaRecorder::State state);
    void onRecorderError(QMediaRecorder::Error error);
    void onProbeBuffer(const QAudioBuffer &buffer);

    // Данные собственного Opus-захвата: тянутся из QAudioInput,
    // волна рисуется из того же потока, проба не нужна
    void onOpusAudioData(void);
    void onOpusStateChanged(QAudio::State state);
    void onTick(void);
    void onInsert(void);

private:

    // Формат записи, выбирается из поддерживаемых бэкендом
    enum VoiceFormat
    {
        VoiceFormatOpus,
        VoiceFormatMp3
    };

    // Один профиль на формат: Opus 48 кГц/CBR по выбору качества,
    // MP3 32 кГц фикс (битрейт lame бэкенд игнорит,
    // upstream catch-22 Qt 5.15)

    // Битрейт Opus по индексу качества 0..3 (24/32/48/64 кбит).
    // Статика чтобы стенд проверял параметры без микрофона и бэкенда
    static int opusBitrateForQuality(int quality);

    // Профиль формата: VBR-качество, целевой битрейт
    // и частота. Opus CBR по выбору качества (лесенка держится),
    // MP3 битрейт 0 = не выставлять (все равно игнорит)
    struct VoiceQualityProfile
    {
        QMultimedia::EncodingQuality quality;
        int bitRate;
        int sampleRate;
    };

    // Режим кодирования общий: CBR держит битрейт Opus
    static QMultimedia::EncodingMode encodingMode(void);

    // Профиль формата. Статика чтобы стенд проверял
    // параметры без микрофона и бэкенда
    static VoiceQualityProfile qualityProfile(VoiceFormat format);

    // Завершить запись и решить про вставку. Вызывается из стопа
    // и из смены состояния: идемпотентно
    void finishRecording(void);

    // Текст кнопки записи зависит от наличия файла: повторный дубль
    // перезатирает предыдущий, кнопка так и говорит
    void updateRecordButton(void);

    // Применить выбранный формат: контейнер, кодек, частота.
    // Вызывается перед стартом записи
    void applyFormat(VoiceFormat format);

    // Подпись фиксированных параметров записи
    void updateFormatInfo(void);

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

    // Выбранное в комбобоксе устройство входа (пустое если список пуст)
    QAudioDeviceInfo currentInputDevice(void) const;

    // Старт/стоп собственного Opus-захвата. Старт возвращает false
    // и пишет причину в statusLabel, файл уже должен быть выбран
    bool startOpusCapture(const QString &filePath);
    void stopOpusCapture(void);

    // Прогрев входа: вход стримит непрерывно, пока открыто окно.
    // Мусор аналогового пробуждения оседает до нажатия записи,
    // оценки шума калибруются реальным фоном. stopMonitor() свободен
    // и для полного освобождения входа старого пути
    void startMonitor(void);
    void stopMonitor(void);

    // Договаривание формата записи с устройством и открытие самого
    // входа: общая часть монитора прогрева и старого пути (файл
    // не открывает). false - нет устройства/формата или вход занят,
    // причины в statusLabel; причина занятости через флаг
    bool openOpusInput(bool reportStartError);

    // Подавление фонового шума: пересчет окон преобразования
    // очистителя под частоту записи, сброс оценок шума
    void setupNoiseChain(int sampleRate);

    // Доступность галочки подавления: включена только для Opus,
    // MP3-запись без подавления, поэтому блокируется
    void updateNoiseSuppressAvailable(void);

    // Доступность выбора качества: только для Opus и вне записи,
    // битрейт MP3 задает бэкенд. Вызывается из смены формата,
    // старта/стопа и инициализации
    void updateOpusQualityAvailable(void);

    // Пропуск чанка через спектральное подавление шума (фон
    // глушится и в паузах, и под речью; уровень речи не трогается;
    // пол калибруется потоком прогрева и записи)
    void applyNoiseSuppress(short *pcm, int samples);

    QString workDir;

    QLabel *statusLabel;
    WaveformWidget *waveform;
    QLabel *timeLabel;
    QRadioButton *opusRadio;
    QRadioButton *mp3Radio;
    QLabel *formatInfoLabel;
    QLabel *opusQualityLabel;
    MtComboBox *opusQuality;
    QLineEdit *fileNameEdit;
    MtComboBox *deviceCombo;
    QPushButton *recordButton;
    QPushButton *insertButton;
    QPushButton *cancelButton;

    QAudioRecorder *recorder;
    QAudioProbe *probe;

    // Собственный Opus-трек: доступен всегда, бэкенду Qt здесь делать
    // нечего, поэтому работает и под Wine. MP3-радиокнопка по-прежнему
    // идет через бэкенд (где он умеет)
    QAudioInput *audioInput;
    QIODevice *opusDevice;
    QAudioFormat opusFormat;
    OpusFileWriter opusWriter;

    // Подавление фонового шума: галочка и живой флаг ее состояния
    QCheckBox *noiseSuppress;
    bool noiseSuppressOn;

    // Спектральный очиститель фона: объект со своим состоянием
    // между чанками (оценки пола, гейны и окна потока)
    SpectralNoiseSuppress noiseSuppressor;

    QTimer *timer;

    VoiceFormat currentFormat;
    bool opusAvailable;
    bool mp3Available;

    // Пользователь правил имя руками: расширение при смене
    // формата меняется, основа сохраняется
    bool fileNameEdited;

    // Длительность меряется своим секундомером: duration() бэкенда
    // на части систем ноль при живом файле записи
    QElapsedTimer recordElapsed;
    bool elapsedRunning;
    qint64 durationMs;
    bool hasRecording;

    // Первые батчи аналогового микрофона после паузы бывают
    // мусорные, сколько уже пропущено
    int warmupDroppedMs;

    // Прогрев входа: чанки всегда кормят волну и модель шума,
    // но в файл попадают только с нажатой записью
    bool micMonitoring;

    // Фильтр постоянной составляющей: мусор прогрева входа -
    // это DC. Состояние одноэлементное, переносится между чанками
    bool dcBlockInit;
    double dcBlockPrevIn;
    double dcBlockPrevOut;

    QString filePath;
};

#endif /* _VOICERECORDERDIALOG_H_ */

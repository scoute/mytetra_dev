#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include "libraries/MtComboBox.h"
#include <QLineEdit>
#include <QTimer>
#include <QPainter>
#include <QPaintEvent>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QAudioRecorder>
#include <QAudioProbe>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QAudioEncoderSettings>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QtMath>
#include <QElapsedTimer>
#include <QRegularExpression>

#include "VoiceRecorderDialog.h"
#include "libraries/VoiceNoteHelper.h"
#include "models/appConfig/AppConfig.h"


extern AppConfig mytetraConfig;


// Сколько мусора прогрева аналогового входа можно пропустить за
// одну запись (мс). Потолок на случай, если за мусор принят
// громкий реальный звук - падение записи всё равно не бесконечно
static const int WARMUP_MAX_DROP_MS=3000;

// Срез DC-blocker в герцах. Мусор прогрева аналогового (HDA)
// микрофона - это конденсатор смещения: постоянное смещение с
// затухающим низкочастотным хвостом. Ниже фундаментальной частоты
// мужского голоса (~85 Гц) речи нет, так что срез безопасен
static const double DC_BLOCK_CUTOFF_HZ=40.0;


// Батч мусора прогрева в насыщении (когда весь батч на пределе,
// как при неподвижном DC) не пишется вовсе. Широкополосный
// числовой мусор так же гасится на всякий случай
static bool isSaturatedChunk(const short *pcm, int samples)
{
    if(samples<480) // короче 10 мс статистика не работает
        return false;

    int saturated=0;

    for(int i=0; i<samples; ++i)
        if(qAbs(pcm[i])>=32000)
            ++saturated;

    return static_cast<qreal>(saturated)/samples>=0.2;
}


WaveformWidget::WaveformWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(260, 80);
}


void WaveformWidget::pushLevel(qreal level)
{
    levels.append(qBound(0.0, level, 1.0));

    while(levels.size()>maxLevels)
        levels.removeFirst();

    update();
}


void WaveformWidget::clearLevels(void)
{
    levels.clear();
    update();
}


int WaveformWidget::levelCount(void) const
{
    return levels.size();
}


qreal WaveformWidget::maxLevel(void) const
{
    qreal peak=0.0;

    for(qreal level : levels)
        peak=qMax(peak, level);

    return peak;
}


void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));

    if(levels.isEmpty())
        return;

    painter.setPen(palette().color(QPalette::Highlight));

    const int middle=height()/2;
    const int barWidth=qMax(1, width()/maxLevels);

    for(int i=0; i<levels.size(); ++i)
    {
        const int barHeight=static_cast<int>(levels.at(i)*(height()-4));
        const int x=width()-(levels.size()-i)*barWidth;
        painter.drawLine(x, middle-barHeight/2, x, middle+barHeight/2);
    }
}


VoiceRecorderDialog::VoiceRecorderDialog(const QString &workDir, QWidget *parent) : QDialog(parent),
    workDir(workDir),
    audioInput(nullptr),
    opusDevice(nullptr),
    currentFormat(VoiceFormatOpus),
    opusAvailable(false),
    mp3Available(false),
    fileNameEdited(false),
    elapsedRunning(false),
    durationMs(0),
    hasRecording(false),

    // Прогрев аналогового входа: счётчик мусора и чистое
    // стартовое состояние DC-blocker (флаг, что первый чанк уже
    // использован как точка отсчета)
    warmupDroppedMs(0),

    // Прогрев входа: запускается в конце конструктора, когда
    // комбобокс устройств уже собран
    micMonitoring(false),

    dcBlockInit(false),
    dcBlockPrevIn(0.0),
    dcBlockPrevOut(0.0),

    // Подавление фона: флаг фактической галочки
    noiseSuppressOn(false)
{
    setWindowTitle(tr("Voice note"));
    setModal(true);

    statusLabel=new QLabel(tr("Ready"), this);
    waveform=new WaveformWidget(this);
    timeLabel=new QLabel(QStringLiteral("0:00"), this);

    QLabel *formatLabel=new QLabel(tr("Format:"), this);
    opusRadio=new QRadioButton(tr("Opus (.opus)"), this);
    mp3Radio=new QRadioButton(tr("MP3 (.mp3)"), this);
    noiseSuppress=new QCheckBox(tr("Suppress noise"), this);

    // Подавление фона по галочке: период, на котором говорят,
    // записывается с цепочкой фильтров и гейтом. Настройка
    // хранится в конфиге, включено по умолчанию
    noiseSuppress->setChecked(mytetraConfig.get_voiceNoteNoiseSuppress());
    noiseSuppressOn=noiseSuppress->isChecked();

    // Свои группы: иначе autoExclusive сиблингов сбрасывает
    // выбор формата при выборе качества и наоборот
    QButtonGroup *formatGroup=new QButtonGroup(this);
    formatGroup->addButton(opusRadio);
    formatGroup->addButton(mp3Radio);

    QLabel *fileNameLabel=new QLabel(tr("File name:"), this);
    fileNameEdit=new QLineEdit(this);
    fileNameEdit->setClearButtonEnabled(true);

    // Профиль один на формат, выбора нет.
    // Метка только информирует о параметрах
    formatInfoLabel=new QLabel(this);

    QLabel *deviceLabel=new QLabel(tr("Microphone:"), this);
    deviceCombo=new MtComboBox(this);

    recordButton=new QPushButton(tr("Record"), this);
    recordButton->setCheckable(true);

    insertButton=new QPushButton(tr("Insert"), this);
    insertButton->setDefault(true);
    insertButton->setEnabled(false);

    cancelButton=new QPushButton(tr("Cancel"), this);

    QHBoxLayout *buttonsLayout=new QHBoxLayout();
    buttonsLayout->addWidget(recordButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(insertButton);
    buttonsLayout->addWidget(cancelButton);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addWidget(statusLabel);
    centralLayout->addWidget(waveform, 1);
    centralLayout->addWidget(timeLabel);

    QHBoxLayout *formatLayout=new QHBoxLayout();
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(opusRadio);
    formatLayout->addWidget(mp3Radio);
    formatLayout->addStretch();
    formatLayout->addWidget(noiseSuppress);
    centralLayout->addLayout(formatLayout);

    QHBoxLayout *fileNameLayout=new QHBoxLayout();
    fileNameLayout->addWidget(fileNameLabel);
    fileNameLayout->addWidget(fileNameEdit, 1);
    centralLayout->addLayout(fileNameLayout);

    centralLayout->addWidget(formatInfoLabel);

    QHBoxLayout *deviceLayout=new QHBoxLayout();
    deviceLayout->addWidget(deviceLabel);
    deviceLayout->addWidget(deviceCombo, 1);
    centralLayout->addLayout(deviceLayout);

    centralLayout->addLayout(buttonsLayout);
    setLayout(centralLayout);

    recorder=new QAudioRecorder(this);

    // Формат доступен если есть и кодек, и контейнер. Строки
    // gstreamer-бэкенда, на других бэкендах имена могут отличаться,
    // поэтому и то и другое проверяется по факту, а не предполагается
    const QStringList supportedContainers=recorder->supportedContainers();
    const QStringList supportedCodecs=recorder->supportedAudioCodecs();

    bool opusCodecOk=false;
    bool opusContainerOk=false;

    for(const QString &codec : supportedCodecs)
    {
        if(codec.contains(QStringLiteral("opus")))
            opusCodecOk=true;
    }

    for(const QString &container : supportedContainers)
    {
        if(container.contains(QStringLiteral("ogg")))
            opusContainerOk=true;
    }

    // Собственный Opus-энкодер доступен всегда, бэкенд не нужен.
    // Переменные opusCodecOk/opusContainerOk оставлены для диагностики выше
    opusAvailable=true;
    Q_UNUSED(opusCodecOk);
    Q_UNUSED(opusContainerOk);

    mp3Available=
        (supportedContainers.contains(QStringLiteral("audio/mpeg, mpegversion=(int)1")) ||
         supportedContainers.contains(QStringLiteral("audio/mpeg"))) &&
        supportedCodecs.contains(QStringLiteral("audio/mpeg, mpegversion=(int)1, layer=(int)3"));

    // Диагностика для подбора форматов: что умеет бэкенд
    qDebug() << "VoiceRecorder: default input" << recorder->audioInput();
    qDebug() << "VoiceRecorder: supported containers" << supportedContainers;
    qDebug() << "VoiceRecorder: supported audio codecs" << supportedCodecs;

    opusRadio->setEnabled(opusAvailable);
    mp3Radio->setEnabled(mp3Available);

    // Предвыбор из настроек, недоступный бэкенду сбрасывается.
    // По умолчанию Opus
    const QString defaultFormat=mytetraConfig.get_voiceNoteFormat();

    if(defaultFormat==QStringLiteral("mp3") && mp3Available)
    {
        mp3Radio->setChecked(true);
        currentFormat=VoiceFormatMp3;
    }
    else if(opusAvailable)
    {
        opusRadio->setChecked(true);
        currentFormat=VoiceFormatOpus;
    }
    else if(mp3Available)
    {
        mp3Radio->setChecked(true);
        currentFormat=VoiceFormatMp3;
    }

    // Галочка подавления следует за форматом
    updateNoiseSuppressAvailable();

    // Предложенное имя: метка времени плюс расширение формата
    fileNameEdit->setText(autoStem()+currentExtension());

    updateFormatInfo();

    probe=new QAudioProbe(this);
    probe->setSource(recorder);

    timer=new QTimer(this);
    timer->setInterval(100);

    connect(opusRadio, &QRadioButton::toggled,
            this,       &VoiceRecorderDialog::onFormatRadio);
    connect(mp3Radio, &QRadioButton::toggled,
            this,      &VoiceRecorderDialog::onFormatRadio);
    connect(fileNameEdit, &QLineEdit::textEdited,
            this,          &VoiceRecorderDialog::onFileNameEdited);
    connect(recordButton, &QPushButton::toggled,
            this,          &VoiceRecorderDialog::onRecordToggled);
    connect(deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,        &VoiceRecorderDialog::onDeviceChanged);
    connect(noiseSuppress, &QCheckBox::toggled,
            this,          &VoiceRecorderDialog::onNoiseSuppressToggled);
    connect(recorder, QOverload<QMediaRecorder::Error>::of(&QAudioRecorder::error),
            this,     &VoiceRecorderDialog::onRecorderError);
    connect(recorder, &QAudioRecorder::stateChanged,
            this,     &VoiceRecorderDialog::onStateChanged);
    connect(probe, &QAudioProbe::audioBufferProbed,
            this,  &VoiceRecorderDialog::onProbeBuffer);
    connect(timer, &QTimer::timeout,
            this,  &VoiceRecorderDialog::onTick);
    connect(insertButton, &QPushButton::clicked,
            this,          &VoiceRecorderDialog::onInsert);
    connect(cancelButton, &QPushButton::clicked,
            this,          &VoiceRecorderDialog::reject);

    // Устройства входа. Выбранный пишет через setAudioInput.
    // Предвыбран системный дефолт, если он есть в списке
    const QList<QAudioDeviceInfo> devices=
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for(const QAudioDeviceInfo &device : devices)
        deviceCombo->addItem(device.deviceName());

    const int defaultIndex=deviceCombo->findText(
        QAudioDeviceInfo::defaultInputDevice().deviceName());

    // Предвыбор без сигнала смены: сработавший onDeviceChanged
    // открыл бы вход при построении окна, а монитор запускается
    // один раз явно, в конце конструктора
    deviceCombo->blockSignals(true);
    deviceCombo->setCurrentIndex(defaultIndex>=0 ? defaultIndex : 0);
    deviceCombo->blockSignals(false);

    // Явный выбор для бэкенда MP3: сигнал смены выше подавлен
    if(!deviceCombo->currentText().isEmpty())
        recorder->setAudioInput(deviceCombo->currentText());

    if(devices.isEmpty())
    {
        recordButton->setEnabled(false);
        statusLabel->setText(tr("Microphone not found"));
    }

    // Без аудио-бэкенда (нет gstreamer-плагинов) тоже записывать нечего
    if(recorder->availability()!=QMultimedia::Available)
    {
        recordButton->setEnabled(false);

        if(devices.isEmpty())
            statusLabel->setText(tr("Microphone not found"));
        else
            statusLabel->setText(tr("Audio backend unavailable"));
    }

    if(!opusAvailable && !mp3Available &&
       recorder->availability()==QMultimedia::Available && !devices.isEmpty())
    {
        recordButton->setEnabled(false);
        statusLabel->setText(tr("No supported audio format"));
    }

    // Прогрев: вход открывается сразу при появлении окна. К нажатию
    // записи мусор аналогового пробуждения давно осел, а модель
    // шума откалибрована реальным фоном. Волна показывает живой
    // уровень микрофона
    if(!devices.isEmpty())
        startMonitor();
}


VoiceRecorderDialog::~VoiceRecorderDialog(void)
{
    // Остановка незавершенной записи: иначе вход продолжает писать
    // в файл который ниже удаляется, а на Windows открытый файл
    // вообще не удалить
    stopOpusCapture();

    // Прогрев держал вход живым для следующего дубля: при закрытии
    // окна он освобождается явно, до удаления объекта
    stopMonitor();

    if(recorder->state()==QMediaRecorder::RecordingState)
        recorder->stop();

    // Не вставленное не храним: файл диалога это мусор.
    // При вставке (Accepted) файл забирает заметка, не трогаем
    if(result()!=QDialog::Accepted && QFileInfo(filePath).isFile())
        QFile::remove(filePath);
}


QString VoiceRecorderDialog::recordedFilePath(void) const
{
    return filePath;
}


qint64 VoiceRecorderDialog::recordedDurationMs(void) const
{
    return durationMs;
}


QMultimedia::EncodingMode VoiceRecorderDialog::encodingMode(void)
{
    return QMultimedia::ConstantBitRateEncoding;
}


VoiceRecorderDialog::VoiceQualityProfile VoiceRecorderDialog::qualityProfile(VoiceFormat format)
{
    // Один профиль на формат: Opus 48 кГц/24 кбит CBR,
    // MP3 32 кГц фикс (битрейт lame игнорит, catch-22 Qt 5.15)
    if(format==VoiceFormatMp3)
        return {QMultimedia::NormalQuality, 0, 32000};

    return {QMultimedia::NormalQuality, 24000, 48000};
}


void VoiceRecorderDialog::applyFormat(VoiceFormat format)
{
    QAudioEncoderSettings audioSettings;
    audioSettings.setChannelCount(1);

    // VBR осознанно: переменный битрейт лучше держит качество
    // голоса при том же размере. Битрейт-профиль идет таргетом
    const VoiceQualityProfile profile=qualityProfile(format);

    audioSettings.setEncodingMode(encodingMode());
    audioSettings.setQuality(profile.quality);

    if(profile.bitRate>0)
        audioSettings.setBitRate(profile.bitRate);

    audioSettings.setSampleRate(profile.sampleRate);

    if(format==VoiceFormatOpus)
    {
        for(const QString &codec : recorder->supportedAudioCodecs())
            if(codec.contains(QStringLiteral("opus")))
            {
                audioSettings.setCodec(codec);
                break;
            }

        for(const QString &container : recorder->supportedContainers())
            if(container.contains(QStringLiteral("ogg")))
            {
                recorder->setContainerFormat(container);
                break;
            }
    }
    else if(format==VoiceFormatMp3)
    {
        audioSettings.setCodec(
            QStringLiteral("audio/mpeg, mpegversion=(int)1, layer=(int)3"));

        if(recorder->supportedContainers().contains(
               QStringLiteral("audio/mpeg, mpegversion=(int)1")))
            recorder->setContainerFormat(
                QStringLiteral("audio/mpeg, mpegversion=(int)1"));
        else
            recorder->setContainerFormat(QStringLiteral("audio/mpeg"));
    }

    recorder->setAudioSettings(audioSettings);
}


void VoiceRecorderDialog::updateFormatInfo(void)
{
    const VoiceQualityProfile profile=qualityProfile(currentFormat);

    // MP3 без килобит осознанно: выход фиксированный,
    // цифры врали бы
    if(currentFormat==VoiceFormatMp3)
        formatInfoLabel->setText(
            tr("MP3, %1 Hz").arg(profile.sampleRate));
    else
        formatInfoLabel->setText(
            tr("Opus, %1 Hz, %2 kbit/s").arg(profile.sampleRate).arg(profile.bitRate/1000));
}


QString VoiceRecorderDialog::currentExtension(void) const
{
    if(currentFormat==VoiceFormatMp3)
        return QStringLiteral(".mp3");

    return QStringLiteral(".opus");
}


QString VoiceRecorderDialog::autoStem(void) const
{
    return QStringLiteral("voice-")+
           QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
}


QString VoiceRecorderDialog::uniqueFileName(const QString &stem, const QString &extension) const
{
    for(int attempt=0; attempt<100; ++attempt)
    {
        QString name=stem;

        if(attempt>0)
            name+=QStringLiteral("-")+QString::number(attempt);

        name+=extension;

        if(!QFileInfo(QDir(workDir).absoluteFilePath(name)).exists())
            return name;
    }

    return QString();
}


QString VoiceRecorderDialog::userFileName(void) const
{
    // Без путей: только имя файла
    QString text=QFileInfo(fileNameEdit->text().trimmed()).fileName().trimmed();

    // Запретные для файловых систем символы
    text.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]")));
    text=text.trimmed();

    // Чужая отсебятина с расширением не проходит: расширение
    // всегда от текущего формата
    static const QRegularExpression knownExt(
        QStringLiteral("\\.(opus|mp3|wav|oga|ogg)$"),
        QRegularExpression::CaseInsensitiveOption);

    text.remove(knownExt);

    QString stem=text.trimmed().left(100);

    if(stem.isEmpty())
        stem=autoStem();

    return uniqueFileName(stem, currentExtension());
}


qreal VoiceRecorderDialog::bufferPeak(const QAudioBuffer &buffer)
{
    const QAudioFormat format=buffer.format();
    const int channels=qMax(1, format.channelCount());
    const int frames=buffer.frameCount();

    if(frames<=0)
        return 0.0;

    qreal peak=0.0;

    if(format.sampleType()==QAudioFormat::SignedInt)
    {
        if(format.sampleSize()==16)
        {
            const qint16 *data=buffer.constData<qint16>();
            for(int i=0; i<frames; ++i)
                peak=qMax(peak, qAbs(static_cast<qreal>(data[i*channels]))/32768.0);
        }
        else if(format.sampleSize()==8)
        {
            const qint8 *data=buffer.constData<qint8>();
            for(int i=0; i<frames; ++i)
                peak=qMax(peak, qAbs(static_cast<qreal>(data[i*channels]))/128.0);
        }
        else if(format.sampleSize()==32)
        {
            const qint32 *data=buffer.constData<qint32>();
            for(int i=0; i<frames; ++i)
                peak=qMax(peak, qAbs(static_cast<qreal>(data[i*channels]))/2147483648.0);
        }
    }
    else if(format.sampleType()==QAudioFormat::UnSignedInt)
    {
        if(format.sampleSize()==8)
        {
            const quint8 *data=buffer.constData<quint8>();
            for(int i=0; i<frames; ++i)
                peak=qMax(peak, qAbs(static_cast<qreal>(data[i*channels])-128.0)/128.0);
        }
    }
    else if(format.sampleType()==QAudioFormat::Float)
    {
        if(format.sampleSize()==32)
        {
            const float *data=buffer.constData<float>();
            for(int i=0; i<frames; ++i)
                peak=qMax(peak, static_cast<qreal>(qAbs(data[i*channels])));
        }
    }

    return qMin(peak, 1.0);
}


void VoiceRecorderDialog::onFormatRadio(bool checked)
{
    if(!checked)
        return;

    if(opusRadio->isChecked())
        currentFormat=VoiceFormatOpus;
    else if(mp3Radio->isChecked())
        currentFormat=VoiceFormatMp3;

    // Галочка подавления следует за форматом
    updateNoiseSuppressAvailable();

    updateFormatInfo();

    // Расширение в поле имени следует за форматом: нетронутое
    // имя предлагается заново, правленное сохраняет основу
    if(!fileNameEdited)
        fileNameEdit->setText(autoStem()+currentExtension());
    else
    {
        QString text=fileNameEdit->text();
        text.remove(QRegularExpression(QStringLiteral("\\.[A-Za-z0-9]+$")));
        fileNameEdit->setText(text+currentExtension());
    }
}


void VoiceRecorderDialog::onFileNameEdited(const QString &text)
{
    Q_UNUSED(text);

    fileNameEdited=true;
}


void VoiceRecorderDialog::onRecordToggled(bool checked)
{
    if(checked)
    {
        // Формат применяется на каждый старт: радиокнопки
        // можно переключать между дублями. Недоступный формат
        // откатывается на второй, без обоих писать нечего
        VoiceFormat format=currentFormat;

        if(format==VoiceFormatOpus && !opusAvailable)
            format=VoiceFormatMp3;
        else if(format==VoiceFormatMp3 && !mp3Available)
            format=VoiceFormatOpus;

        const bool formatOk=(format==VoiceFormatOpus && opusAvailable) ||
                            (format==VoiceFormatMp3 && mp3Available);

        if(!formatOk)
        {
            statusLabel->setText(tr("No supported audio format"));
            recordButton->setChecked(false);
            return;
        }

        currentFormat=format;

        // Имя из поля пользователя, пусто если каталога нет
        const QString name=userFileName();

        if(name.isEmpty() || workDir.isEmpty())
        {
            statusLabel->setText(tr("Can not record here"));
            recordButton->setChecked(false);
            return;
        }

        // Повторный дубль: прошлый файл стирается сразу, мусора не копим.
        // Имя могли и поменять, поэтому трется предыдущий путь,
        // а не свежевычисленный
        if(!filePath.isEmpty() && QFileInfo(filePath).isFile())
            QFile::remove(filePath);

        filePath=QDir(workDir).absoluteFilePath(name);

        waveform->clearLevels();
        durationMs=0;
        timeLabel->setText(VoiceNoteHelper::formatDuration(0));

        // MP3 пишет бэкенд, Opus пишет наш энкодер
        if(format==VoiceFormatMp3)
        {
            // Устройство освобождается от прогрева: бэкенд открывает
            // захват сам, двух владельцев быть не должно
            stopMonitor();

            applyFormat(format);
            recorder->setOutputLocation(QUrl::fromLocalFile(filePath));
            recorder->record();
        }
        else
        {
            if(!startOpusCapture(filePath))
            {
                recordButton->setChecked(false);
                return;
            }
        }

        timer->start();

        // Устройство, формат, качество и имя не переключают на ходу
        deviceCombo->setEnabled(false);
        opusRadio->setEnabled(false);
        mp3Radio->setEnabled(false);
        fileNameEdit->setEnabled(false);

        recordButton->setText(tr("Stop"));
        statusLabel->setText(tr("Recording..."));
        insertButton->setEnabled(false);
    }
    else
    {
        // MP3 пишет бэкенд, Opus пишет наш энкодер
        if(currentFormat==VoiceFormatMp3)
        {
            recorder->stop();

            // Финал придет из смены состояния. Если бэкенд состояние
            // не сменил (тихо ничего не писал), завершаем сами
            if(recorder->state()!=QMediaRecorder::RecordingState)
                finishRecording();

            // Прогрев возобновляется после бэкенда: он не всегда
            // отпускает устройство мгновенно, попытка отложена.
            // Неуспех не фатален - следующее нажатие записи пойдет
            // старым путем уже с нажатия
            QTimer::singleShot(300, this, [this]
            {
                // Нажатие Записи внутри задержки: вход нельзя ронять
                // рестартом, учитывая активную запись
                if(!recordButton->isChecked())
                    startMonitor();
            });
        }
        else
        {
            stopOpusCapture();

            // Точная длительность из писателя, а не из секундомера
            durationMs=opusWriter.durationMs();
            timeLabel->setText(VoiceNoteHelper::formatDuration(durationMs));

            finishRecording();
        }
    }
}


// Текст кнопки записи зависит от наличия файла: повторный дубль
// перезатирает предыдущий (прошлый файл уже стерт на старте),
// кнопка так и говорит
void VoiceRecorderDialog::updateRecordButton(void)
{
    if(QFileInfo(filePath).isFile())
    {
        recordButton->setText(tr("Re-record"));
        recordButton->setToolTip(tr("Old recording will be overwritten"));
    }
    else
    {
        recordButton->setText(tr("Record"));
        recordButton->setToolTip(QString());
    }
}


// Завершить запись и решить про вставку. Идемпотентно: зовут
// и стоп кнопкой, и смена состояния рекордера
void VoiceRecorderDialog::finishRecording(void)
{
    timer->stop();

    // Секундомер точнее duration() бэкенда
    if(elapsedRunning)
    {
        durationMs=recordElapsed.elapsed();
        elapsedRunning=false;
    }
    else
        durationMs=qMax(durationMs, recorder->duration());
    timeLabel->setText(VoiceNoteHelper::formatDuration(durationMs));

    deviceCombo->setEnabled(true);
    opusRadio->setEnabled(opusAvailable);
    mp3Radio->setEnabled(mp3Available);
    fileNameEdit->setEnabled(true);
    updateRecordButton();

    if(recordButton->isChecked())
    {
        recordButton->blockSignals(true);
        recordButton->setChecked(false);
        recordButton->blockSignals(false);
    }

    // Короткий шум вставкой не становится. Пустой или почти пустой
    // файл (только заголовок) тоже: бэкенд мог ничего не отдать
    hasRecording=(durationMs>500 && QFileInfo(filePath).isFile() &&
                  QFileInfo(filePath).size()>1024);

    if(hasRecording)
        statusLabel->setText(tr("Done"));
    else if(QFileInfo(filePath).isFile() && QFileInfo(filePath).size()<=1024)
        statusLabel->setText(tr("File is too small, backend wrote no audio"));
    else if(statusLabel->text()==tr("Recording..."))
        statusLabel->setText(tr("Nothing recorded"));

    insertButton->setEnabled(hasRecording);
}


void VoiceRecorderDialog::onDeviceChanged(int index)
{
    Q_UNUSED(index);

    // Выбранное устройство пишет. Имя пустое пока список пуст
    if(!deviceCombo->currentText().isEmpty())
        recorder->setAudioInput(deviceCombo->currentText());

    // Прогрев переключается с устройством: мусор нового входа
    // пропускается заново, состояния в startMonitor сбрасываются
    // намеренно. При записи комбобокс заблокирован, лишнего
    // не случается
    if(!recordButton->isChecked())
        startMonitor();
}


// Доступность галочки шумоподавления: работает только на
// собственном захвате Opus. MP3 пишет бэкенд, из его конвейера
// проба читает, но подменить поток не может
void VoiceRecorderDialog::updateNoiseSuppressAvailable(void)
{
    const bool available=(currentFormat==VoiceFormatOpus);

    noiseSuppress->setEnabled(available);
    noiseSuppress->setToolTip(available
                              ? QString()
                              : tr("Noise suppression works in Opus format"));
}


QAudioDeviceInfo VoiceRecorderDialog::currentInputDevice(void) const
{
    const QString name=deviceCombo->currentText();

    if(!name.isEmpty())
        for(const QAudioDeviceInfo &device :
            QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
            if(device.deviceName()==name)
                return device;

    return QAudioDeviceInfo::defaultInputDevice();
}


// Пересчет галочки подавления шума: живой флаг и настройка
void VoiceRecorderDialog::onNoiseSuppressToggled(bool checked)
{
    noiseSuppressOn=checked;
    mytetraConfig.set_voiceNoteNoiseSuppress(checked);
}


// Пересчет окон преобразования очистителя шума под частоту
// записи и чистое состояние его оценок
void VoiceRecorderDialog::setupNoiseChain(int sampleRate)
{
    noiseSuppressor.setup(sampleRate);
}


// Пропуск чанка через спектральное подавление шума. Пол тянется
// онлайн по сглаженным огибающим бинов (без подслушивания до
// нажатия записи), фон глушится и в паузах, и под речью
void VoiceRecorderDialog::applyNoiseSuppress(short *pcm, int samples)
{
    noiseSuppressor.process(pcm, samples);
}


// Договаривание формата записи с устройством и открытие самого входа:
// общая часть монитора прогрева и старого пути (файл не открывает).
// false - нет устройства/формата или вход занят; причины первых двух
// уже в статусе, занятость входа репортится только по флагу
bool VoiceRecorderDialog::openOpusInput(bool reportStartError)
{
    QAudioDeviceInfo device=currentInputDevice();

    if(device.isNull())
    {
        statusLabel->setText(tr("Microphone not found"));
        return false;
    }

    // Энкодер понимает 8/12/16/24/48 кГц, моно 16 бит.
    // Берется лучшее из поддерживаемого микрофоном
    static const int rates[]={48000, 24000, 16000, 12000, 8000};

    QAudioFormat format;
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    bool formatOk=false;

    for(int rate : rates)
    {
        format.setSampleRate(rate);

        if(device.isFormatSupported(format))
        {
            formatOk=true;
            break;
        }
    }

    if(!formatOk)
    {
        statusLabel->setText(tr("No supported audio format"));
        return false;
    }

    opusFormat=format;

    // Сколько мусора пропущено и что фильтр DC еще не инициализирован,
    // ровно для этой сессии входа
    warmupDroppedMs=0;
    dcBlockInit=false;
    dcBlockPrevIn=0.0;
    dcBlockPrevOut=0.0;

    // Подавление фона: пересчет коэффициентов под частоту входа
    // и чистое состояние. Оценки шума калибруются в потоке до
    // нажатия записи
    setupNoiseChain(format.sampleRate());

    audioInput=new QAudioInput(device, opusFormat, this);
    opusDevice=audioInput->start();

    // Старт может не состояться: устройство еще не отпущено прошлым
    // дублем (deleteLater + асинхронное освобождение в бэкенде) или
    // занято другим приложением. Без проверки дальше тихая мертвая
    // запись (readyRead не придет), а stop() по нестартовавшему входу
    // роняет часть бэкендов, поэтому неуспех сразу чистый
    if(opusDevice==nullptr || audioInput->error()!=QAudio::NoError)
    {
        // Здесь вызов не из сигнала входа: удаляем сразу, не deleteLater
        delete audioInput;
        audioInput=nullptr;
        opusDevice=nullptr;

        if(reportStartError)
            statusLabel->setText(tr("Can not record here"));

        return false;
    }

    connect(opusDevice, &QIODevice::readyRead,
            this,        &VoiceRecorderDialog::onOpusAudioData);
    connect(audioInput, &QAudioInput::stateChanged,
            this,       &VoiceRecorderDialog::onOpusStateChanged);

    return true;
}


bool VoiceRecorderDialog::startOpusCapture(const QString &filePath)
{
    // Прошлый дубль: файл закрывается, при прогреве вход не трогается
    stopOpusCapture();

    // Прогрев: вход жив, мусор осел, модель шума откалибрована.
    // Запись идет в тот же поток. Бюджет пропуска мусора свежий,
    // на случай всплеска при нажатии; состояния DC и оценки
    // шума специально не сбрасываются
    if(micMonitoring)
    {
        warmupDroppedMs=0;

        if(!opusWriter.open(filePath, opusFormat.sampleRate()))
        {
            statusLabel->setText(opusWriter.errorString());
            return false;
        }

        waveform->clearLevels();
        return true;
    }

    // Старый путь: прогрева нет (устройство было занято при открытии
    // окна или прогрев не удался), вход открывается по нажатию
    if(!openOpusInput(true))
        return false;

    if(!opusWriter.open(filePath, opusFormat.sampleRate()))
    {
        statusLabel->setText(opusWriter.errorString());

        // Файл не открылся: записи не будет, вход освобождается,
        // устройство не держится занятым впустую
        stopOpusCapture();

        return false;
    }

    waveform->clearLevels();

    return true;
}


void VoiceRecorderDialog::stopOpusCapture(void)
{
    // Дубль заканчивается закрытием файла
    if(opusWriter.isOpen())
        opusWriter.close();

    // Прогрев жив: вход тот же, монитор продолжается и после записи,
    // следующий дубль воспользуется уже прогретым состоянием
    if(micMonitoring)
        return;

    // Старый путь: вход освобождается целиком
    stopMonitor();
}


// Прогрев входа: вход стримит пока открыто окно. Мусор аналогового
// пробуждения оседает до нажатия записи, модель шума калибруется
// реальным фоном комнаты, волна показывает живой уровень. Начало -
// с рестарта: старый вход сначала освобождается
void VoiceRecorderDialog::startMonitor(void)
{
    stopMonitor();

    // Без репорта в статус: прогрев не обязательная часть, старый
    // путь нажатия записи полноценный запасной вариант
    if(!openOpusInput(false))
        return;

    micMonitoring=true;
    waveform->clearLevels();
}


void VoiceRecorderDialog::stopMonitor(void)
{
    micMonitoring=false;

    // Освобождать нечего: после старого пути вход уже удален,
    // сбрасывается только указатель устройства
    if(audioInput==nullptr)
    {
        opusDevice=nullptr;
        return;
    }

    // Сигналы отключаются до удаления: readyRead/stateChanged
    // иначе их доставка в полуживой объект роняет бэкенд
    // (PulseAudio threaded-ml) при повторных записях
    if(opusDevice!=nullptr)
        disconnect(opusDevice, &QIODevice::readyRead,
                   this,        &VoiceRecorderDialog::onOpusAudioData);

    disconnect(audioInput, &QAudioInput::stateChanged,
               this,       &VoiceRecorderDialog::onOpusStateChanged);

    // Нестартовавший вход (остаток неуспешного старта) stop()
    // не дергаем: часть бэкендов на этом падает. Только удаляем
    if(audioInput->state()!=QAudio::StoppedState)
        audioInput->stop();

    // Синхронное удаление: deleteLater оставлял вход живым до
    // следующего цикла событий, и при закрытии диалога бэкенд
    // обращался к уже освобожденным данным
    delete audioInput;
    audioInput=nullptr;
    opusDevice=nullptr;
}


void VoiceRecorderDialog::onOpusAudioData(void)
{
    if(opusDevice==nullptr)
        return;

    const QByteArray data=opusDevice->readAll();

    if(data.isEmpty())
        return;

    // DC-blocker: y[n]=x[n]-x[n-1]+R*y[n-1]. Снимает выстрел по
    // постоянной составляющей при пробуждении аналогового входа
    // из suspend: рельса демпфируется в ноль за миллисекунды,
    // затухающий низкочастотный хвост тоже уходит.
    // Фильтр в одном копе QByteArray, ибо данные и так из копии
    // readAll
    QByteArray processed=data;
    short *pcm=reinterpret_cast<short *>(processed.data());
    const int samples=processed.size()/static_cast<int>(sizeof(short));
    const double radius=1.0-6.2831853071795862*DC_BLOCK_CUTOFF_HZ
                                    /opusFormat.sampleRate();

    qreal rawSum=0.0;
    qreal rawMean=0.0;

    int start=0;

    if(!dcBlockInit)
    {
        // Первая выборка записи считается точкой отсчета DC:
        // рельса с самого начала не имеет переходных процессов
        dcBlockInit=true;
        dcBlockPrevIn=pcm[0];
        dcBlockPrevOut=0.0;
        pcm[0]=0;
        start=1;
    }

    for(int i=start; i<samples; ++i)
    {
        // Среднее сырых сэмплов - для теста рельсы: у нормальной
        // речи DC никогда не доходит до половины шкалы
        rawSum+=pcm[i];

        const double s=pcm[i];
        const double y=s-dcBlockPrevIn+radius*dcBlockPrevOut;
        dcBlockPrevIn=s;
        dcBlockPrevOut=y;
        pcm[i]=static_cast<short>(qBound(-32768.0, y, 32767.0));
    }

    rawMean=static_cast<qreal>(rawSum)/samples;

    // Мусор прогрева: насыщенные батчи не пишем и в волну не
    // показываем (после фильтра DC такое уже почти не случается,
    // доставка от широкополосного цифрового мусора). Плюс батч
    // с рельсой по DC: ступенька внутри чанка фильтр уже
    // не гасит
    const bool garbageChunk=isSaturatedChunk(pcm, samples)
                            || qAbs(rawMean)>=16384;

    if(warmupDroppedMs<WARMUP_MAX_DROP_MS && garbageChunk)
    {
        warmupDroppedMs+=samples*1000/opusFormat.sampleRate();
        return;
    }

    // Подавление фона по галочке: в модель шума попадают только
    // чанки, прошедшие проверку мусора прогрева, - пол калибруется
    // реальным звуком, а не рельсовой помехой
    if(noiseSuppressOn)
        applyNoiseSuppress(pcm, samples);

    // Пик для волны из того же потока, масштаб как у пробы. Уровень
    // рисуется и во время прогрева: видно, жив ли микрофон и не
    // занят ли он посторонним приложением
    qreal peak=0.0;

    for(int i=0; i<samples; ++i)
        peak=qMax(peak, qAbs(static_cast<qreal>(pcm[i]))/32768.0);

    waveform->pushLevel(peak*4.0);

    // Во время прогрева файл не открыт: чанки заканчиваются
    // на волне и подавлении шума
    if(!opusWriter.isOpen())
        return;

    if(!opusWriter.writePcm(pcm, samples))
    {
        statusLabel->setText(opusWriter.errorString());
        recordButton->setChecked(false);
        return;
    }

    durationMs=opusWriter.durationMs();
    timeLabel->setText(VoiceNoteHelper::formatDuration(durationMs));
}


void VoiceRecorderDialog::onOpusStateChanged(QAudio::State state)
{
    Q_UNUSED(state);

    // У QAudioInput нет errorString, только код: любая ошибка
    // захвата завершает запись тем же текстом, что отказ старта
    if(audioInput==nullptr || audioInput->error()==QAudio::NoError)
        return;

    // Поломанный вход монитором оставаться не может: он
    // разбирается целиком, в конце будет попытка ре-прогрева
    micMonitoring=false;
    stopOpusCapture();

    timer->stop();
    elapsedRunning=false;

    statusLabel->setText(tr("Can not record here"));

    deviceCombo->setEnabled(true);
    opusRadio->setEnabled(opusAvailable);
    mp3Radio->setEnabled(mp3Available);
    fileNameEdit->setEnabled(true);
    updateRecordButton();

    if(recordButton->isChecked())
    {
        recordButton->blockSignals(true);
        recordButton->setChecked(false);
        recordButton->blockSignals(false);
    }

    hasRecording=false;
    insertButton->setEnabled(false);

    // Попытка ре-прогрева: следующее нажатие записи в случае удачи
    // снова получит прогретый вход, в случае неудачи - старый путь
    startMonitor();
}


void VoiceRecorderDialog::onStateChanged(QMediaRecorder::State state)
{
    if(state==QMediaRecorder::RecordingState)
    {
        statusLabel->setText(tr("Recording..."));

        // Секундомер стартует по факту записи, а не по кнопке
        recordElapsed.start();
        elapsedRunning=true;
    }
    else if(state==QMediaRecorder::StoppedState)
        finishRecording();
}


void VoiceRecorderDialog::onRecorderError(QMediaRecorder::Error error)
{
    Q_UNUSED(error);

    timer->stop();
    elapsedRunning=false;
    statusLabel->setText(recorder->errorString());

    deviceCombo->setEnabled(true);
    opusRadio->setEnabled(opusAvailable);
    mp3Radio->setEnabled(mp3Available);
    fileNameEdit->setEnabled(true);
    updateRecordButton();

    if(recordButton->isChecked())
    {
        recordButton->blockSignals(true);
        recordButton->setChecked(false);
        recordButton->blockSignals(false);
    }

    hasRecording=false;
    insertButton->setEnabled(false);
}


void VoiceRecorderDialog::onProbeBuffer(const QAudioBuffer &buffer)
{
    // Волна с запасом x4: сырые пики микрофона мелкие,
    // кламп до 1 внутри виджета
    waveform->pushLevel(bufferPeak(buffer)*4.0);
}


void VoiceRecorderDialog::onTick(void)
{
    if(elapsedRunning)
        durationMs=recordElapsed.elapsed();
    else
        durationMs=qMax(durationMs, recorder->duration());

    timeLabel->setText(VoiceNoteHelper::formatDuration(durationMs));
}


void VoiceRecorderDialog::onInsert(void)
{
    if(hasRecording)
        accept();
}

#include <QAudio>
#include <QAudioOutput>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QTimer>

#include "OpusPlayer.h"
#include "OpusFileReader.h"


// Устройство-источник для QAudioOutput (pull mode): тянет декодированный
// PCM из ридера по запросу аудиопотока
class OpusFeedDevice : public QIODevice
{
public:

    explicit OpusFeedDevice(OpusFileReader *reader, QObject *parent=nullptr)
        : QIODevice(parent), reader(reader) {}

    bool isSequential(void) const override { return true; }

protected:

    qint64 readData(char *data, qint64 maxSize) override
    {
        if(reader==nullptr || !reader->isOpen())
            return 0;

        // Читаем фреймами: short, моно/стерео учитывается внутри ридера
        const int maxFrames=static_cast<int>(maxSize/sizeof(short));
        short *out=reinterpret_cast<short *>(data);

        int total=0;

        while(total<maxFrames)
        {
            const int got=reader->readPcm(out+total, maxFrames-total);

            if(got<=0)
                break;

            total+=got;
        }

        return static_cast<qint64>(total)*sizeof(short);
    }

    qint64 writeData(const char *, qint64) override { return -1; }

private:

    OpusFileReader *reader;
};


OpusPlayer::OpusPlayer(QObject *parent) : QObject(parent)
{
    reader=new OpusFileReader();
    audioOutput=nullptr;
    feedDevice=nullptr;
    outRate=48000;
    totalMs=0;
    lastPosition=0;
    playing=false;
    endReached=false;
}


OpusPlayer::~OpusPlayer(void)
{
    stop();
    delete reader;
}


bool OpusPlayer::setFile(const QString &path)
{
    stop();

    if(!reader->open(path))
        return false;

    totalMs=reader->durationMs();
    outRate=reader->outputRate();
    lastPosition=0;
    endReached=false;

    // Длительность снята — файл сразу закрываем, иначе на Windows/Wine
    // висящий хэндл блокирует удаление записи в корзину
    reader->close();

    filePath=path;

    emit durationChanged(totalMs);

    return true;
}


void OpusPlayer::play(void)
{
    if(filePath.isEmpty())
        return;

    // После stop()/финала файл закрыт — открываем заново с той же частотой
    if(!reader->isOpen())
    {
        if(!reader->open(filePath, outRate))
        {
            emit error(QStringLiteral("Can not open file"));
            return;
        }

        reader->seekMs(lastPosition);
    }

    if(audioOutput==nullptr)
    {
        QAudioFormat format;
        format.setSampleRate(reader->outputRate());
        format.setChannelCount(reader->channels());
        format.setSampleSize(16);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QAudioDeviceInfo device=QAudioDeviceInfo::defaultOutputDevice();

        if(!device.isFormatSupported(format))
            format=device.nearestFormat(format);

        audioOutput=new QAudioOutput(device, format, this);
        audioOutput->setNotifyInterval(100);

        connect(audioOutput, &QAudioOutput::notify,
                this,        &OpusPlayer::onNotify);
        connect(audioOutput, &QAudioOutput::stateChanged,
                this,        &OpusPlayer::onStateChanged);

        feedDevice=new OpusFeedDevice(reader, this);
        feedDevice->open(QIODevice::ReadOnly);
    }

    // Пауза снимается продолжением, а не перезапуском
    if(audioOutput->state()==QAudio::SuspendedState)
    {
        endReached=false;
        playing=true;
        audioOutput->resume();
        return;
    }

    endReached=false;
    playing=true;
    audioOutput->start(feedDevice);
}


void OpusPlayer::pause(void)
{
    if(audioOutput!=nullptr && playing)
    {
        audioOutput->suspend();
        playing=false;
    }
}


void OpusPlayer::stop(void)
{
    playing=false;
    endReached=false;
    lastPosition=0;

    if(audioOutput!=nullptr)
    {
        audioOutput->stop();
        delete audioOutput;
        audioOutput=nullptr;
    }

    if(feedDevice!=nullptr)
    {
        delete feedDevice;
        feedDevice=nullptr;
    }

    // Хэндл файла отпускаем: на Windows/Wine открытый файл
    // нельзя переименовать (удаление записи в корзину падало)
    if(reader!=nullptr && reader->isOpen())
        reader->close();
}


void OpusPlayer::setPosition(qint64 ms)
{
    if(reader==nullptr || filePath.isEmpty())
        return;

    // Позицию могут двигать на паузе/стопе, когда файл уже закрыт
    if(!reader->isOpen() && !reader->open(filePath, outRate))
        return;

    const bool wasPlaying=playing;

    if(audioOutput!=nullptr)
        audioOutput->stop();

    reader->seekMs(ms);
    lastPosition=reader->positionMs();
    endReached=false;

    emit positionChanged(lastPosition);

    if(wasPlaying && audioOutput!=nullptr)
        audioOutput->start(feedDevice);
    else if(reader->isOpen())
    {
        // Не играет (пауза/стоп/простой сдвиг ползунка): хэндл не держим,
        // play() потом сам переоткроет и встанет на lastPosition
        reader->close();
    }
}


qint64 OpusPlayer::positionMs(void) const
{
    return lastPosition;
}


qint64 OpusPlayer::durationMs(void) const
{
    return totalMs;
}


bool OpusPlayer::isPlaying(void) const
{
    return playing;
}


bool OpusPlayer::isPaused(void) const
{
    return audioOutput!=nullptr &&
           audioOutput->state()==QAudio::SuspendedState;
}


void OpusPlayer::onNotify(void)
{
    if(reader==nullptr || !reader->isOpen())
        return;

    lastPosition=reader->positionMs();

    emit positionChanged(lastPosition);
}


void OpusPlayer::onStateChanged(QAudio::State state)
{
    // Pull-источник отдал 0 байт: файл кончился внутри readData.
    // QAudioOutput уходит в Idle, трактуем как финал
    if(state==QAudio::IdleState && playing && !endReached)
    {
        if(reader==nullptr || !reader->isOpen())
            return;

        endReached=true;
        playing=false;
        lastPosition=totalMs;

        emit positionChanged(lastPosition);
        emit finished();

        // Хэндл отпускаем сразу: иначе блокируется удаление в корзину
        reader->close();
    }
}


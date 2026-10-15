#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QTimer>
#include <QLibrary>
#include <QFileInfo>
#include <QDebug>

#include "VoicePlayerDialog.h"
#include "libraries/VoiceNoteHelper.h"
#include "models/appConfig/AppConfig.h"


extern AppConfig mytetraConfig;


// GStreamer без заголовков: резолвятся только стабильные
// ABI-функции 1.x. Числовые константы енумов тоже стабильны
// и не менялись весь 1.x: состояния, GST_FORMAT_TIME,
// GST_SEEK_FLAG_FLUSH, типы сообщений шины
namespace
{

enum
{
    GST_STATE_NULL=1,
    GST_STATE_PAUSED=3,
    GST_STATE_PLAYING=4,
    GST_FORMAT_TIME=3,
    GST_SEEK_FLAG_FLUSH=1,
    GST_MESSAGE_EOS=1,
    GST_MESSAGE_ERROR=2
};

typedef int (*GstInitCheckFn)(int *, char ***, void **);
typedef void *(*GstParseLaunchFn)(const char *, void **);
typedef void *(*GstElementFactoryMakeFn)(const char *, const char *);
typedef void (*GstObjectUnrefFn)(void *);
typedef int (*GstElementSetStateFn)(void *, int);
typedef int (*GstElementQueryPositionFn)(void *, int, long long *);
typedef int (*GstElementQueryDurationFn)(void *, int, long long *);
typedef int (*GstElementSeekSimpleFn)(void *, int, int, long long);
typedef void *(*GstElementGetBusFn)(void *);
typedef void *(*GstBusPopFilteredFn)(void *, int);
typedef void (*GstMessageUnrefFn)(void *);
typedef int (*GstUtilSetObjectArgFn)(void *, const char *, const char *);
typedef void *(*GstBinGetByNameFn)(void *, const char *);
typedef void (*GstVersionFn)(unsigned *, unsigned *, unsigned *, unsigned *);

struct GstApi
{
    GstInitCheckFn initCheck=nullptr;
    GstParseLaunchFn parseLaunch=nullptr;
    GstElementFactoryMakeFn factoryMake=nullptr;
    GstObjectUnrefFn objectUnref=nullptr;
    GstElementSetStateFn setState=nullptr;
    GstElementQueryPositionFn queryPosition=nullptr;
    GstElementQueryDurationFn queryDuration=nullptr;
    GstElementSeekSimpleFn seekSimple=nullptr;
    GstElementGetBusFn getBus=nullptr;
    GstBusPopFilteredFn busPop=nullptr;
    GstMessageUnrefFn messageUnref=nullptr;
    GstUtilSetObjectArgFn setObjectArg=nullptr;
    GstBinGetByNameFn binGetByName=nullptr;
    GstVersionFn version=nullptr;
    bool ok=false;
};

const GstApi &gstApi(void)
{
    static GstApi api;
    static bool done=false;
    static QLibrary gstLib;

    if(!done)
    {
        done=true;
        const char *candidates[]={"libgstreamer-1.0.so.0",
                                 "libgstreamer-1.0.so",
                                 nullptr};

        for(int i=0; candidates[i]!=nullptr; ++i)
        {
            gstLib.setFileName(QString::fromLatin1(candidates[i]));

            if(gstLib.load())
                break;
        }

        if(!gstLib.isLoaded())
        {
            qWarning() << "VoicePlayer: libgstreamer not loaded";
            return api;
        }

        bool ok=true;

#define VOICE_RESOLVE(field, name) \
        api.field=reinterpret_cast<decltype(api.field)>(gstLib.resolve(name)); \
        ok=ok && (api.field!=nullptr)

        VOICE_RESOLVE(initCheck, "gst_init_check");
        VOICE_RESOLVE(parseLaunch, "gst_parse_launch");
        VOICE_RESOLVE(factoryMake, "gst_element_factory_make");
        VOICE_RESOLVE(objectUnref, "gst_object_unref");
        VOICE_RESOLVE(setState, "gst_element_set_state");
        VOICE_RESOLVE(queryPosition, "gst_element_query_position");
        VOICE_RESOLVE(queryDuration, "gst_element_query_duration");
        VOICE_RESOLVE(seekSimple, "gst_element_seek_simple");
        VOICE_RESOLVE(getBus, "gst_element_get_bus");
        VOICE_RESOLVE(busPop, "gst_bus_pop_filtered");
        VOICE_RESOLVE(messageUnref, "gst_message_unref");
        VOICE_RESOLVE(setObjectArg, "gst_util_set_object_arg");
        VOICE_RESOLVE(binGetByName, "gst_bin_get_by_name");
        VOICE_RESOLVE(version, "gst_version");

#undef VOICE_RESOLVE

        if(ok)
        {
            unsigned major=0;
            unsigned vminor=0;
            unsigned vmicro=0;
            unsigned vnano=0;
            api.version(&major, &vminor, &vmicro, &vnano);
            ok=(major==1);

            if(ok)
            {
                int argc=0;
                api.initCheck(&argc, nullptr, nullptr);
            }
        }

        if(!ok)
            qWarning() << "VoicePlayer: gstreamer resolve failed";

        api.ok=ok;
    }

    return api;
}

} // namespace


QList<double> VoicePlayerDialog::speedSteps(void)
{
    return QList<double>() << 0.5 << 0.75 << 1.0 << 1.25 << 1.5 << 1.75 << 2.0;
}


double VoicePlayerDialog::snapSpeed(double speed)
{
    const QList<double> steps=speedSteps();
    double best=steps.first();
    double bestDiff=qAbs(speed-best);

    for(double step : steps)
    {
        const double diff=qAbs(speed-step);

        if(diff<bestDiff)
        {
            bestDiff=diff;
            best=step;
        }
    }

    return best;
}


VoicePlayerDialog::VoicePlayerDialog(const QString &filePath, const QString &title,
                                     QWidget *parent, const QString &sinkOverride) : QDialog(parent),
    sinkName(sinkOverride.isEmpty() ? QStringLiteral("autoaudiosink") : sinkOverride),
    currentSpeed(1.0),
    fileAbsPath(QFileInfo(filePath).absoluteFilePath())
{
    setWindowTitle(tr("Voice note"));
    setModal(true);

    titleLabel=new QLabel(title, this);
    playButton=new QPushButton(tr("Play"), this);
    playButton->setCheckable(true);

    QLabel *speedLabel=new QLabel(tr("Speed:"), this);
    speedCombo=new QComboBox(this);

    for(double step : speedSteps())
        speedCombo->addItem(QString::number(step)+QStringLiteral("x"), step);

    positionSlider=new QSlider(Qt::Horizontal, this);
    timeLabel=new QLabel(VoiceNoteHelper::formatDuration(0), this);
    statusLabel=new QLabel(this);
    closeButton=new QPushButton(tr("Close"), this);

    QHBoxLayout *playerLayout=new QHBoxLayout();
    playerLayout->addWidget(playButton);
    playerLayout->addWidget(speedLabel);
    playerLayout->addWidget(speedCombo);
    playerLayout->addWidget(positionSlider, 1);
    playerLayout->addWidget(timeLabel);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addWidget(titleLabel);
    centralLayout->addLayout(playerLayout);
    centralLayout->addWidget(statusLabel);
    centralLayout->addWidget(closeButton);
    setLayout(centralLayout);

    // Скорость по умолчанию из глобальных настроек
    currentSpeed=snapSpeed(mytetraConfig.get_voiceNoteSpeed());
    speedCombo->setCurrentIndex(speedCombo->findData(currentSpeed));

    tick=new QTimer(this);
    tick->setInterval(200);

    connect(playButton, &QPushButton::toggled,
            this,        &VoicePlayerDialog::onPlayToggled);
    connect(speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,       &VoicePlayerDialog::onSpeedChanged);
    connect(positionSlider, &QSlider::sliderMoved,
            this,           &VoicePlayerDialog::onSliderMoved);
    connect(tick, &QTimer::timeout,
            this, &VoicePlayerDialog::onTick);
    connect(closeButton, &QPushButton::clicked,
            this,        &VoicePlayerDialog::accept);

    // Диагностика пути: абсолютный, размер, читаемость
    const QString absolutePath=fileAbsPath;
    const QFileInfo checkFile(absolutePath);
    qDebug() << "VoicePlayer file" << absolutePath
             << "exists" << checkFile.isFile()
             << "size" << checkFile.size()
             << "readable" << checkFile.isReadable();

    if(!checkFile.isFile())
    {
        timeLabel->setText(tr("File not found"));
        playButton->setEnabled(false);
        speedCombo->setEnabled(false);
        positionSlider->setEnabled(false);
        return;
    }

    if(!gstApi().ok)
    {
        statusLabel->setText(tr("Audio backend unavailable"));
        playButton->setEnabled(false);
        speedCombo->setEnabled(false);
        positionSlider->setEnabled(false);
        return;
    }

    buildPipeline();

    if(gstPipeline==nullptr)
    {
        statusLabel->setText(tr("Can not open file"));
        playButton->setEnabled(false);
        speedCombo->setEnabled(false);
        positionSlider->setEnabled(false);
        return;
    }

    // Без tempo-элемента обычное проигрывание: выпадашка гаснет
    if(gstTempo==nullptr)
        speedCombo->setEnabled(false);

    applySpeed();
    tick->start();
}


VoicePlayerDialog::~VoicePlayerDialog(void)
{
    closePipeline();
}


QString VoicePlayerDialog::formatTime(qint64 ms, qint64 totalMs)
{
    return VoiceNoteHelper::formatDuration(ms)+
           QStringLiteral(" / ")+
           VoiceNoteHelper::formatDuration(totalMs);
}


void VoicePlayerDialog::buildPipeline(void)
{
    const GstApi &api=gstApi();

    // Tempo-элемент: pitch держит тон явно, scaletempo по построению.
    // Пробные инстансы сразу отпускаются
    QString tempoPart;
    void *probe=api.factoryMake("pitch", nullptr);

    if(probe!=nullptr)
    {
        api.objectUnref(probe);
        tempoPart=QStringLiteral("pitch name=tempo tempo=1.0 pitch=1.0");
        qDebug() << "VoicePlayer tempo element: pitch";
    }
    else
    {
        probe=api.factoryMake("scaletempo", nullptr);

        if(probe!=nullptr)
        {
            api.objectUnref(probe);
            tempoPart=QStringLiteral("scaletempo name=tempo tempo=1.0");
            qDebug() << "VoicePlayer tempo element: scaletempo";
        }
        else
            qDebug() << "VoicePlayer tempo element: none, plain playback";
    }

    // Путь из сохраненного при построении: filesrc экранируется
    // от кавычек и бэкслэшей
    QString location=fileAbsPath;
    location.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    location.replace(QStringLiteral("\""), QStringLiteral("\\\""));

    QString launch=QStringLiteral("filesrc location=\"%1\" ! decodebin ! audioconvert ! audioresample ! ").arg(location);

    if(!tempoPart.isEmpty())
        launch+=tempoPart+QStringLiteral(" ! ");

    launch+=sinkName;

    qDebug() << "VoicePlayer pipeline:" << launch;

    gstPipeline=api.parseLaunch(launch.toUtf8().constData(), nullptr);

    if(gstPipeline==nullptr)
    {
        qWarning() << "VoicePlayer: parse launch failed";
        return;
    }

    gstBus=api.getBus(gstPipeline);

    if(!tempoPart.isEmpty())
        gstTempo=api.binGetByName(gstPipeline, "tempo");
}


void VoicePlayerDialog::closePipeline(void)
{
    const GstApi &api=gstApi();

    if(!api.ok)
        return;

    if(gstPipeline!=nullptr)
    {
        api.setState(gstPipeline, GST_STATE_NULL);

        if(gstBus!=nullptr)
        {
            api.objectUnref(gstBus);
            gstBus=nullptr;
        }

        if(gstTempo!=nullptr)
        {
            api.objectUnref(gstTempo);
            gstTempo=nullptr;
        }

        api.objectUnref(gstPipeline);
        gstPipeline=nullptr;
    }
}


void VoicePlayerDialog::applySpeed(void)
{
    if(gstTempo==nullptr || !gstApi().ok)
        return;

    gstApi().setObjectArg(gstTempo, "tempo",
        QString::number(currentSpeed).toLatin1().constData());
}


void VoicePlayerDialog::pollBus(void)
{
    const GstApi &api=gstApi();

    if(gstBus==nullptr)
        return;

    // EOS и ошибки забираются по отдельности: тип сообщения
    // из структуры без заголовков не прочитать
    for(int i=0; i<8; ++i)
    {
        void *eosMsg=api.busPop(gstBus, GST_MESSAGE_EOS);

        if(eosMsg!=nullptr)
        {
            api.messageUnref(eosMsg);
            onEosReached();
        }

        void *errMsg=api.busPop(gstBus, GST_MESSAGE_ERROR);

        if(errMsg!=nullptr)
        {
            api.messageUnref(errMsg);
            onPlaybackError();
            return;
        }

        if(eosMsg==nullptr && errMsg==nullptr)
            break;
    }
}


qint64 VoicePlayerDialog::queryPositionMs(void) const
{
    if(gstPipeline==nullptr || !gstApi().ok)
        return -1;

    long long posNs=0;

    if(!gstApi().queryPosition(gstPipeline, GST_FORMAT_TIME, &posNs))
        return -1;

    return static_cast<qint64>(posNs/1000000);
}


qint64 VoicePlayerDialog::queryDurationMs(void) const
{
    if(gstPipeline==nullptr || !gstApi().ok)
        return -1;

    long long durNs=0;

    if(!gstApi().queryDuration(gstPipeline, GST_FORMAT_TIME, &durNs))
        return -1;

    return static_cast<qint64>(durNs/1000000);
}


void VoicePlayerDialog::onPlayToggled(bool checked)
{
    const GstApi &api=gstApi();

    if(gstPipeline==nullptr)
        return;

    qDebug() << "VoicePlayer play toggled" << checked
             << "pos" << positionMs << "total" << totalMs;

    if(checked)
    {
        playButton->setText(tr("Pause"));

        // Повтор после конца: явная перемотка в начало перед play.
        // Пауза перед seek для надежности на части приемников
        if(eosReached || (totalMs>0 && positionMs>=totalMs-200))
        {
            api.setState(gstPipeline, GST_STATE_PAUSED);
            api.seekSimple(gstPipeline, GST_FORMAT_TIME,
                           GST_SEEK_FLAG_FLUSH, 0);
            eosReached=false;
        }

        api.setState(gstPipeline, GST_STATE_PLAYING);
    }
    else
    {
        playButton->setText(tr("Play"));
        api.setState(gstPipeline, GST_STATE_PAUSED);
    }
}


void VoicePlayerDialog::onSpeedChanged(int index)
{
    currentSpeed=speedCombo->itemData(index).toDouble();
    applySpeed();
}


void VoicePlayerDialog::onSliderMoved(int position)
{
    const GstApi &api=gstApi();

    if(gstPipeline==nullptr || totalMs<=0)
        return;

    api.seekSimple(gstPipeline, GST_FORMAT_TIME,
                   GST_SEEK_FLAG_FLUSH,
                   static_cast<long long>(position)*1000000);
    positionMs=position;
    timeLabel->setText(formatTime(position, totalMs));
}


void VoicePlayerDialog::onTick(void)
{
    if(gstPipeline==nullptr)
        return;

    pollBus();

    if(gstPipeline==nullptr)
        return;

    if(totalMs<=0)
    {
        const qint64 duration=queryDurationMs();

        if(duration>0)
        {
            totalMs=duration;
            positionSlider->setRange(0, static_cast<int>(duration));
        }
    }

    const qint64 position=queryPositionMs();

    if(position>=0)
    {
        positionMs=position;

        if(!positionSlider->isSliderDown())
            positionSlider->setValue(static_cast<int>(position));

        timeLabel->setText(formatTime(position, totalMs));
    }
}


void VoicePlayerDialog::onEosReached(void)
{
    // Доиграло до конца: кнопка снова Play, ползунок в начало.
    // Позиция сбросится seek при следующем play
    eosReached=true;

    playButton->blockSignals(true);
    playButton->setChecked(false);
    playButton->blockSignals(false);
    playButton->setText(tr("Play"));

    positionSlider->blockSignals(true);
    positionSlider->setValue(0);
    positionSlider->blockSignals(false);
    timeLabel->setText(formatTime(0, totalMs));
}


void VoicePlayerDialog::onPlaybackError(void)
{
    qDebug() << "VoicePlayer playback error";

    statusLabel->setText(tr("Playback error"));

    playButton->setEnabled(false);
    speedCombo->setEnabled(false);
    positionSlider->setEnabled(false);

    if(playButton->isChecked())
    {
        playButton->blockSignals(true);
        playButton->setChecked(false);
        playButton->blockSignals(false);
    }

    playButton->setText(tr("Play"));
}

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <QMediaPlayer>
#include <QMediaContent>

#include "libraries/OpusPlayer.h"
#include "libraries/MtComboBox.h"
#include "models/appConfig/AppConfig.h"

#include "VoicePlayerDialog.h"
#include "libraries/VoiceNoteHelper.h"


extern AppConfig mytetraConfig;


VoicePlayerDialog::VoicePlayerDialog(const QString &filePath, const QString &title,
                                     QWidget *parent) : QDialog(parent),
    totalMs(0),
    opusPlayer(nullptr),
    useOpusPlayer(false)
{
    setWindowTitle(tr("Voice note"));
    setModal(true);

    titleLabel=new QLabel(title, this);
    playButton=new QPushButton(tr("Play"), this);
    playButton->setCheckable(true);
    positionSlider=new QSlider(Qt::Horizontal, this);
    timeLabel=new QLabel(VoiceNoteHelper::formatDuration(0), this);
    statusLabel=new QLabel(this);
    closeButton=new QPushButton(tr("Close"), this);

    // Скорость проигрывания 0.5x..2x шагом 0.25, дефолт из конфига.
    // Работает только на бэкенде Qt (MP3): у QMediaPlayer нет темпа
    // с компенсацией тона, а собственный Opus-декодер темпа не имеет
    // вовсе (gstreamer-пайплайн v1 сюда не переносится: он убил бы
    // проигрывание opus на Wine). Для opus выпадашка гаснет
    speedLabel=new QLabel(tr("Speed:"), this);
    speedCombo=new MtComboBox(this);

    for(double step : speedSteps())
        speedCombo->addItem(QString::number(step)+QStringLiteral("x"), step);

    currentSpeed=snapSpeed(mytetraConfig.get_voiceNoteSpeed());
    speedCombo->setCurrentIndex(speedCombo->findData(currentSpeed));

    QHBoxLayout *playerLayout=new QHBoxLayout();
    playerLayout->addWidget(playButton);
    playerLayout->addWidget(positionSlider, 1);
    playerLayout->addWidget(timeLabel);

    QHBoxLayout *speedLayout=new QHBoxLayout();
    speedLayout->addWidget(speedLabel);
    speedLayout->addWidget(speedCombo, 1);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addWidget(titleLabel);
    centralLayout->addLayout(playerLayout);
    centralLayout->addLayout(speedLayout);
    centralLayout->addWidget(statusLabel);
    centralLayout->addWidget(closeButton);
    setLayout(centralLayout);

    // .opus играет собственный декодер: бэкенд Qt его нигде,
    // кроме Linux, не умеет. Остальное играет бэкенд как раньше
    useOpusPlayer=filePath.endsWith(QStringLiteral(".opus"), Qt::CaseInsensitive);

    player=new QMediaPlayer(this);

    connect(playButton, &QPushButton::toggled,
            this,        &VoicePlayerDialog::onPlayToggled);

    if(useOpusPlayer)
    {
        opusPlayer=new OpusPlayer(this);

        connect(opusPlayer, &OpusPlayer::positionChanged,
                this,       &VoicePlayerDialog::onPositionChanged);
        connect(opusPlayer, &OpusPlayer::durationChanged,
                this,       &VoicePlayerDialog::onDurationChanged);
        connect(opusPlayer, &OpusPlayer::finished,
                this,       &VoicePlayerDialog::onOpusFinished);
    }
    else
    {
        connect(player, &QMediaPlayer::positionChanged,
                this,   &VoicePlayerDialog::onPositionChanged);
        connect(player, &QMediaPlayer::durationChanged,
                this,   &VoicePlayerDialog::onDurationChanged);
        connect(player, QOverload<QMediaPlayer::State>::of(&QMediaPlayer::stateChanged),
                this,   &VoicePlayerDialog::onStateChanged);
        connect(player, &QMediaPlayer::mediaStatusChanged,
                this,   &VoicePlayerDialog::onMediaStatusChanged);
        connect(player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
                this,   &VoicePlayerDialog::onPlayerError);
    }
    connect(positionSlider, &QSlider::sliderMoved,
            this,           &VoicePlayerDialog::onSliderMoved);
    connect(speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,       &VoicePlayerDialog::onSpeedChanged);
    connect(closeButton, &QPushButton::clicked,
            this,        &VoicePlayerDialog::accept);

    // Диагностика пути: абсолютный, размер, читаемость
    const QString absolutePath=QFileInfo(filePath).absoluteFilePath();
    const QFileInfo checkFile(absolutePath);
    qDebug() << "VoicePlayer file" << absolutePath
             << "exists" << checkFile.isFile()
             << "size" << checkFile.size()
             << "readable" << checkFile.isReadable();

    if(!checkFile.isFile())
    {
        timeLabel->setText(tr("File not found"));
        playButton->setEnabled(false);
        positionSlider->setEnabled(false);
        return;
    }

    if(useOpusPlayer)
    {
        if(!opusPlayer->setFile(absolutePath))
        {
            timeLabel->setText(tr("Can not open file"));
            playButton->setEnabled(false);
            positionSlider->setEnabled(false);
            return;
        }
    }
    else
    {
        player->setMedia(QUrl::fromLocalFile(absolutePath));
        player->setPlaybackRate(currentSpeed);
    }

    // Скорость только для бэкенда Qt: Opus-декодер темпа не имеет
    if(useOpusPlayer)
    {
        speedLabel->setEnabled(false);
        speedCombo->setEnabled(false);
        speedCombo->setToolTip(tr("Speed applies to MP3 format"));
    }
}


QList<double> VoicePlayerDialog::speedSteps(void)
{
    // Ступени выпадашки: 0.5..2.0 шаг 0.25
    return QList<double>() << 0.5 << 0.75 << 1.0 << 1.25
                           << 1.5 << 1.75 << 2.0;
}


double VoicePlayerDialog::snapSpeed(double speed)
{
    // Ближайшая ступень к значению
    double best=1.0;
    double bestDiff=qAbs(speed-best);

    foreach(double step, speedSteps())
    {
        const double diff=qAbs(speed-step);

        if(diff<bestDiff)
        {
            best=step;
            bestDiff=diff;
        }
    }

    return best;
}


void VoicePlayerDialog::onSpeedChanged(int index)
{
    currentSpeed=speedCombo->itemData(index).toDouble();

    // Скорость уходит в бэкенд Qt на ходу. Opus-декодер темпа
    // не имеет, его выпадашка заблокирована выше
    if(!useOpusPlayer)
        player->setPlaybackRate(currentSpeed);
}


QString VoicePlayerDialog::formatTime(qint64 ms, qint64 totalMs)
{
    return VoiceNoteHelper::formatDuration(ms)+
           QStringLiteral(" / ")+
           VoiceNoteHelper::formatDuration(totalMs);
}


void VoicePlayerDialog::onPlayToggled(bool checked)
{
    if(useOpusPlayer)
    {
        if(checked)
        {
            playButton->setText(tr("Pause"));

            // Повтор после конца: явная перемотка в начало перед play
            if(totalMs>0 && opusPlayer->positionMs()>=totalMs-200)
                opusPlayer->setPosition(0);

            opusPlayer->play();
        }
        else
        {
            playButton->setText(tr("Play"));
            opusPlayer->pause();
        }

        return;
    }

    qDebug() << "VoicePlayer play toggled" << checked
             << "pos" << player->position() << "total" << totalMs
             << "state" << player->state() << "mediaStatus" << player->mediaStatus();

    if(checked)
    {
        playButton->setText(tr("Pause"));

        // Повтор после конца: бэкенд с EOS сам не перезапускается,
        // явная перемотка в начало перед play
        if(totalMs>0 && player->position()>=totalMs-200)
            player->setPosition(0);

        player->play();
    }
    else
    {
        playButton->setText(tr("Play"));
        player->pause();
    }
}


void VoicePlayerDialog::onOpusFinished(void)
{
    // Доиграло до конца: кнопка снова Play, ползунок в начало.
    // Зеркало ветки StoppedState ниже, только через свой плеер
    playButton->blockSignals(true);
    playButton->setChecked(false);
    playButton->blockSignals(false);
    playButton->setText(tr("Play"));

    positionSlider->blockSignals(true);
    positionSlider->setValue(0);
    positionSlider->blockSignals(false);
    timeLabel->setText(formatTime(0, totalMs));
}


void VoicePlayerDialog::onPositionChanged(qint64 positionMs)
{
    if(!positionSlider->isSliderDown())
        positionSlider->setValue(static_cast<int>(positionMs));

    timeLabel->setText(formatTime(positionMs, totalMs));
}


void VoicePlayerDialog::onDurationChanged(qint64 durationMs)
{
    totalMs=durationMs;
    positionSlider->setRange(0, static_cast<int>(durationMs));
    timeLabel->setText(formatTime(0, totalMs));
}


void VoicePlayerDialog::onStateChanged(QMediaPlayer::State state)
{
    qDebug() << "VoicePlayer state" << state << "pos" << player->position();

    // Доиграло до конца: кнопка снова Play, ползунок в начало.
    // Сбрасывается только интерфейс: pause и setPosition в этом
    // месте роняют повторный запуск на части бэкендов. Позиция
    // плеера остается в конце, перед play мотается явно
    if(state==QMediaPlayer::StoppedState && playButton->isChecked())
    {
        playButton->blockSignals(true);
        playButton->setChecked(false);
        playButton->blockSignals(false);
        playButton->setText(tr("Play"));

        positionSlider->blockSignals(true);
        positionSlider->setValue(0);
        positionSlider->blockSignals(false);
        timeLabel->setText(formatTime(0, totalMs));
    }
}


void VoicePlayerDialog::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "VoicePlayer media status" << status << player->errorString();

    // Битый или неподдержанный файл: причина вместо тишины
    if(status==QMediaPlayer::InvalidMedia || status==QMediaPlayer::NoMedia)
    {
        if(player->error()!=QMediaPlayer::NoError)
            statusLabel->setText(player->errorString());
        else
            statusLabel->setText(tr("Can not open file"));

        playButton->setEnabled(false);
        positionSlider->setEnabled(false);
    }
    else if(status==QMediaPlayer::LoadedMedia || status==QMediaPlayer::BufferedMedia)
        statusLabel->clear();
}


void VoicePlayerDialog::onPlayerError(QMediaPlayer::Error error)
{
    Q_UNUSED(error);

    qDebug() << "VoicePlayer error" << player->errorString();
    statusLabel->setText(player->errorString());

    playButton->setEnabled(false);
    positionSlider->setEnabled(false);

    if(playButton->isChecked())
    {
        playButton->blockSignals(true);
        playButton->setChecked(false);
        playButton->blockSignals(false);
    }

    playButton->setText(tr("Play"));
}


void VoicePlayerDialog::onSliderMoved(int positionMs)
{
    if(useOpusPlayer)
        opusPlayer->setPosition(positionMs);
    else
        player->setPosition(positionMs);
}

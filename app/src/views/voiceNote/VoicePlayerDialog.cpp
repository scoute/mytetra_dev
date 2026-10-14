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

#include "VoicePlayerDialog.h"
#include "libraries/VoiceNoteHelper.h"


VoicePlayerDialog::VoicePlayerDialog(const QString &filePath, const QString &title,
                                     QWidget *parent) : QDialog(parent),
    totalMs(0)
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

    QHBoxLayout *playerLayout=new QHBoxLayout();
    playerLayout->addWidget(playButton);
    playerLayout->addWidget(positionSlider, 1);
    playerLayout->addWidget(timeLabel);

    QVBoxLayout *centralLayout=new QVBoxLayout(this);
    centralLayout->addWidget(titleLabel);
    centralLayout->addLayout(playerLayout);
    centralLayout->addWidget(statusLabel);
    centralLayout->addWidget(closeButton);
    setLayout(centralLayout);

    player=new QMediaPlayer(this);

    connect(playButton, &QPushButton::toggled,
            this,        &VoicePlayerDialog::onPlayToggled);
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
    connect(positionSlider, &QSlider::sliderMoved,
            this,           &VoicePlayerDialog::onSliderMoved);
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

    player->setMedia(QUrl::fromLocalFile(absolutePath));
}


QString VoicePlayerDialog::formatTime(qint64 ms, qint64 totalMs)
{
    return VoiceNoteHelper::formatDuration(ms)+
           QStringLiteral(" / ")+
           VoiceNoteHelper::formatDuration(totalMs);
}


void VoicePlayerDialog::onPlayToggled(bool checked)
{
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
    player->setPosition(positionMs);
}

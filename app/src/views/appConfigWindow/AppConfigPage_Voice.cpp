#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QDebug>

#include "AppConfigPage_Voice.h"
#include "models/appConfig/AppConfig.h"
#include "views/voiceNote/VoicePlayerDialog.h"


extern AppConfig mytetraConfig;


AppConfigPage_Voice::AppConfigPage_Voice(QWidget *parent) : ConfigPage(parent)
{
  setupUi();
  setupSignals();
  assembly();
}


AppConfigPage_Voice::~AppConfigPage_Voice()
{

}


void AppConfigPage_Voice::setupUi(void)
{
  qDebug() << "Create voice config page";

  // Формат по умолчанию из конфига, Opus если пусто
  formatBox=new QGroupBox(this);

  opusRadio=new QRadioButton(this);
  opusRadio->setText(tr("Opus (.opus)"));

  mp3Radio=new QRadioButton(this);
  mp3Radio->setText(tr("MP3 (.mp3)"));

  // Группы до установок: иначе autoExclusive сиблингов
  // сбрасывает соседний выбор
  QButtonGroup *formatGroup=new QButtonGroup(this);
  formatGroup->addButton(opusRadio);
  formatGroup->addButton(mp3Radio);

  if(mytetraConfig.get_voiceNoteFormat()==QStringLiteral("mp3"))
    mp3Radio->setChecked(true);
  else
    opusRadio->setChecked(true);

  // Скорость проигрывания по умолчанию из конфига
  speedBox=new QGroupBox(this);
  speedCombo=new QComboBox(this);

  for(double step : VoicePlayerDialog::speedSteps())
    speedCombo->addItem(QString::number(step)+QStringLiteral("x"), step);

  speedCombo->setCurrentIndex(speedCombo->findData(
      VoicePlayerDialog::snapSpeed(mytetraConfig.get_voiceNoteSpeed())));

  retranslateUi();
}


void AppConfigPage_Voice::setupSignals(void)
{

}


void AppConfigPage_Voice::assembly(void)
{
  QVBoxLayout *formatLayout=new QVBoxLayout();
  formatLayout->addWidget(opusRadio);
  formatLayout->addWidget(mp3Radio);
  formatBox->setLayout(formatLayout);

  QVBoxLayout *speedLayout=new QVBoxLayout();
  speedLayout->addWidget(speedCombo);
  speedBox->setLayout(speedLayout);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(formatBox);
  centralLayout->addWidget(speedBox);
  centralLayout->addStretch();
  setLayout(centralLayout);
}


// Прямые тексты новым языком
void AppConfigPage_Voice::retranslateUi(void)
{
  formatBox->setTitle(tr("Default format"));
  opusRadio->setText(tr("Opus (.opus)"));
  mp3Radio->setText(tr("MP3 (.mp3)"));

  speedBox->setTitle(tr("Default playback speed"));
}


// Перезапуск не нужен: диалог записи читает дефолты при открытии
int AppConfigPage_Voice::applyChanges(void)
{
  qDebug() << "Apply changes voice";

  const QString format=mp3Radio->isChecked() ? QStringLiteral("mp3")
                                             : QStringLiteral("opus");

  if(mytetraConfig.get_voiceNoteFormat()!=format)
    mytetraConfig.set_voiceNoteFormat(format);

  const double speed=speedCombo->currentData().toDouble();

  if(mytetraConfig.get_voiceNoteSpeed()!=speed)
    mytetraConfig.set_voiceNoteSpeed(speed);

  // Ключ качества оставлен для совместимости: выбора нет,
  // профили фиксированы. Старые значения молча игнорируются
  return 0;
}

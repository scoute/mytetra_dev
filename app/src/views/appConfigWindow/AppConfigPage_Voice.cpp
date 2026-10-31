#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDebug>

#include "AppConfigPage_Voice.h"
#include "models/appConfig/AppConfig.h"


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

  // Качество Opus по умолчанию из конфига, Middle если мусор
  qualityBox=new QGroupBox(this);

  qualitySuperLowRadio=new QRadioButton(this);
  qualityExtraLowRadio=new QRadioButton(this);
  qualityLowRadio=new QRadioButton(this);
  qualityMiddleRadio=new QRadioButton(this);
  qualityHighRadio=new QRadioButton(this);
  qualityExtraRadio=new QRadioButton(this);

  // Группы до установок: иначе autoExclusive сиблингов
  // сбрасывает соседний выбор
  QButtonGroup *qualityGroup=new QButtonGroup(this);
  qualityGroup->addButton(qualitySuperLowRadio);
  qualityGroup->addButton(qualityExtraLowRadio);
  qualityGroup->addButton(qualityLowRadio);
  qualityGroup->addButton(qualityMiddleRadio);
  qualityGroup->addButton(qualityHighRadio);
  qualityGroup->addButton(qualityExtraRadio);

  switch(mytetraConfig.get_voiceNoteQuality())
  {
  case 0:
    qualitySuperLowRadio->setChecked(true);
    break;

  case 1:
    qualityExtraLowRadio->setChecked(true);
    break;

  case 2:
    qualityLowRadio->setChecked(true);
    break;

  case 4:
    qualityHighRadio->setChecked(true);
    break;

  case 5:
    qualityExtraRadio->setChecked(true);
    break;

  case 3:
  default:
    qualityMiddleRadio->setChecked(true);
    break;
  }

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

  QVBoxLayout *qualityLayout=new QVBoxLayout();
  qualityLayout->addWidget(qualitySuperLowRadio);
  qualityLayout->addWidget(qualityExtraLowRadio);
  qualityLayout->addWidget(qualityLowRadio);
  qualityLayout->addWidget(qualityMiddleRadio);
  qualityLayout->addWidget(qualityHighRadio);
  qualityLayout->addWidget(qualityExtraRadio);
  qualityBox->setLayout(qualityLayout);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(formatBox);
  centralLayout->addWidget(qualityBox);
  centralLayout->addStretch();
  setLayout(centralLayout);
}


// Прямые тексты новым языком
void AppConfigPage_Voice::retranslateUi(void)
{
  formatBox->setTitle(tr("Default format"));
  opusRadio->setText(tr("Opus (.opus)"));
  mp3Radio->setText(tr("MP3 (.mp3)"));

  qualityBox->setTitle(tr("Default Opus quality"));
  qualitySuperLowRadio->setText(tr("Super low (12 kbit/s)"));
  qualityExtraLowRadio->setText(tr("Extra low (16 kbit/s)"));
  qualityLowRadio->setText(tr("Low (24 kbit/s)"));
  qualityMiddleRadio->setText(tr("Middle (32 kbit/s)"));
  qualityHighRadio->setText(tr("High (48 kbit/s)"));
  qualityExtraRadio->setText(tr("Extra high (64 kbit/s)"));

}


// Перезапуск не нужен: диалог записи читает дефолты при открытии
int AppConfigPage_Voice::applyChanges(void)
{
  qDebug() << "Apply changes voice";

  const QString format=mp3Radio->isChecked() ? QStringLiteral("mp3")
                                             : QStringLiteral("opus");

  if(mytetraConfig.get_voiceNoteFormat()!=format)
    mytetraConfig.set_voiceNoteFormat(format);

  int quality=3;

  if(qualitySuperLowRadio->isChecked())
    quality=0;
  else if(qualityExtraLowRadio->isChecked())
    quality=1;
  else if(qualityLowRadio->isChecked())
    quality=2;
  else if(qualityHighRadio->isChecked())
    quality=4;
  else if(qualityExtraRadio->isChecked())
    quality=5;

  if(mytetraConfig.get_voiceNoteQuality()!=quality)
    mytetraConfig.set_voiceNoteQuality(quality);

  return 0;
}

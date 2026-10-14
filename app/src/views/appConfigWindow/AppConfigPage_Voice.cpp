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

  // Качество по умолчанию из конфига, среднее если мусор
  qualityBox=new QGroupBox(this);

  lowRadio=new QRadioButton(this);
  lowRadio->setText(tr("Low"));

  mediumRadio=new QRadioButton(this);
  mediumRadio->setText(tr("Medium"));

  highRadio=new QRadioButton(this);
  highRadio->setText(tr("High"));

  QButtonGroup *qualityGroup=new QButtonGroup(this);
  qualityGroup->addButton(lowRadio);
  qualityGroup->addButton(mediumRadio);
  qualityGroup->addButton(highRadio);

  const int quality=mytetraConfig.get_voiceNoteQuality();

  if(quality==0)
    lowRadio->setChecked(true);
  else if(quality==2)
    highRadio->setChecked(true);
  else
    mediumRadio->setChecked(true);

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
  qualityLayout->addWidget(lowRadio);
  qualityLayout->addWidget(mediumRadio);
  qualityLayout->addWidget(highRadio);
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

  qualityBox->setTitle(tr("Default quality"));
  lowRadio->setText(tr("Low"));
  mediumRadio->setText(tr("Medium"));
  highRadio->setText(tr("High"));
}


// Перезапуск не нужен: диалог записи читает дефолты при открытии
int AppConfigPage_Voice::applyChanges(void)
{
  qDebug() << "Apply changes voice";

  const QString format=mp3Radio->isChecked() ? QStringLiteral("mp3")
                                             : QStringLiteral("opus");

  if(mytetraConfig.get_voiceNoteFormat()!=format)
    mytetraConfig.set_voiceNoteFormat(format);

  int quality=1;

  if(lowRadio->isChecked())
    quality=0;
  else if(highRadio->isChecked())
    quality=2;

  if(mytetraConfig.get_voiceNoteQuality()!=quality)
    mytetraConfig.set_voiceNoteQuality(quality);

  return 0;
}

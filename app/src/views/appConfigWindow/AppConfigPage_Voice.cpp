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

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(formatBox);
  centralLayout->addStretch();
  setLayout(centralLayout);
}


// Прямые тексты новым языком
void AppConfigPage_Voice::retranslateUi(void)
{
  formatBox->setTitle(tr("Default format"));
  opusRadio->setText(tr("Opus (.opus)"));
  mp3Radio->setText(tr("MP3 (.mp3)"));

}


// Перезапуск не нужен: диалог записи читает дефолты при открытии
int AppConfigPage_Voice::applyChanges(void)
{
  qDebug() << "Apply changes voice";

  const QString format=mp3Radio->isChecked() ? QStringLiteral("mp3")
                                             : QStringLiteral("opus");

  if(mytetraConfig.get_voiceNoteFormat()!=format)
    mytetraConfig.set_voiceNoteFormat(format);

  // Ключ качества оставлен для совместимости: выбора нет,
  // профили фиксированы. Старые значения молча игнорируются
  return 0;
}

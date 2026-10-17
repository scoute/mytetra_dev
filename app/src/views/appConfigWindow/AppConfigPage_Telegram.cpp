#include <QWidget>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QGroupBox>

#include "AppConfigPage_Telegram.h"
#include "models/appConfig/AppConfig.h"


extern AppConfig mytetraConfig;


AppConfigPage_Telegram::AppConfigPage_Telegram(QWidget *parent) : ConfigPage(parent)
{
  setupUi();
  setupSignals();
  assembly();
}


AppConfigPage_Telegram::~AppConfigPage_Telegram()
{

}


void AppConfigPage_Telegram::setupUi(void)
{
  qDebug() << "Create telegram config page";

  // Раскладка заметок импорта: месяц (по умолчанию) или сообщение.
  // Сообщения кладутся в ветки чат-год-месяц-день: MCP не умеет
  // резать большие заметки, сортировка потом пойдет по мелким
  monthNote=new QRadioButton(this);
  monthNote->setText(tr("One note per month: chat - year (default)"));

  dayNote=new QRadioButton(this);
  dayNote->setText(tr("One note per day: chat - year - month"));

  messageNote=new QRadioButton(this);
  messageNote->setText(tr("One note per message: chat - year - month - day"));

  monthNote->setChecked(false);
  dayNote->setChecked(false);
  messageNote->setChecked(false);

  if(mytetraConfig.get_telegramImportMode()==QStringLiteral("message"))
    messageNote->setChecked(true);
  else if(mytetraConfig.get_telegramImportMode()==QStringLiteral("day"))
    dayNote->setChecked(true);
  else
    monthNote->setChecked(true);

  layoutBox=new QGroupBox(this);
  layoutBox->setTitle(tr("Notes layout"));
}


void AppConfigPage_Telegram::setupSignals(void)
{

}


void AppConfigPage_Telegram::assembly(void)
{
  QVBoxLayout *layoutLayout=new QVBoxLayout;
  layoutLayout->addWidget(monthNote);
  layoutLayout->addWidget(dayNote);
  layoutLayout->addWidget(messageNote);
  layoutBox->setLayout(layoutLayout);

  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(layoutBox);
  centralLayout->addStretch();

  setLayout(centralLayout);
}


void AppConfigPage_Telegram::retranslateUi(void)
{
  monthNote->setText(tr("One note per month: chat - year (default)"));
  dayNote->setText(tr("One note per day: chat - year - month"));
  messageNote->setText(tr("One note per message: chat - year - month - day"));

  layoutBox->setTitle(tr("Notes layout"));
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Telegram::applyChanges(void)
{
  qDebug() << "Apply changes telegram";

  int result=0;

  // Выбор пишется в конфиг и применяется со следующего импорта,
  // перезапуск не нужен
  QString mode=QStringLiteral("month");

  if(messageNote->isChecked())
    mode=QStringLiteral("message");
  else if(dayNote->isChecked())
    mode=QStringLiteral("day");

  if(mytetraConfig.get_telegramImportMode()!=mode)
    mytetraConfig.set_telegramImportMode(mode);

  return result;
}

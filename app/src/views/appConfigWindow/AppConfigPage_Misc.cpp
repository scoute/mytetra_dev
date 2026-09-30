#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>

#include "AppConfigPage_Misc.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ConfigEditorHelper.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


AppConfigPage_Misc::AppConfigPage_Misc(QWidget *parent) : ConfigPage(parent)
{
  setupUi();
  setupSignals();
  assembly();
}


AppConfigPage_Misc::~AppConfigPage_Misc()
{

}


void AppConfigPage_Misc::setupUi(void)
{
  qDebug() << "Create misc config page";

  // Блок настройки отображения отладочных сообщений в консоли
  printDebugMessages=new QCheckBox(this);
  printDebugMessages->setText(tr("Print debug messages to console"));
  printDebugMessages->setChecked(mytetraConfig.get_printdebugmessages());

  // Разрешение/запрещение лога действий
  enableActionLog=new QCheckBox(this);
  enableActionLog->setText(tr("Enable action logging (experimental)"));
  enableActionLog->setChecked(mytetraConfig.getEnableLogging());

  // Разрешение/запрещение создавать пустую запись (без текста)
  enableCreateEmptyRecord=new QCheckBox(this);
  enableCreateEmptyRecord->setText(tr("Create empty note enable"));
  enableCreateEmptyRecord->setChecked(mytetraConfig.getEnableCreateEmptyRecord());

  // Разрешение/запрещение игнорировать ошибки самоподписанных SSL-сертификатов
  // при скачивании файлов и картинок. Нужно для сайтов с самоподписанными
  // сертификатами, но снижает защищенность, поэтому по умолчанию выключено
  ignoreSelfSignedSslErrors=new QCheckBox(this);
  ignoreSelfSignedSslErrors->setText(tr("Ignore self-signed SSL certificate errors when downloading (less secure)"));
  ignoreSelfSignedSslErrors->setChecked(mytetraConfig.getIgnoreSelfSignedSslErrors());

  // Лимиты клиппера: сколько картинок забирать в одну заметку
  // и максимальный размер одной картинки. Пропущенные сверх лимитов
  // картинки остаются внешними ссылками, о чем клиппер сообщает сразу
  clipperMaxImages=new QSpinBox(this);
  clipperMaxImages->setMinimum(1);
  clipperMaxImages->setMaximum(1000);
  clipperMaxImages->setValue(mytetraConfig.get_clipperMaxImages());

  clipperMaxImageSizeMb=new QSpinBox(this);
  clipperMaxImageSizeMb->setMinimum(1);
  clipperMaxImageSizeMb->setMaximum(100);
  clipperMaxImageSizeMb->setValue(mytetraConfig.get_clipperMaxImageSizeMb());
  clipperMaxImageSizeMb->setSuffix(tr(" MB"));

  // Кнопка редактирования файла конфигурации MyTetra
  editMyTetraConfigFile=new QPushButton(this);
  editMyTetraConfigFile->setText(tr("Edit config file"));
  editMyTetraConfigFile->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));
}


void AppConfigPage_Misc::setupSignals(void)
{
  connect(editMyTetraConfigFile, &QPushButton::clicked, this, &AppConfigPage_Misc::onClickedEditMyTetraConfigFile);
}


void AppConfigPage_Misc::assembly(void)
{
  // Группировщик виджетов для опасной зоны
  dangerBox=new QGroupBox(this);
  dangerBox->setTitle(tr("Danger actions (Attention!)"));

  // Виджеты вставляются в группировщик опасной зоны
  QVBoxLayout *dangerLayout = new QVBoxLayout;
  dangerLayout->addWidget(editMyTetraConfigFile);
  dangerBox->setLayout(dangerLayout);


  // Группировщик лимитов клиппера
  QGroupBox *clipperBox=new QGroupBox(this);
  clipperBox->setTitle(tr("Clipper"));

  QLabel *clipperMaxImagesLabel=new QLabel(tr("Maximum images per note:"), this);
  QLabel *clipperMaxImageSizeLabel=new QLabel(tr("Maximum size of one image:"), this);

  QHBoxLayout *clipperImagesLayout=new QHBoxLayout;
  clipperImagesLayout->addWidget(clipperMaxImagesLabel);
  clipperImagesLayout->addWidget(clipperMaxImages);
  clipperImagesLayout->addStretch();

  QHBoxLayout *clipperSizeLayout=new QHBoxLayout;
  clipperSizeLayout->addWidget(clipperMaxImageSizeLabel);
  clipperSizeLayout->addWidget(clipperMaxImageSizeMb);
  clipperSizeLayout->addStretch();

  QVBoxLayout *clipperLayout=new QVBoxLayout;
  clipperLayout->addLayout(clipperImagesLayout);
  clipperLayout->addLayout(clipperSizeLayout);
  clipperBox->setLayout(clipperLayout);


  // Собирается основной слой
  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(printDebugMessages);
  centralLayout->addWidget(enableActionLog);
  centralLayout->addWidget(enableCreateEmptyRecord);
  centralLayout->addWidget(ignoreSelfSignedSslErrors);
  centralLayout->addWidget(clipperBox);
  centralLayout->addWidget(dangerBox);
  centralLayout->addStretch();

  // Основной слой устанавливается
  setLayout(centralLayout);
}


void AppConfigPage_Misc::onClickedEditMyTetraConfigFile(void)
{
  // Сбрасываются в файл конфига все возможные изменения, которые, возможно еще не были записаны
  mytetraConfig.sync();

  ConfigEditorHelper::editConfigFile( globalParameters.getWorkDirectory()+"/conf.ini", 0.8 );
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Misc::applyChanges(void)
{
  qDebug() << "Apply changes misc";

  int result=0;

  // Сохраняется настройка отображения отладочных сообщений в консоли
  if(mytetraConfig.get_printdebugmessages()!=printDebugMessages->isChecked())
    mytetraConfig.set_printdebugmessages(printDebugMessages->isChecked());

  // Сохраняется настройка разрешения/запрещения лога действий
  if(mytetraConfig.getEnableLogging()!=enableActionLog->isChecked())
  {
    mytetraConfig.setEnableLogging(enableActionLog->isChecked());
    result=1;
  }

  // Сохраняется настройка возможности создания записи, не содержащей текст
  if(mytetraConfig.getEnableCreateEmptyRecord()!=enableCreateEmptyRecord->isChecked())
    mytetraConfig.setEnableCreateEmptyRecord(enableCreateEmptyRecord->isChecked());

  // Сохраняется настройка игнорирования ошибок самоподписанных SSL-сертификатов
  if(mytetraConfig.getIgnoreSelfSignedSslErrors()!=ignoreSelfSignedSslErrors->isChecked())
    mytetraConfig.setIgnoreSelfSignedSslErrors(ignoreSelfSignedSslErrors->isChecked());

  // Сохраняются лимиты клиппера
  if(mytetraConfig.get_clipperMaxImages()!=clipperMaxImages->value())
    mytetraConfig.set_clipperMaxImages(clipperMaxImages->value());

  if(mytetraConfig.get_clipperMaxImageSizeMb()!=clipperMaxImageSizeMb->value())
    mytetraConfig.set_clipperMaxImageSizeMb(clipperMaxImageSizeMb->value());

  return result;
}

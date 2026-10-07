#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QToolButton>
#include <QColorDialog>
#include <QPixmap>
#include <QColor>

#include "AppConfigPage_Misc.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ConfigEditorHelper.h"
#include "libraries/helpers/ObjectHelper.h"
#include "views/favoritesPanel/FavoritesPanel.h"


extern QObject *pMainWindow;


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

  // Панель избранного над деревом
  enableFavoritesPanel=new QCheckBox(this);
  enableFavoritesPanel->setText(tr("Show favorites panel"));
  enableFavoritesPanel->setChecked(mytetraConfig.get_favoritesEnabled());

  // Разрешение/запрещение создавать пустую запись (без текста)
  enableCreateEmptyRecord=new QCheckBox(this);
  enableCreateEmptyRecord->setText(tr("Create empty note enable"));
  enableCreateEmptyRecord->setChecked(mytetraConfig.getEnableCreateEmptyRecord());

  // Цвет закраски секрета: кнопка с квадратиком-образцом,
  // диалог дает всю радугу включая кастомные цвета
  secretColorLabel=new QLabel(this);
  secretColorLabel->setText(tr("Secret color: "));

  secretColorButton=new QToolButton(this);
  secretColor=new QColor();
  this->setColorForSecretButton(QColor(mytetraConfig.get_secretColor()));

  // Кнопка редактирования файла конфигурации MyTetra
  editMyTetraConfigFile=new QPushButton(this);
  editMyTetraConfigFile->setText(tr("Edit config file"));
  editMyTetraConfigFile->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));
}


void AppConfigPage_Misc::setupSignals(void)
{
  connect(editMyTetraConfigFile, &QPushButton::clicked, this, &AppConfigPage_Misc::onClickedEditMyTetraConfigFile);
  connect(secretColorButton, &QToolButton::clicked, this, &AppConfigPage_Misc::onClickedSecretColor);
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


  // Слой для надписи цвета секрета и кнопки выбора
  QHBoxLayout *secretColorLayout=new QHBoxLayout();
  secretColorLayout->addWidget(secretColorLabel);
  secretColorLayout->addWidget(secretColorButton);
  secretColorLayout->addStretch();

  // Собирается основной слой
  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(printDebugMessages);
  centralLayout->addWidget(enableActionLog);
  centralLayout->addWidget(enableCreateEmptyRecord);
  centralLayout->addWidget(enableFavoritesPanel);
  centralLayout->addLayout(secretColorLayout);
  centralLayout->addWidget(dangerBox);
  centralLayout->addStretch();

  // Основной слой устанавливается
  setLayout(centralLayout);
}


void AppConfigPage_Misc::setColorForSecretButton(QColor iColor)
{
  // Квадратик на кнопке выбора цвета
  QPixmap pix(16, 16);
  pix.fill(iColor.rgb());
  secretColorButton->setIcon(pix);

  *secretColor=iColor;
}


void AppConfigPage_Misc::onClickedSecretColor()
{
  // Диалог запроса цвета со всей радугой
  QColor selectedColor=QColorDialog::getColor(*secretColor, this);

  // Если цвет выбран, и он правильный
  if(selectedColor.isValid())
    this->setColorForSecretButton(selectedColor);
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

  // Сохраняется показ панели избранного
  if(mytetraConfig.get_favoritesEnabled()!=enableFavoritesPanel->isChecked())
  {
    mytetraConfig.set_favoritesEnabled(enableFavoritesPanel->isChecked());

    // Панель применяется живьем без перезапуска
    FavoritesPanel *favoritesPanel=find_object<FavoritesPanel>("favoritesPanel");
    if(favoritesPanel!=nullptr)
      favoritesPanel->setVisible(enableFavoritesPanel->isChecked());
  }

  // Сохраняется цвет закраски секрета
  if(mytetraConfig.get_secretColor()!=secretColor->name())
    mytetraConfig.set_secretColor(secretColor->name());

  return result;
}

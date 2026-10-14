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
#include "views/record/MetaEditor.h"


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

  // Цвет закраски секрета: кнопка с квадратиком-образцом,
  // диалог дает всю радугу включая кастомные цвета
  secretColorLabel=new QLabel(this);
  secretColorLabel->setText(tr("Secret color: "));

  secretColorButton=new QToolButton(this);
  secretColor=new QColor();
  this->setColorForSecretButton(QColor(mytetraConfig.get_secretColor()));

  // Цвет фона поля заметки: кнопка с образцом и сброс к цвету темы.
  // Пустая настройка означает цвет темы, тогда на кнопке показан
  // действующий цвет темы
  notePaperColorLabel=new QLabel(this);
  notePaperColorLabel->setText(tr("Note paper color: "));

  notePaperColorButton=new QToolButton(this);
  notePaperColor=new QColor();
  notePaperColorIsTheme=mytetraConfig.get_notePaperColor().isEmpty();
  this->setColorForNotePaperButton(
    QColor(MetaEditor::effectiveNotePaperColor(mytetraConfig.getInterfaceTheme(),
                                              mytetraConfig.get_notePaperColor())));

  notePaperColorReset=new QPushButton(this);
  notePaperColorReset->setText(tr("Reset"));

  // Цвет шрифта поля заметки: кнопка с образцом и сброс к цвету темы.
  // Пустая настройка означает цвет темы, тогда на кнопке показан
  // действующий цвет темы
  noteFontColorLabel=new QLabel(this);
  noteFontColorLabel->setText(tr("Note font color: "));

  noteFontColorButton=new QToolButton(this);
  noteFontColor=new QColor();
  noteFontColorIsTheme=mytetraConfig.get_noteFontColor().isEmpty();
  this->setColorForNoteFontButton(
    QColor(MetaEditor::effectiveNoteFontColor(mytetraConfig.getInterfaceTheme(),
                                              mytetraConfig.get_noteFontColor())));

  noteFontColorReset=new QPushButton(this);
  noteFontColorReset->setText(tr("Reset"));

  // Кнопка редактирования файла конфигурации MyTetra
  editMyTetraConfigFile=new QPushButton(this);
  editMyTetraConfigFile->setText(tr("Edit config file"));
  editMyTetraConfigFile->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));
}


void AppConfigPage_Misc::setupSignals(void)
{
  connect(editMyTetraConfigFile, &QPushButton::clicked, this, &AppConfigPage_Misc::onClickedEditMyTetraConfigFile);
  connect(secretColorButton, &QToolButton::clicked, this, &AppConfigPage_Misc::onClickedSecretColor);
  connect(notePaperColorButton, &QToolButton::clicked, this, &AppConfigPage_Misc::onClickedNotePaperColor);
  connect(notePaperColorReset, &QPushButton::clicked, this, &AppConfigPage_Misc::onClickedNotePaperColorReset);
  connect(noteFontColorButton, &QToolButton::clicked, this, &AppConfigPage_Misc::onClickedNoteFontColor);
  connect(noteFontColorReset, &QPushButton::clicked, this, &AppConfigPage_Misc::onClickedNoteFontColorReset);
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

  // Слой для надписи цвета бумаги, кнопки выбора и сброса
  QHBoxLayout *notePaperColorLayout=new QHBoxLayout();
  notePaperColorLayout->addWidget(notePaperColorLabel);
  notePaperColorLayout->addWidget(notePaperColorButton);
  notePaperColorLayout->addWidget(notePaperColorReset);
  notePaperColorLayout->addStretch();

  // Слой для надписи цвета шрифта, кнопки выбора и сброса
  QHBoxLayout *noteFontColorLayout=new QHBoxLayout();
  noteFontColorLayout->addWidget(noteFontColorLabel);
  noteFontColorLayout->addWidget(noteFontColorButton);
  noteFontColorLayout->addWidget(noteFontColorReset);
  noteFontColorLayout->addStretch();

  // Собирается основной слой
  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(printDebugMessages);
  centralLayout->addWidget(enableActionLog);
  centralLayout->addWidget(enableCreateEmptyRecord);
  centralLayout->addLayout(secretColorLayout);
  centralLayout->addLayout(notePaperColorLayout);
  centralLayout->addLayout(noteFontColorLayout);
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


void AppConfigPage_Misc::setColorForNotePaperButton(QColor iColor)
{
  // Квадратик на кнопке выбора цвета
  QPixmap pix(16, 16);
  pix.fill(iColor.rgb());
  notePaperColorButton->setIcon(pix);

  *notePaperColor=iColor;
}


void AppConfigPage_Misc::onClickedNotePaperColor()
{
  // Диалог запроса цвета со всей радугой
  QColor selectedColor=QColorDialog::getColor(*notePaperColor, this);

  // Если цвет выбран, и он правильный
  if(selectedColor.isValid())
  {
    notePaperColorIsTheme=false;
    this->setColorForNotePaperButton(selectedColor);
  }
}


void AppConfigPage_Misc::onClickedNotePaperColorReset()
{
  // Возврат к цвету темы: на кнопке показывается действующий цвет темы,
  // в конфиг при Apply запишется пустота
  notePaperColorIsTheme=true;

  this->setColorForNotePaperButton(
    QColor(MetaEditor::effectiveNotePaperColor(mytetraConfig.getInterfaceTheme(), "")));
}


void AppConfigPage_Misc::setColorForNoteFontButton(QColor iColor)
{
  // Квадратик на кнопке выбора цвета
  QPixmap pix(16, 16);
  pix.fill(iColor.rgb());
  noteFontColorButton->setIcon(pix);

  *noteFontColor=iColor;
}


void AppConfigPage_Misc::onClickedNoteFontColor()
{
  // Диалог запроса цвета со всей радугой
  QColor selectedColor=QColorDialog::getColor(*noteFontColor, this);

  // Если цвет выбран, и он правильный
  if(selectedColor.isValid())
  {
    noteFontColorIsTheme=false;
    this->setColorForNoteFontButton(selectedColor);
  }
}


void AppConfigPage_Misc::onClickedNoteFontColorReset()
{
  // Возврат к цвету темы: на кнопке показывается действующий цвет темы,
  // в конфиг при Apply запишется пустота
  noteFontColorIsTheme=true;

  this->setColorForNoteFontButton(
    QColor(MetaEditor::effectiveNoteFontColor(mytetraConfig.getInterfaceTheme(), "")));
}


void AppConfigPage_Misc::retranslateUi(void)
{
  printDebugMessages->setText(tr("Print debug messages to console"));
  enableActionLog->setText(tr("Enable action logging (experimental)"));
  enableCreateEmptyRecord->setText(tr("Create empty note enable"));

  secretColorLabel->setText(tr("Secret color: "));

  notePaperColorLabel->setText(tr("Note paper color: "));
  notePaperColorReset->setText(tr("Reset"));

  noteFontColorLabel->setText(tr("Note font color: "));
  noteFontColorReset->setText(tr("Reset"));

  editMyTetraConfigFile->setText(tr("Edit config file"));
  dangerBox->setTitle(tr("Danger actions (Attention!)"));
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

  // Сохраняется цвет закраски секрета
  if(mytetraConfig.get_secretColor()!=secretColor->name())
    mytetraConfig.set_secretColor(secretColor->name());

  // Сохраняется цвет фона поля заметки и применяется сразу.
  // Режим цвета темы пишется пустотой, иначе пишется выбранный цвет
  if(notePaperColorIsTheme)
  {
    if(!mytetraConfig.get_notePaperColor().isEmpty())
      mytetraConfig.set_notePaperColor("");
  }
  else if(mytetraConfig.get_notePaperColor()!=notePaperColor->name())
    mytetraConfig.set_notePaperColor(notePaperColor->name());

  // Сохраняется цвет шрифта поля заметки и применяется сразу.
  // Режим цвета темы пишется пустотой, иначе пишется выбранный цвет
  if(noteFontColorIsTheme)
  {
    if(!mytetraConfig.get_noteFontColor().isEmpty())
      mytetraConfig.set_noteFontColor("");
  }
  else if(mytetraConfig.get_noteFontColor()!=noteFontColor->name())
    mytetraConfig.set_noteFontColor(noteFontColor->name());

  MetaEditor *editorScreen=find_object<MetaEditor>("editorScreen");

  if(editorScreen!=nullptr)
    editorScreen->applyNoteColors();

  return result;
}

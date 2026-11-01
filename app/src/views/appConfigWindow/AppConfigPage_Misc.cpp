#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>

#include "AppConfigPage_Misc.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ConfigEditorHelper.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigGeneral.h"
#include "models/appConfig/AppConfigServices.h"
#include "models/appConfig/AppConfigSync.h"
#include "models/appConfig/AppConfigStorage.h"

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
  printDebugMessages->setChecked(AppContext::instance().appConfig()->generalSection()->get_printdebugmessages());

  // Разрешение/запрещение лога действий
  enableActionLog=new QCheckBox(this);
  enableActionLog->setText(tr("Enable action logging (experimental)"));
  enableActionLog->setChecked(AppContext::instance().appConfig()->storageSection()->getEnableLogging());

  // Разрешение/запрещение создавать пустую запись (без текста)
  enableCreateEmptyRecord=new QCheckBox(this);
  enableCreateEmptyRecord->setText(tr("Create empty note enable"));
  enableCreateEmptyRecord->setChecked(AppContext::instance().appConfig()->recordTableSection()->getEnableCreateEmptyRecord());

  // Умное автосохранение: срабатывает только при реальных правках
  autosaveBox=new QGroupBox(this);
  autosaveBox->setTitle(tr("Autosave"));
  autosaveEnable=new QCheckBox(tr("Enable autosave, min"), this);
  autosaveEnable->setChecked(AppContext::instance().appConfig()->syncSection()->get_autosaveEnable());
  autosaveInterval=new QSpinBox(this);
  autosaveInterval->setValue(AppContext::instance().appConfig()->syncSection()->get_autosaveInterval());
  autosaveInterval->setRange(1, 999);
  autosaveInterval->setEnabled(AppContext::instance().appConfig()->syncSection()->get_autosaveEnable());

  // Сетевые загрузки: ослабление проверки TLS только по явному согласию.
  // По умолчанию невалидный/самоподписанный сертификат отклоняется
  networkBox=new QGroupBox(this);
  networkBox->setTitle(tr("Network and certificates"));
  tlsIgnoreInvalidCertificates=new QCheckBox(this);
  tlsIgnoreInvalidCertificates->setText(
    tr("Allow loading with invalid or self-signed TLS certificates"));
  tlsIgnoreInvalidCertificates->setChecked(
    AppContext::instance().appConfig()->servicesSection()->get_tlsIgnoreInvalidCertificates());
  tlsIgnoreInvalidCertificates->setToolTip(
    tr("Disables certificate verification for images and files downloaded "
       "from the network. This reduces protection against traffic "
       "substitution. Enable only if you understand the risk."));
  tlsIgnoreInvalidCertificates->setWhatsThis(
    tr("Normally a site with an expired, self-signed or otherwise invalid "
       "TLS certificate is refused. This option allows downloading images "
       "and files from such sites anyway, but an attacker on the network "
       "could substitute the content. Default: off."));

  // Кнопка редактирования файла конфигурации MyTetra
  editMyTetraConfigFile=new QPushButton(this);
  editMyTetraConfigFile->setText(tr("Edit config file"));
  editMyTetraConfigFile->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed, QSizePolicy::ToolButton));
}

void AppConfigPage_Misc::setupSignals(void)
{
  connect(editMyTetraConfigFile, &QPushButton::clicked, this, &AppConfigPage_Misc::onClickedEditMyTetraConfigFile);
  connect(autosaveEnable, &QCheckBox::toggled, autosaveInterval, &QSpinBox::setEnabled);
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

  // Автосохранение: галочка и минуты в одну строку
  QHBoxLayout *autosaveLayout=new QHBoxLayout;
  autosaveLayout->addWidget(autosaveEnable);
  autosaveLayout->addWidget(autosaveInterval);
  autosaveLayout->addStretch(1);
  autosaveBox->setLayout(autosaveLayout);

  // Сетевая группа: одна галочка с явным предупреждением
  QVBoxLayout *networkLayout=new QVBoxLayout;
  networkLayout->addWidget(tlsIgnoreInvalidCertificates);
  networkBox->setLayout(networkLayout);

  // Собирается основной слой
  QVBoxLayout *centralLayout=new QVBoxLayout();
  centralLayout->addWidget(printDebugMessages);
  centralLayout->addWidget(enableActionLog);
  centralLayout->addWidget(enableCreateEmptyRecord);
  centralLayout->addWidget(autosaveBox);
  centralLayout->addWidget(networkBox);
  centralLayout->addWidget(dangerBox);
  centralLayout->addStretch();

  // Основной слой устанавливается
  setLayout(centralLayout);
}

void AppConfigPage_Misc::onClickedEditMyTetraConfigFile(void)
{
  // Сбрасываются в файл конфига все возможные изменения, которые, возможно еще не были записаны
  AppContext::instance().appConfig()->sync();

  ConfigEditorHelper::editConfigFile( AppContext::instance().globalParameters()->getWorkDirectory()+"/conf.ini", 0.8 );
}

void AppConfigPage_Misc::retranslateUi(void)
{
  printDebugMessages->setText(tr("Print debug messages to console"));
  enableActionLog->setText(tr("Enable action logging (experimental)"));
  enableCreateEmptyRecord->setText(tr("Create empty note enable"));
  autosaveBox->setTitle(tr("Autosave"));
  autosaveEnable->setText(tr("Enable autosave, min"));

  networkBox->setTitle(tr("Network and certificates"));
  tlsIgnoreInvalidCertificates->setText(
    tr("Allow loading with invalid or self-signed TLS certificates"));
  tlsIgnoreInvalidCertificates->setToolTip(
    tr("Disables certificate verification for images and files downloaded "
       "from the network. This reduces protection against traffic "
       "substitution. Enable only if you understand the risk."));
  tlsIgnoreInvalidCertificates->setWhatsThis(
    tr("Normally a site with an expired, self-signed or otherwise invalid "
       "TLS certificate is refused. This option allows downloading images "
       "and files from such sites anyway, but an attacker on the network "
       "could substitute the content. Default: off."));

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
  if(AppContext::instance().appConfig()->generalSection()->get_printdebugmessages()!=printDebugMessages->isChecked())
    AppContext::instance().appConfig()->generalSection()->set_printdebugmessages(printDebugMessages->isChecked());

  // Сохраняется настройка разрешения/запрещения лога действий
  if(AppContext::instance().appConfig()->storageSection()->getEnableLogging()!=enableActionLog->isChecked())
  {
    AppContext::instance().appConfig()->storageSection()->setEnableLogging(enableActionLog->isChecked());
    result=1;
  }

  // Сохраняется настройка возможности создания записи, не содержащей текст
  if(AppContext::instance().appConfig()->recordTableSection()->getEnableCreateEmptyRecord()!=enableCreateEmptyRecord->isChecked())
    AppContext::instance().appConfig()->recordTableSection()->setEnableCreateEmptyRecord(enableCreateEmptyRecord->isChecked());

  // Умное автосохранение: интервал подхватится сам на следующем
  // срабатывании (таймер перечитывает конфиг каждый раз)
  if(AppContext::instance().appConfig()->syncSection()->get_autosaveEnable()!=autosaveEnable->isChecked())
    AppContext::instance().appConfig()->syncSection()->set_autosaveEnable(autosaveEnable->isChecked());

  if(AppContext::instance().appConfig()->syncSection()->get_autosaveInterval()!=autosaveInterval->value())
    AppContext::instance().appConfig()->syncSection()->set_autosaveInterval(autosaveInterval->value());

  // Сетевые загрузки: значение читается при каждом запросе,
  // перезапуск не нужен
  if(AppContext::instance().appConfig()->servicesSection()->get_tlsIgnoreInvalidCertificates()!=tlsIgnoreInvalidCertificates->isChecked())
    AppContext::instance().appConfig()->servicesSection()->set_tlsIgnoreInvalidCertificates(tlsIgnoreInvalidCertificates->isChecked());

  return result;
}

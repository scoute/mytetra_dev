#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

#include "AppConfigPage_Synchro.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/helpers/MessageHelper.h"
#include "libraries/AppContext.h"

AppConfigPage_Synchro::AppConfigPage_Synchro(QWidget *parent) : ConfigPage(parent)
{
    qDebug() << "Create sync config page";

    setupUi();
    setupSignals();
    assembly();
}

void AppConfigPage_Synchro::setupUi(void)
{
    synchroCommand=new QLineEdit(this);
    synchroCommand->setText(AppContext::instance().appConfig()->get_synchrocommand());
    synchroCommand->setCursorPosition(0);

    synchroOnStartup=new QCheckBox(this);
    synchroOnStartup->setText(tr("Synchronize at MyTetra startup"));
    synchroOnStartup->setChecked(AppContext::instance().appConfig()->get_synchroonstartup());

    synchroOnExit=new QCheckBox(this);
    synchroOnExit->setText(tr("Synchronize when exit from MyTetra"));
    synchroOnExit->setChecked(AppContext::instance().appConfig()->get_synchroonexit());

    synchroOnPeriodic=new QCheckBox(this);
    synchroOnPeriodic->setText(tr("Enable periodic background synchronize"));
    synchroOnPeriodic->setChecked(AppContext::instance().appConfig()->getEnablePeriodicSyncro());

    // Область ввода команды синхронизации
    commandText=new QLabel(this);
    commandText->setText(tr("Synchronization command"));

    commandAboutText=new QLabel(this);
    commandAboutText->setText(tr("Use <b>%a</b> macro for get database directory path"));
    commandAboutText->setWordWrap(true);

    // Группировщик виджетов для настройки автоматического старта синхронизации
    synchroOnBox=new QGroupBox(this);
    synchroOnBox->setTitle(tr("Automatic start synchronization"));

    // Виджеты автоматического старта синхронизации вставляются в группировщик
    synchroOnLayout=new QVBoxLayout();
    synchroOnLayout->addWidget( synchroOnStartup );
    synchroOnLayout->addWidget( synchroOnExit );
    synchroOnLayout->addWidget( synchroOnPeriodic );

    synchroPeriodText=new QLabel(this);
    synchroPeriodText->setText( tr("Synchronization period: ") );

    synchroPeriod=new QSpinBox(this);
    synchroPeriod->setMaximum( 100000 ); // Максимальная граница должна устанавливаться перед заданием значения
    synchroPeriod->setValue( AppContext::instance().appConfig()->getPeriodicSyncroPeriod() );

    synchroPeriodPostfix=new QLabel(this);
    synchroPeriodPostfix->setText( tr("sec.") );

    synchroPeriodLayout=new QHBoxLayout();
    synchroPeriodLayout->addWidget( synchroPeriodText );
    synchroPeriodLayout->addWidget( synchroPeriod );
    synchroPeriodLayout->addWidget( synchroPeriodPostfix );
    synchroPeriodLayout->addStretch();
    synchroOnLayout->addLayout( synchroPeriodLayout );

    synchroOnBox->setLayout( synchroOnLayout );

    // Начальное состояние зависимых параметров (активны-неактивны) для периодической синхронизации
    onEnablePeriodicSyncro( AppContext::instance().appConfig()->getEnablePeriodicSyncro() );

    // Виджеты настройки периодической проверки базы на предмет изменений
    enablePeriodicCheckBase=new QCheckBox(this);
    enablePeriodicCheckBase->setText( tr("Periodic check database tree for change at 3rd-party app") ); // Периодически проверять дерево базы на предмет изменения сторонней программой
    enablePeriodicCheckBase->setChecked( AppContext::instance().appConfig()->getEnablePeriodicCheckBase() );

    checkBasePeriodText=new QLabel(this);
    checkBasePeriodText->setText( tr("Checking period: ") );

    checkBasePeriod=new QSpinBox(this);
    checkBasePeriod->setMaximum( 100000 ); // Максимальная граница должна устанавливаться перед заданием значения
    checkBasePeriod->setValue( AppContext::instance().appConfig()->getCheckBasePeriod() );

    checkBasePeriodPostfix=new QLabel(this);
    checkBasePeriodPostfix->setText( tr("sec.") );

    checkBasePeriodLayout=new QHBoxLayout();
    checkBasePeriodLayout->addWidget( checkBasePeriodText );
    checkBasePeriodLayout->addWidget( checkBasePeriod );
    checkBasePeriodLayout->addWidget( checkBasePeriodPostfix );
    checkBasePeriodLayout->addStretch();

    enablePeriodicCheckMessage=new QCheckBox(this);
    enablePeriodicCheckMessage->setText( tr("Show message if a database tree was changed by external app") );
    enablePeriodicCheckMessage->setChecked( AppContext::instance().appConfig()->getEnablePeriodicCheckMessage() );

    periodicCheckLayout=new QVBoxLayout();
    periodicCheckLayout->addWidget( enablePeriodicCheckBase);
    periodicCheckLayout->addLayout( checkBasePeriodLayout);
    periodicCheckLayout->addWidget( enablePeriodicCheckMessage);

    periodicCheckBox=new QGroupBox(this);
    periodicCheckBox->setTitle( tr("Periodic checking database tree") );
    periodicCheckBox->setLayout( periodicCheckLayout );

    // Начальное состояние зависимых параметров (активны-неактивны) выставляются в зависимости от галки
    onEnablePeriodicCheckBase( AppContext::instance().appConfig()->getEnablePeriodicCheckBase() );
}

void AppConfigPage_Synchro::assembly(void)
{
    centralLayout=new QVBoxLayout();

    centralLayout->addWidget( commandText );
    centralLayout->addWidget( synchroCommand );
    centralLayout->addWidget( commandAboutText );
    centralLayout->addWidget( synchroOnBox );
    centralLayout->addWidget( periodicCheckBox );
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout( centralLayout );
}

void AppConfigPage_Synchro::setupSignals(void)
{
    connect( enablePeriodicCheckBase, &QCheckBox::toggled, this, &AppConfigPage_Synchro::onEnablePeriodicCheckBase);
    connect( synchroOnPeriodic,       &QCheckBox::toggled, this, &AppConfigPage_Synchro::onEnablePeriodicSyncro);
}

void AppConfigPage_Synchro::onEnablePeriodicCheckBase(bool state)
{
    checkBasePeriodText->setEnabled(state);
    checkBasePeriod->setEnabled(state);
    checkBasePeriodPostfix->setEnabled(state);

    enablePeriodicCheckMessage->setEnabled(state);
}

void AppConfigPage_Synchro::onEnablePeriodicSyncro(bool state)
{
    synchroPeriodText->setEnabled(state);
    synchroPeriod->setEnabled(state);
    synchroPeriodPostfix->setEnabled(state);
}

void AppConfigPage_Synchro::retranslateUi(void)
{
  synchroOnStartup->setText(tr("Synchronize at MyTetra startup"));
  synchroOnExit->setText(tr("Synchronize when exit from MyTetra"));
  synchroOnPeriodic->setText(tr("Enable periodic background synchronize"));

  commandText->setText(tr("Synchronization command"));
  commandAboutText->setText(tr("Use <b>%a</b> macro for get database directory path"));

  synchroOnBox->setTitle(tr("Automatic start synchronization"));

  synchroPeriodText->setText(tr("Synchronization period: "));
  synchroPeriodPostfix->setText(tr("sec."));

  enablePeriodicCheckBase->setText(tr("Periodic check database tree for change at 3rd-party app"));
  checkBasePeriodText->setText(tr("Checking period: "));
  checkBasePeriodPostfix->setText(tr("sec."));
  enablePeriodicCheckMessage->setText(tr("Show message if a database tree was changed by external app"));

  periodicCheckBox->setTitle(tr("Periodic checking database tree"));
}

// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Synchro::applyChanges(void)
{
    qDebug() << "Apply changes synchro";

    // Сохраняется строка с командой синхронизации
    if(AppContext::instance().appConfig()->get_synchrocommand()!=synchroCommand->text())
        AppContext::instance().appConfig()->set_synchrocommand(synchroCommand->text());

    // Сохраняется настройка запуска синхронизации при старте
    if(AppContext::instance().appConfig()->get_synchroonstartup()!=synchroOnStartup->isChecked())
        AppContext::instance().appConfig()->set_synchroonstartup(synchroOnStartup->isChecked());

    // Сохраняется настройка запуска синхронизации при выходе
    if(AppContext::instance().appConfig()->get_synchroonexit()!=synchroOnExit->isChecked())
        AppContext::instance().appConfig()->set_synchroonexit(synchroOnExit->isChecked());

    // Сохраняется период синхронизации
    // установка периода должна производиться перед обработкой галки включения/выключения
    if(AppContext::instance().appConfig()->getPeriodicSyncroPeriod()!=synchroPeriod->value())
    {
        AppContext::instance().appConfig()->setPeriodicSyncroPeriod( synchroPeriod->value() );

        AppContext::instance().periodicSyncro()->setDelay( synchroPeriod->value() );
    }

    // Сохраняется настройка париодического запуска синхронизации
    if(AppContext::instance().appConfig()->getEnablePeriodicSyncro()!=synchroOnPeriodic->isChecked())
    {
        AppContext::instance().appConfig()->setEnablePeriodicSyncro(synchroOnPeriodic->isChecked());

        if( AppContext::instance().appConfig()->getEnablePeriodicSyncro() )
        {
            showMessageBox(tr("The first background <b>synchronizing</b> starting.<br/>Maybe a slight delay or freezing window..."));
            AppContext::instance().periodicSyncro()->start();
        }
        else
            AppContext::instance().periodicSyncro()->stop();
    }

    // Сохраняется период проверки дерева базы
    // установка периода должна производиться перед обработкой галки включения/выключения
    if(AppContext::instance().appConfig()->getCheckBasePeriod()!=checkBasePeriod->value())
    {
        AppContext::instance().appConfig()->setCheckBasePeriod( checkBasePeriod->value() );

        AppContext::instance().periodicCheckBase()->setDelay( checkBasePeriod->value() );
    }

    // Сохраняется разрешение переодически проверять дерево базы
    if(AppContext::instance().appConfig()->getEnablePeriodicCheckBase()!=enablePeriodicCheckBase->isChecked())
    {
        AppContext::instance().appConfig()->setEnablePeriodicCheckBase(enablePeriodicCheckBase->isChecked());

        if( AppContext::instance().appConfig()->getEnablePeriodicCheckBase() )
        {
            showMessageBox(tr("The first background <b>checking base</b> starting.<br/>Maybe a slight delay or freezing window..."));
            AppContext::instance().periodicCheckBase()->start();
        }
        else
            AppContext::instance().periodicCheckBase()->stop();
    }

    // Сохраняется разрешение выводить сообщение при внешнем изменении базы
    if(AppContext::instance().appConfig()->getEnablePeriodicCheckMessage()!=enablePeriodicCheckMessage->isChecked())
        AppContext::instance().appConfig()->setEnablePeriodicCheckMessage(enablePeriodicCheckMessage->isChecked());

    return 0;
}

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QRadioButton>

#include "AppConfigPage_Synchro.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/PeriodicCheckBase.h"
#include "libraries/PeriodicSyncro.h"
#include "libraries/helpers/MessageHelper.h"


extern AppConfig mytetraConfig;
extern PeriodicCheckBase periodicCheckBase;
extern PeriodicSyncro periodicSyncro;


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
    synchroCommand->setText(mytetraConfig.get_synchrocommand());
    synchroCommand->setCursorPosition(0);

    // Режим кнопки синхронизации: натив или внешняя команда.
    // При нативе поле команды не нужно — гасим, чтобы не вводило
    // в заблуждение (сама команда при этом сохраняется)
    syncToolbarBox=new QGroupBox(this);
    syncToolbarBox->setTitle(tr("Toolbar sync button"));
    syncToolbarNative=new QRadioButton(this);
    syncToolbarNative->setText(tr("Built-in Wi-Fi sync (native)"));
    syncToolbarExternal=new QRadioButton(this);
    syncToolbarExternal->setText(tr("External command (as before)"));

    if(mytetraConfig.get_syncToolbarNative())
        syncToolbarNative->setChecked(true);
    else
        syncToolbarExternal->setChecked(true);

    QVBoxLayout *syncToolbarLayout=new QVBoxLayout();
    syncToolbarLayout->addWidget(syncToolbarNative);
    syncToolbarLayout->addWidget(syncToolbarExternal);

    // В native-режиме команда/период — легаси, а старт/выход и фон
    // работают через встроенный Wi-Fi синк. Иначе страница читается
    // как «два разных синка в одном месте».
    QLabel *syncModeNote=new QLabel(this);
    syncModeNote->setWordWrap(true);
    syncModeNote->setText(tr("Native mode uses the built-in Wi-Fi sync: "
                             "startup/exit/background run it without a command. "
                             "The command below is used only in external mode."));
    syncToolbarLayout->addWidget(syncModeNote);

    syncToolbarBox->setLayout(syncToolbarLayout);

    synchroOnStartup=new QCheckBox(this);
    synchroOnStartup->setText(tr("Synchronize at MyTetra startup"));
    synchroOnStartup->setChecked(mytetraConfig.get_synchroonstartup());

    synchroOnExit=new QCheckBox(this);
    synchroOnExit->setText(tr("Synchronize when exit from MyTetra"));
    synchroOnExit->setChecked(mytetraConfig.get_synchroonexit());

    synchroOnPeriodic=new QCheckBox(this);
    synchroOnPeriodic->setText(tr("Enable periodic background synchronize"));
    synchroOnPeriodic->setChecked(mytetraConfig.getEnablePeriodicSyncro());

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
    synchroPeriod->setValue( mytetraConfig.getPeriodicSyncroPeriod() );

    synchroPeriodPostfix=new QLabel(this);
    synchroPeriodPostfix->setText( tr("sec.") );

    synchroPeriodLayout=new QHBoxLayout();
    synchroPeriodLayout->addWidget( synchroPeriodText );
    synchroPeriodLayout->addWidget( synchroPeriod );
    synchroPeriodLayout->addWidget( synchroPeriodPostfix );
    synchroPeriodLayout->addStretch();
    synchroOnLayout->addLayout( synchroPeriodLayout );

    syncBgPush=new QCheckBox(this);
    syncBgPush->setText(tr("Background sync: upload mine"));
    syncBgPush->setChecked(mytetraConfig.get_syncBgPush());
    syncBgPull=new QCheckBox(this);
    syncBgPull->setText(tr("Background sync: download theirs"));
    syncBgPull->setChecked(mytetraConfig.get_syncBgPull());
    syncBgNote=new QLabel(this);
    syncBgNote->setText(tr("Native background sync: safe additions only, at most once per 15 min"));
    syncBgNote->setWordWrap(true);
    synchroOnLayout->addWidget( syncBgPush );
    synchroOnLayout->addWidget( syncBgPull );
    synchroOnLayout->addWidget( syncBgNote );

    synchroOnBox->setLayout( synchroOnLayout );

    // Начальное состояние зависимых параметров (активны-неактивны) для периодической синхронизации
    onEnablePeriodicSyncro( mytetraConfig.getEnablePeriodicSyncro() );


    // Виджеты настройки периодической проверки базы на предмет изменений
    enablePeriodicCheckBase=new QCheckBox(this);
    enablePeriodicCheckBase->setText( tr("Periodic check database tree for change at 3rd-party app") ); // Периодически проверять дерево базы на предмет изменения сторонней программой
    enablePeriodicCheckBase->setChecked( mytetraConfig.getEnablePeriodicCheckBase() );

    checkBasePeriodText=new QLabel(this);
    checkBasePeriodText->setText( tr("Checking period: ") );

    checkBasePeriod=new QSpinBox(this);
    checkBasePeriod->setMaximum( 100000 ); // Максимальная граница должна устанавливаться перед заданием значения
    checkBasePeriod->setValue( mytetraConfig.getCheckBasePeriod() );

    checkBasePeriodPostfix=new QLabel(this);
    checkBasePeriodPostfix->setText( tr("sec.") );

    checkBasePeriodLayout=new QHBoxLayout();
    checkBasePeriodLayout->addWidget( checkBasePeriodText );
    checkBasePeriodLayout->addWidget( checkBasePeriod );
    checkBasePeriodLayout->addWidget( checkBasePeriodPostfix );
    checkBasePeriodLayout->addStretch();

    enablePeriodicCheckMessage=new QCheckBox(this);
    enablePeriodicCheckMessage->setText( tr("Show message if a database tree was changed by external app") );
    enablePeriodicCheckMessage->setChecked( mytetraConfig.getEnablePeriodicCheckMessage() );

    periodicCheckLayout=new QVBoxLayout();
    periodicCheckLayout->addWidget( enablePeriodicCheckBase);
    periodicCheckLayout->addLayout( checkBasePeriodLayout);
    periodicCheckLayout->addWidget( enablePeriodicCheckMessage);

    periodicCheckBox=new QGroupBox(this);
    periodicCheckBox->setTitle( tr("Periodic checking database tree") );
    periodicCheckBox->setLayout( periodicCheckLayout );

    // Начальное состояние зависимых параметров (активны-неактивны) выставляются в зависимости от галки
    onEnablePeriodicCheckBase( mytetraConfig.getEnablePeriodicCheckBase() );
}


void AppConfigPage_Synchro::assembly(void)
{
    centralLayout=new QVBoxLayout();

    centralLayout->addWidget( syncToolbarBox );
    centralLayout->addWidget( commandText );
    centralLayout->addWidget( synchroCommand );
    centralLayout->addWidget( commandAboutText );
    centralLayout->addWidget( synchroOnBox );
    centralLayout->addWidget( periodicCheckBox );
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout( centralLayout );

    // Поле команды активно только во внешнем режиме
    onSyncToolbarModeChanged();
}


void AppConfigPage_Synchro::setupSignals(void)
{
    connect( enablePeriodicCheckBase, &QCheckBox::toggled, this, &AppConfigPage_Synchro::onEnablePeriodicCheckBase);
    connect( synchroOnPeriodic,       &QCheckBox::toggled, this, &AppConfigPage_Synchro::onEnablePeriodicSyncro);
    connect( syncToolbarNative,   &QRadioButton::toggled, this, &AppConfigPage_Synchro::onSyncToolbarModeChanged);
    connect( syncToolbarExternal, &QRadioButton::toggled, this, &AppConfigPage_Synchro::onSyncToolbarModeChanged);
}


void AppConfigPage_Synchro::onSyncToolbarModeChanged(void)
{
    const bool external=syncToolbarExternal->isChecked();

    commandText->setEnabled(external);
    synchroCommand->setEnabled(external);
    commandAboutText->setEnabled(external);
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

    // Направления фона — тоже только при включённой периодике
    syncBgPush->setEnabled(state);
    syncBgPull->setEnabled(state);
    syncBgNote->setEnabled(state);
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Synchro::applyChanges(void)
{
    qDebug() << "Apply changes synchro";

    // Сохраняется режим кнопки синхронизации: натив или внешняя команда
    if(mytetraConfig.get_syncToolbarNative()!=syncToolbarNative->isChecked())
        mytetraConfig.set_syncToolbarNative(syncToolbarNative->isChecked());

    // Сохраняется строка с командой синхронизации
    if(mytetraConfig.get_synchrocommand()!=synchroCommand->text())
        mytetraConfig.set_synchrocommand(synchroCommand->text());

    // Сохраняется настройка запуска синхронизации при старте
    if(mytetraConfig.get_synchroonstartup()!=synchroOnStartup->isChecked())
        mytetraConfig.set_synchroonstartup(synchroOnStartup->isChecked());

    // Сохраняется настройка запуска синхронизации при выходе
    if(mytetraConfig.get_synchroonexit()!=synchroOnExit->isChecked())
        mytetraConfig.set_synchroonexit(synchroOnExit->isChecked());


    // Сохраняется период синхронизации
    // установка периода должна производиться перед обработкой галки включения/выключения
    if(mytetraConfig.getPeriodicSyncroPeriod()!=synchroPeriod->value())
    {
        mytetraConfig.setPeriodicSyncroPeriod( synchroPeriod->value() );

        periodicSyncro.setDelay( synchroPeriod->value() );
    }


    // Сохраняется настройка париодического запуска синхронизации
    if(mytetraConfig.getEnablePeriodicSyncro()!=synchroOnPeriodic->isChecked())
    {
        mytetraConfig.setEnablePeriodicSyncro(synchroOnPeriodic->isChecked());

        if( mytetraConfig.getEnablePeriodicSyncro() )
        {
            showMessageBox(tr("The first background <b>synchronizing</b> starting.<br/>Maybe a slight delay or freezing window..."));
            periodicSyncro.start();
        }
        else
            periodicSyncro.stop();
    }

    // Сохраняются направления фонового нативного синка
    if(mytetraConfig.get_syncBgPush()!=syncBgPush->isChecked())
        mytetraConfig.set_syncBgPush(syncBgPush->isChecked());

    if(mytetraConfig.get_syncBgPull()!=syncBgPull->isChecked())
        mytetraConfig.set_syncBgPull(syncBgPull->isChecked());


    // Сохраняется период проверки дерева базы
    // установка периода должна производиться перед обработкой галки включения/выключения
    if(mytetraConfig.getCheckBasePeriod()!=checkBasePeriod->value())
    {
        mytetraConfig.setCheckBasePeriod( checkBasePeriod->value() );

        periodicCheckBase.setDelay( checkBasePeriod->value() );
    }


    // Сохраняется разрешение переодически проверять дерево базы
    if(mytetraConfig.getEnablePeriodicCheckBase()!=enablePeriodicCheckBase->isChecked())
    {
        mytetraConfig.setEnablePeriodicCheckBase(enablePeriodicCheckBase->isChecked());

        if( mytetraConfig.getEnablePeriodicCheckBase() )
        {
            showMessageBox(tr("The first background <b>checking base</b> starting.<br/>Maybe a slight delay or freezing window..."));
            periodicCheckBase.start();
        }
        else
            periodicCheckBase.stop();
    }

    // Сохраняется разрешение выводить сообщение при внешнем изменении базы
    if(mytetraConfig.getEnablePeriodicCheckMessage()!=enablePeriodicCheckMessage->isChecked())
        mytetraConfig.setEnablePeriodicCheckMessage(enablePeriodicCheckMessage->isChecked());

    return 0;
}

#include <QWidget>
#include <QDebug>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>

#include "AppConfigPage_Appearance.h"

#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigUi.h"

AppConfigPage_Appearance::AppConfigPage_Appearance(EditorShowTextDispatcher *dispatcher, QWidget *parent) : ConfigPage(parent),
    m_dispatcher(dispatcher)
{
    setupUi();
    setupSignals();
    assembly();
}

AppConfigPage_Appearance::~AppConfigPage_Appearance()
{

}

void AppConfigPage_Appearance::setupUi()
{
    qDebug() << "Create appearance config page";

    // Настройка запуска MyTetra в свернутом окне
    runInMinimizedWindow=new QCheckBox(this);
    runInMinimizedWindow->setText(tr("Run MyTetra in a minimized window"));
    runInMinimizedWindow->setChecked( AppContext::instance().appConfig()->uiSection()->get_runinminimizedwindow() );

    // Настройка поведения открепляемых окон, как они будут сворачиваться и разворачитьваться
    dockableWindowsBehavior=new QCheckBox(this);
    dockableWindowsBehavior->setText(tr("Hide detached windows if close main window"));
    dockableWindowsBehavior->setChecked( AppContext::instance().appConfig()->uiSection()->getDockableWindowsBehavior()=="together" );
}

void AppConfigPage_Appearance::setupSignals()
{

}

void AppConfigPage_Appearance::assembly()
{
    // Группировщик виджетов настройки поведения окна
    behaviorBox=new QGroupBox(this);
    behaviorBox->setTitle(tr("Windows behavior"));

    QVBoxLayout *behaviorLayout = new QVBoxLayout;
    behaviorLayout->addWidget(runInMinimizedWindow);
    behaviorLayout->addWidget(dockableWindowsBehavior);
    behaviorBox->setLayout(behaviorLayout);

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(behaviorBox);
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout(centralLayout);
}

// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
void AppConfigPage_Appearance::retranslateUi(void)
{
  runInMinimizedWindow->setText(tr("Run MyTetra in a minimized window"));
  dockableWindowsBehavior->setText(tr("Hide detached windows if close main window"));

  behaviorBox->setTitle(tr("Windows behavior"));
}

int AppConfigPage_Appearance::applyChanges()
{
    qDebug() << "Apply changes appearance";

    int result=0;

    // Сохраняется настройка режима запуска MyTetra - обычный или свернутый
    if(AppContext::instance().appConfig()->uiSection()->get_runinminimizedwindow()!=runInMinimizedWindow->isChecked())
    {
        AppContext::instance().appConfig()->uiSection()->set_runinminimizedwindow(runInMinimizedWindow->isChecked());
    }

    // Сохраняется настройка поведения открепляемых окон
    QString mode=dockableWindowsBehavior->isChecked() ? "together" : "single";
    if(AppContext::instance().appConfig()->uiSection()->getDockableWindowsBehavior()!=mode)
    {
        AppContext::instance().appConfig()->uiSection()->setDockableWindowsBehavior(mode);

        m_dispatcher->switchBehavior(mode);
    }

    return result;
}

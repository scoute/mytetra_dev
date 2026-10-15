#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>

#include "AppConfigPage_History.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"

extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


AppConfigPage_History::AppConfigPage_History(QWidget *parent) : ConfigPage(parent)
{
    setupUi();
    setupSignals();
    assembly();
}


AppConfigPage_History::~AppConfigPage_History()
{

}


void AppConfigPage_History::setupUi()
{
    qDebug() << "Create history config page";

    // Настройки курсора при навигации по истории
    rememberAtHistoryNavigationCheckBox=new QCheckBox(this);
    rememberAtHistoryNavigationCheckBox->setText(tr("Remember cursor position at history navigation"));
    rememberAtHistoryNavigationCheckBox->setChecked(mytetraConfig.getRememberCursorAtHistoryNavigation());

    rememberAtOrdinarySelectionCheckBox=new QCheckBox(this);
    rememberAtOrdinarySelectionCheckBox->setText(tr("Try remember cursor position at ordinary selection"));
    rememberAtOrdinarySelectionCheckBox->setChecked(mytetraConfig.getRememberCursorAtOrdinarySelection());

    // Сквош истории изменений заметки: автосжатие снизу по лимиту.
    // По умолчанию выключено
    squashEnable=new QCheckBox(this);
    squashEnable->setText(tr("Squash old note versions automatically"));
    squashEnable->setChecked(mytetraConfig.get_historySquashEnable());

    squashKeepLabel=new QLabel(tr("Keep versions:"), this);

    // Выпадашка фиксированных лимитов: произвольные числа
    // не нужны, лимит это грубый порог
    squashKeep=new QComboBox(this);
    squashKeep->addItem(QStringLiteral("10"), 10);
    squashKeep->addItem(QStringLiteral("20"), 20);
    squashKeep->addItem(QStringLiteral("50"), 50);
    squashKeep->addItem(QStringLiteral("100"), 100);
    squashKeep->addItem(QStringLiteral("200"), 200);
    squashKeep->addItem(QStringLiteral("500"), 500);
    squashKeep->addItem(QStringLiteral("1000"), 1000);

    const int keepIndex=squashKeep->findData(mytetraConfig.get_historySquashKeep());
    squashKeep->setCurrentIndex(keepIndex>=0 ? keepIndex : squashKeep->findData(100));
    squashKeep->setEnabled(mytetraConfig.get_historySquashEnable());
    squashKeepLabel->setEnabled(mytetraConfig.get_historySquashEnable());
}


void AppConfigPage_History::setupSignals()
{
    connect(squashEnable, &QCheckBox::toggled, squashKeep, &QComboBox::setEnabled);
    connect(squashEnable, &QCheckBox::toggled, squashKeepLabel, &QLabel::setEnabled);
}


void AppConfigPage_History::assembly()
{
    // Группировщик виджетов для настроек курсора при навигации по истории
    historyBox=new QGroupBox(this);
    historyBox->setTitle(tr("History of visited notes"));

    // Виджеты вставляются в группировщик настроек курсора при навигации по истории
    QVBoxLayout *historyLayout = new QVBoxLayout;
    historyLayout->addWidget(rememberAtHistoryNavigationCheckBox);
    historyLayout->addWidget(rememberAtOrdinarySelectionCheckBox);
    historyBox->setLayout(historyLayout);

    // Раздел истории изменений заметки: сквош снизу по лимиту
    noteHistoryBox=new QGroupBox(this);
    noteHistoryBox->setTitle(tr("Note change history"));

    QHBoxLayout *squashLayout=new QHBoxLayout;
    squashLayout->addWidget(squashEnable);
    squashLayout->addWidget(squashKeepLabel);
    squashLayout->addWidget(squashKeep);
    squashLayout->addStretch(1);
    noteHistoryBox->setLayout(squashLayout);

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(historyBox);
    centralLayout->addWidget(noteHistoryBox);
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout(centralLayout);
}


void AppConfigPage_History::retranslateUi(void)
{
  rememberAtHistoryNavigationCheckBox->setText(tr("Remember cursor position at history navigation"));
  rememberAtOrdinarySelectionCheckBox->setText(tr("Try remember cursor position at ordinary selection"));
  historyBox->setTitle(tr("History of visited notes"));
  noteHistoryBox->setTitle(tr("Note change history"));
  squashEnable->setText(tr("Squash old note versions automatically"));
  squashKeepLabel->setText(tr("Keep versions:"));
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_History::applyChanges()
{
    qDebug() << "Apply changes history";

    int result=0;

    // Сохраняется настройка нужно ли вспоминать позицию курсора при перемещении
    // по истории
    if(mytetraConfig.getRememberCursorAtHistoryNavigation()!=rememberAtHistoryNavigationCheckBox->isChecked())
      mytetraConfig.setRememberCursorAtHistoryNavigation(rememberAtHistoryNavigationCheckBox->isChecked());

    // Сохраняется настройка нужно ли пытаться вспоминать позицию курсора при
    // обычном выборе записи
    if(mytetraConfig.getRememberCursorAtOrdinarySelection()!=rememberAtOrdinarySelectionCheckBox->isChecked())
      mytetraConfig.setRememberCursorAtOrdinarySelection(rememberAtOrdinarySelectionCheckBox->isChecked());

    // Сквош истории изменений: авточка и лимит. Лимит подхватится
    // сам при следующем снапшоте
    if(mytetraConfig.get_historySquashEnable()!=squashEnable->isChecked())
      mytetraConfig.set_historySquashEnable(squashEnable->isChecked());

    if(mytetraConfig.get_historySquashKeep()!=squashKeep->currentData().toInt())
      mytetraConfig.set_historySquashKeep(squashKeep->currentData().toInt());

    return result;
}

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QToolButton>
#include <QCommonStyle>
#include <QWhatsThis>

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

    // Знак вопроса у группы истории
    historyHelpButton=new QToolButton(this);
    QCommonStyle styleHelp;
    historyHelpButton->setIcon(styleHelp.standardIcon(QStyle::SP_MessageBoxQuestion));
    historyHelpButton->setToolTip(tr("What is the notes history"));
    historyHelpButton->setWhatsThis(tr("History of viewed notes: go back and forward with Ctrl+Alt+Left and Ctrl+Alt+Right.\nThese options restore the text cursor position."));
}


void AppConfigPage_History::setupSignals()
{
    connect(historyHelpButton, &QToolButton::clicked, this, &AppConfigPage_History::onHistoryHelpButton);
}


void AppConfigPage_History::assembly()
{
    // Группировщик виджетов для настроек курсора при навигации по истории.
    // Штатный заголовок пустой: название рисуется своей строкой чтобы
    // рядом встал знак вопроса
    historyBox=new QGroupBox(this);
    historyBox->setTitle(QString());

    QLabel *historyTitleLabel=new QLabel(tr("History of visited notes"), this);
    QFont historyTitleFont=historyTitleLabel->font();
    historyTitleFont.setBold(true);
    historyTitleLabel->setFont(historyTitleFont);

    QHBoxLayout *historyTitleLayout=new QHBoxLayout;
    historyTitleLayout->addWidget(historyTitleLabel);
    historyTitleLayout->addWidget(historyHelpButton);
    historyTitleLayout->addStretch();

    // Виджеты вставляются в группировщик настроек курсора при навигации по истории
    QVBoxLayout *historyLayout = new QVBoxLayout;
    historyLayout->addLayout(historyTitleLayout);
    historyLayout->addWidget(rememberAtHistoryNavigationCheckBox);
    historyLayout->addWidget(rememberAtOrdinarySelectionCheckBox);
    historyBox->setLayout(historyLayout);

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(historyBox);
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout(centralLayout);
}


// Клик по знаку вопроса мгновенно показывает подсказку
// под кнопкой: ждать секунду наведения не нужно
void AppConfigPage_History::onHistoryHelpButton(void)
{
    QPoint showPoint=historyHelpButton->mapToGlobal(QPoint(0, historyHelpButton->height()));

    QWhatsThis::showText(showPoint, historyHelpButton->whatsThis(), historyHelpButton);
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

    return result;
}

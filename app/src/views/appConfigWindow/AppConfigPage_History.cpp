#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>

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

    // Мастер-тумблер истории заметок: без него ни снапшотов,
    // ни диалога. По умолчанию история ведется
    historyEnable=new QCheckBox(this);
    historyEnable->setText(tr("Enable note history tracking"));
    historyEnable->setChecked(mytetraConfig.get_historyEnable());

    // Сквош истории изменений заметки: автосжатие снизу по лимиту
    squashEnable=new QCheckBox(this);
    squashEnable->setText(tr("Squash note history automatically"));
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

    // Выпадашка всегда активна при включенной истории: лимит нужен
    // и ручной кнопке "Сжать историю". Без мастер-тумблера вся
    // группа серая
    const bool squashActive=mytetraConfig.get_historyEnable();
    squashEnable->setEnabled(squashActive);
    squashKeep->setEnabled(squashActive);
    squashKeepLabel->setEnabled(squashActive);
}


void AppConfigPage_History::setupSignals()
{
    // Без мастер-тумблера сквош-настройки не при делах
    connect(historyEnable, &QCheckBox::toggled, squashEnable, &QCheckBox::setEnabled);
    connect(historyEnable, &QCheckBox::toggled, squashKeep, &QComboBox::setEnabled);
    connect(historyEnable, &QCheckBox::toggled, squashKeepLabel, &QLabel::setEnabled);
    connect(historyEnable, &QCheckBox::toggled, squashModeLabel, &QLabel::setEnabled);
    connect(historyEnable, &QCheckBox::toggled, squashDeleteOldest, &QRadioButton::setEnabled);
    connect(historyEnable, &QCheckBox::toggled, squashSmartMerge, &QRadioButton::setEnabled);
    connect(historyEnable, &QCheckBox::toggled, squashBundle10, &QRadioButton::setEnabled);
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

    QVBoxLayout *noteHistoryLayout=new QVBoxLayout;
    noteHistoryLayout->addWidget(historyEnable);

    QHBoxLayout *squashLayout=new QHBoxLayout;
    squashLayout->addWidget(squashEnable);
    squashLayout->addWidget(squashKeepLabel);
    squashLayout->addWidget(squashKeep);
    squashLayout->addStretch(1);
    noteHistoryLayout->addLayout(squashLayout);

    // Стратегия превышения лимита: радиокнопки с id под SquashMode
    squashModeLabel=new QLabel(tr("What to do on limit excess:"), this);
    noteHistoryLayout->addWidget(squashModeLabel);

    squashModeGroup=new QButtonGroup(this);
    squashDeleteOldest=new QRadioButton(tr("Delete oldest entries"), this);
    squashSmartMerge=new QRadioButton(tr("Merge similar neighbors"), this);
    squashBundle10=new QRadioButton(tr("Compress every 10 entries into one except the latest 10"), this);
    squashModeGroup->addButton(squashDeleteOldest, 0);
    squashModeGroup->addButton(squashSmartMerge, 1);
    squashModeGroup->addButton(squashBundle10, 2);
    noteHistoryLayout->addWidget(squashDeleteOldest);
    noteHistoryLayout->addWidget(squashSmartMerge);
    noteHistoryLayout->addWidget(squashBundle10);

    const int modeIndex=qBound(0, mytetraConfig.get_historySquashMode(), 2);
    squashModeGroup->button(modeIndex)->setChecked(true);

    const bool squashUiActive=mytetraConfig.get_historyEnable();
    squashModeLabel->setEnabled(squashUiActive);
    squashDeleteOldest->setEnabled(squashUiActive);
    squashSmartMerge->setEnabled(squashUiActive);
    squashBundle10->setEnabled(squashUiActive);
    noteHistoryBox->setLayout(noteHistoryLayout);

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
  historyEnable->setText(tr("Enable note history tracking"));
  squashEnable->setText(tr("Squash note history automatically"));
  squashKeepLabel->setText(tr("Keep versions:"));
  squashModeLabel->setText(tr("What to do on limit excess:"));
  squashDeleteOldest->setText(tr("Delete oldest entries"));
  squashSmartMerge->setText(tr("Merge similar neighbors"));
  squashBundle10->setText(tr("Compress every 10 entries into one except the latest 10"));
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

    // Мастер-тумблер истории: выключенная история не ведется,
    // сквош-настройки при ней не при делах (серые в UI)
    if(mytetraConfig.get_historyEnable()!=historyEnable->isChecked())
      mytetraConfig.set_historyEnable(historyEnable->isChecked());

    // Сквош истории изменений: авточка и лимит. Лимит подхватится
    // сам при следующем снапшоте
    if(mytetraConfig.get_historySquashEnable()!=squashEnable->isChecked())
      mytetraConfig.set_historySquashEnable(squashEnable->isChecked());

    if(mytetraConfig.get_historySquashKeep()!=squashKeep->currentData().toInt())
      mytetraConfig.set_historySquashKeep(squashKeep->currentData().toInt());

    if(mytetraConfig.get_historySquashMode()!=squashModeGroup->checkedId())
      mytetraConfig.set_historySquashMode(qBound(0, squashModeGroup->checkedId(), 2));

    return result;
}

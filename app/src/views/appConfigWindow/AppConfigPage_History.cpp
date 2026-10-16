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
#include <QMessageBox>
#include <QSignalBlocker>

#include "AppConfigPage_History.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/GitHelper.h"

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
    historyEnable->setText(tr("Enable note history (git must be installed)"));
    historyEnable->setChecked(mytetraConfig.get_historyEnable());

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

    // Все сквош-виджеты серые без мастер-тумблера.
    // Отдельной галочки автосжатия нет: раз лимит выставлен,
    // ротация идет всегда, авто после снапшота
    const bool squashActive=mytetraConfig.get_historyEnable();
    squashKeep->setEnabled(squashActive);
    squashKeepLabel->setEnabled(squashActive);

    // Стратегия превышения лимита: радиокнопки с id под SquashMode.
    // Создание здесь, а не в assembly: setupSignals коннектит их
    // сразу после setupUi
    squashModeLabel=new QLabel(tr("What to do on limit excess:"), this);
    squashModeGroup=new QButtonGroup(this);
    squashDeleteOldest=new QRadioButton(tr("Delete oldest entries"), this);
    squashSmartMerge=new QRadioButton(tr("Merge similar neighbors"), this);
    squashBundle10=new QRadioButton(tr("Compress every 10 entries into one except the latest 10"), this);
    squashModeGroup->addButton(squashDeleteOldest, 0);
    squashModeGroup->addButton(squashSmartMerge, 1);
    squashModeGroup->addButton(squashBundle10, 2);

    const int modeIndex=qBound(0, mytetraConfig.get_historySquashMode(), 2);
    squashModeGroup->button(modeIndex)->setChecked(true);

    squashModeLabel->setEnabled(squashActive);
    squashDeleteOldest->setEnabled(squashActive);
    squashSmartMerge->setEnabled(squashActive);
    squashBundle10->setEnabled(squashActive);

    // Итоговая серость одним слотом (см. выше)
    updateSquashEnabled();
}


void AppConfigPage_History::setupSignals()
{
    // Включение истории только при наличии git: иначе предупреждение
    // и возврат галочки. Проверка здесь, а не в apply: отказ виден сразу
    connect(historyEnable, &QCheckBox::toggled, this, &AppConfigPage_History::onHistoryEnableToggled);

    // Серость одним слотом: мастер-тумблер гасит всю группу
    connect(historyEnable, &QCheckBox::toggled, this, &AppConfigPage_History::updateSquashEnabled);
}


// Единое состояние серости: без мастер-тумблера серая вся группа
void AppConfigPage_History::updateSquashEnabled(void)
{
    const bool active=historyEnable->isChecked();

    squashKeep->setEnabled(active);
    squashKeepLabel->setEnabled(active);

    squashModeLabel->setEnabled(active);
    squashDeleteOldest->setEnabled(active);
    squashSmartMerge->setEnabled(active);
    squashBundle10->setEnabled(active);
}


void AppConfigPage_History::onHistoryEnableToggled(bool checked)
{
    if(!checked)
        return;

    if(GitHelper::isAvailable())
        return;

    QMessageBox::warning(this, tr("Note change history"),
                         tr("Cannot enable note history:\n"
                            "git is not installed.\n"
                            "Linux: install the git package. Windows: https://git-scm.com."));

    // Возврат галочки без рекурсии сигнала
    const QSignalBlocker blocker(historyEnable);
    historyEnable->setChecked(false);
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

    // Лимит отдельной строкой между включением и сжатием
    QHBoxLayout *keepLayout=new QHBoxLayout;
    keepLayout->addWidget(squashKeepLabel);
    keepLayout->addWidget(squashKeep);
    keepLayout->addStretch(1);
    noteHistoryLayout->addLayout(keepLayout);

    // Стратегия превышения лимита (виджеты созданы в setupUi)
    noteHistoryLayout->addWidget(squashModeLabel);
    noteHistoryLayout->addWidget(squashDeleteOldest);
    noteHistoryLayout->addWidget(squashSmartMerge);
    noteHistoryLayout->addWidget(squashBundle10);
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
  historyEnable->setText(tr("Enable note history (git must be installed)"));
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

    // Сквош-лимит и стратегия. Подхватятся сами при следующем
    // снапшоте
    if(mytetraConfig.get_historySquashKeep()!=squashKeep->currentData().toInt())
      mytetraConfig.set_historySquashKeep(squashKeep->currentData().toInt());

    if(mytetraConfig.get_historySquashMode()!=squashModeGroup->checkedId())
      mytetraConfig.set_historySquashMode(qBound(0, squashModeGroup->checkedId(), 2));

    return result;
}

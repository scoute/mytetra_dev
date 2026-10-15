#ifndef APPCONFIGPAGE_HISTORY_H
#define APPCONFIGPAGE_HISTORY_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class QLabel;
class QComboBox;
class QRadioButton;
class QButtonGroup;

class AppConfigPage_History : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_History(QWidget *parent = nullptr);
    virtual ~AppConfigPage_History();

    int applyChanges(void);
  void retranslateUi(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка
    QGroupBox *historyBox;

    QCheckBox *rememberAtHistoryNavigationCheckBox;
    QCheckBox *rememberAtOrdinarySelectionCheckBox;

    // Раздел истории изменений заметки
    QGroupBox *noteHistoryBox;

    QCheckBox *historyEnable;  // Мастер-тумблер ведения истории

    QCheckBox *squashEnable;   // Автосжатие старых версий по лимиту
    QLabel *squashKeepLabel;   // Подпись выпадашки лимита
    QComboBox *squashKeep;     // Сколько версий хранить (genesis плюс свежие)

    QLabel *squashModeLabel;       // Подпись стратегий превышения лимита
    QButtonGroup *squashModeGroup; // Группа стратегий, id совпадают с SquashMode
    QRadioButton *squashDeleteOldest; // Удалить старые
    QRadioButton *squashSmartMerge;   // Склеить похожие соседние
    QRadioButton *squashBundle10;     // Паковать каждую 10-ю исключая свежие 10
};

#endif // APPCONFIGPAGE_HISTORY_H

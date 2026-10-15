#ifndef APPCONFIGPAGE_HISTORY_H
#define APPCONFIGPAGE_HISTORY_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class QLabel;
class QComboBox;

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

    QCheckBox *squashEnable;   // Автосжатие старых версий по лимиту
    QLabel *squashKeepLabel;   // Подпись выпадашки лимита
    QComboBox *squashKeep;     // Сколько версий хранить (genesis плюс свежие)
};

#endif // APPCONFIGPAGE_HISTORY_H

#ifndef APPCONFIGPAGE_HISTORY_H
#define APPCONFIGPAGE_HISTORY_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class QToolButton;

class AppConfigPage_History : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_History(QWidget *parent = nullptr);
    virtual ~AppConfigPage_History();

    int applyChanges(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка
    QGroupBox *historyBox;

    // Знак вопроса у группы истории
    QToolButton *historyHelpButton;

    QCheckBox *rememberAtHistoryNavigationCheckBox;
    QCheckBox *rememberAtOrdinarySelectionCheckBox;

  private slots:

    // Знак вопроса у группы истории: мгновенная подсказка
    void onHistoryHelpButton(void);
};

#endif // APPCONFIGPAGE_HISTORY_H

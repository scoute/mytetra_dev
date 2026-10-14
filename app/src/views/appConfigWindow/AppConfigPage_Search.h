#ifndef APPCONFIGPAGE_SEARCH_H
#define APPCONFIGPAGE_SEARCH_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QRadioButton;

class AppConfigPage_Search : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Search(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Search();

    int applyChanges(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка
    QGroupBox *findInNoteBox;

    QRadioButton *findBarRadio; // Поиск полоской внутри заметки
    QRadioButton *findWindowRadio; // Поиск отдельным окном
};

#endif // APPCONFIGPAGE_SEARCH_H

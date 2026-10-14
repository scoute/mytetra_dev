#ifndef APPCONFIGPAGE_APPEARANCE_H
#define APPCONFIGPAGE_APPEARANCE_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;

class AppConfigPage_Appearance : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Appearance(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Appearance();

    int applyChanges(void);
    void retranslateUi(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка поведения окон. Дизайн (тема, иконки,
    // цвета, шрифты) живёт на странице Themes
    QGroupBox *behaviorBox;

    QCheckBox *runInMinimizedWindow; // Разрешен ли запуск в свернутом окне
    QCheckBox *dockableWindowsBehavior; // Поведение открепляемых окон
};

#endif // APPCONFIGPAGE_APPEARANCE_H

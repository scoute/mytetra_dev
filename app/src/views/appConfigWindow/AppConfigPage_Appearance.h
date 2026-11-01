#ifndef APPCONFIGPAGE_APPEARANCE_H
#define APPCONFIGPAGE_APPEARANCE_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class EditorShowTextDispatcher;

class AppConfigPage_Appearance : public ConfigPage
{
    Q_OBJECT

public:
    // P3: диспетчер внедряется диалогом (был EditorShowTextDispatcher::instance())
    explicit AppConfigPage_Appearance(EditorShowTextDispatcher *dispatcher, QWidget *parent = nullptr);
    virtual ~AppConfigPage_Appearance();

    int applyChanges(void);
    void retranslateUi(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка поведения окон. Дизайн (тема, иконки,
    // цвета, шрифты) живёт на странице Themes
    // P3: внедренный диспетчер (DI вместо синглтона)
    EditorShowTextDispatcher *m_dispatcher=nullptr;

    QGroupBox *behaviorBox;

    QCheckBox *runInMinimizedWindow; // Разрешен ли запуск в свернутом окне
    QCheckBox *dockableWindowsBehavior; // Поведение открепляемых окон
};

#endif // APPCONFIGPAGE_APPEARANCE_H

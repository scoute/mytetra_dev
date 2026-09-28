#ifndef APPCONFIGPAGE_APPEARANCE_H
#define APPCONFIGPAGE_APPEARANCE_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class QLabel;
class MtComboBox;

class AppConfigPage_Appearance : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Appearance(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Appearance();

    int applyChanges(void);
    void cancelChanges(void) override;

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    void setupThemeComboBox(void);
    void setupIconSizeComboBox(void);

    // Применить тему из выпадашки. Вызывается живьем и из applyChanges
    void applyThemeSelection(void);

    // Объединяющая рамка
    QGroupBox *behaviorBox;
    QGroupBox *interfaceBox;

    QLabel *themeLabel;
    MtComboBox *themeNameComboBox;

    QLabel *iconSizeLabel;
    MtComboBox *iconSizeComboBox;

    QCheckBox *runInMinimizedWindow; // Разрешен ли запуск в свернутом окне
    QCheckBox *dockableWindowsBehavior; // Поведение открепляемых окон

    // Тема, бывшая активной при открытии диалога. Нужна для отката
    // живого предпросмотра если диалог закроют через Cancel
    QString initialTheme;

  private slots:

    // Тема применяется сразу при выборе в выпадашке: предпросмотр живьем
    void onThemeChanged(int index);
};

#endif // APPCONFIGPAGE_APPEARANCE_H

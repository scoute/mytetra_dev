#ifndef APPCONFIGPAGE_FAVORITES_H
#define APPCONFIGPAGE_FAVORITES_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;
class QRadioButton;
class QLabel;

class AppConfigPage_Favorites : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Favorites(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Favorites();

    void retranslateUi(void);
    int applyChanges(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка
    QGroupBox *favoritesBox;

    // Показывать панель избранного, включена по умолчанию
    QCheckBox *enableFavoritesPanel;

    // Где хранить звездочки: сайдкар или поля XML
    QGroupBox *storageBox;
    QRadioButton *sidecarRadio;
    QRadioButton *xmlRadio;
    QLabel *storageHint;
};

#endif // APPCONFIGPAGE_FAVORITES_H

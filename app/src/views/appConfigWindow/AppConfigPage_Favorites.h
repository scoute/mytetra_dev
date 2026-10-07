#ifndef APPCONFIGPAGE_FAVORITES_H
#define APPCONFIGPAGE_FAVORITES_H

#include "ConfigPage.h"

class QWidget;
class QGroupBox;
class QCheckBox;

class AppConfigPage_Favorites : public ConfigPage
{
    Q_OBJECT

public:
    AppConfigPage_Favorites(QWidget *parent = nullptr);
    virtual ~AppConfigPage_Favorites();

    int applyChanges(void);

  protected:

    void setupUi(void);
    void setupSignals(void);
    void assembly(void);

    // Объединяющая рамка
    QGroupBox *favoritesBox;

    // Показывать панель избранного, включена по умолчанию
    QCheckBox *enableFavoritesPanel;
};

#endif // APPCONFIGPAGE_FAVORITES_H

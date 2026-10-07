#include <QDebug>
#include <QVBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>

#include "AppConfigPage_Favorites.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "views/favoritesPanel/FavoritesPanel.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern QObject *pMainWindow;


AppConfigPage_Favorites::AppConfigPage_Favorites(QWidget *parent) : ConfigPage(parent)
{
    setupUi();
    setupSignals();
    assembly();
}


AppConfigPage_Favorites::~AppConfigPage_Favorites()
{

}


void AppConfigPage_Favorites::setupUi()
{
    qDebug() << "Create favorites config page";

    // Панель избранного между тулбаром веток и деревом
    enableFavoritesPanel=new QCheckBox(this);
    enableFavoritesPanel->setText(tr("Show favorites panel"));
    enableFavoritesPanel->setChecked(mytetraConfig.get_favoritesEnabled());
}


void AppConfigPage_Favorites::setupSignals()
{

}


void AppConfigPage_Favorites::assembly()
{
    // Группировщик виджетов настроек избранного
    favoritesBox=new QGroupBox(this);
    favoritesBox->setTitle(tr("Favorite notes"));

    // Виджеты вставляются в группировщик настроек избранного
    QVBoxLayout *favoritesLayout = new QVBoxLayout;
    favoritesLayout->addWidget(enableFavoritesPanel);
    favoritesBox->setLayout(favoritesLayout);

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(favoritesBox);
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout(centralLayout);
}


// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Favorites::applyChanges()
{
    qDebug() << "Apply changes favorites";

    int result=0;

    // Сохраняется показ панели избранного
    if(mytetraConfig.get_favoritesEnabled()!=enableFavoritesPanel->isChecked())
    {
        mytetraConfig.set_favoritesEnabled(enableFavoritesPanel->isChecked());

        // Панель применяется живьем без перезапуска. Сам refresh решит
        // показать или спрятать по наличию звездочек
        FavoritesPanel *favoritesPanel=find_object<FavoritesPanel>("favoritesPanel");
        if(favoritesPanel!=nullptr)
            favoritesPanel->refreshFavorites();
    }

    return result;
}

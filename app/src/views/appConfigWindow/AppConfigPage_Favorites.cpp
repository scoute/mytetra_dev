#include <QDebug>
#include <QVBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>

#include "AppConfigPage_Favorites.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/Favorites.h"
#include "libraries/helpers/ObjectHelper.h"
#include "views/favoritesPanel/FavoritesPanel.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;


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

    // Хранилище звездочек: сайдкар data/favorites.xml (по умолчанию)
    // или поля favorite прямо в mytetra.xml
    sidecarRadio=new QRadioButton(this);
    sidecarRadio->setText(tr("Sidecar file (data/favorites.xml)"));

    xmlRadio=new QRadioButton(this);
    xmlRadio->setText(tr("Main file (mytetra.xml)"));

    if(mytetraConfig.get_favoritesStorage()==Favorites::StorageXml)
      xmlRadio->setChecked(true);
    else
      sidecarRadio->setChecked(true);

    storageHint=new QLabel(this);
    storageHint->setText(tr("Switching storage moves existing stars"));
    storageHint->setWordWrap(true);
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

    // Группировщик выбора хранилища
    storageBox=new QGroupBox(this);
    storageBox->setTitle(tr("Favorites storage"));

    QVBoxLayout *storageLayout = new QVBoxLayout;
    storageLayout->addWidget(sidecarRadio);
    storageLayout->addWidget(xmlRadio);
    storageLayout->addWidget(storageHint);
    storageBox->setLayout(storageLayout);

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(favoritesBox);
    centralLayout->addWidget(storageBox);
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout(centralLayout);
}


void AppConfigPage_Favorites::retranslateUi(void)
{
  enableFavoritesPanel->setText(tr("Show favorites panel"));
  favoritesBox->setTitle(tr("Favorite notes"));

  sidecarRadio->setText(tr("Sidecar file (data/favorites.xml)"));
  xmlRadio->setText(tr("Main file (mytetra.xml)"));
  storageHint->setText(tr("Switching storage moves existing stars"));
  storageBox->setTitle(tr("Favorites storage"));
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

    // Смена хранилища: звездочки переносятся, панель обновляется сигналом
    const QString wantedStorage=xmlRadio->isChecked() ? Favorites::StorageXml
                                                      : Favorites::StorageSidecar;

    if(mytetraConfig.get_favoritesStorage()!=wantedStorage)
        Favorites::instance().setStorage(wantedStorage);

    return result;
}

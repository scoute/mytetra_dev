#include <QStackedWidget>
#include <QObject>
#include <QDialog>
#include <QDebug>
#include <QWidget>
#include <QListWidgetItem>

#include "ConfigDialog.h"
#include "ConfigPage.h"
#include "AppConfigDialog.h"
#include "AppConfigPage_Main.h"
#include "AppConfigPage_Crypt.h"
#include "AppConfigPage_Misc.h"
#include "AppConfigPage_Synchro.h"
#include "AppConfigPage_RecordTable.h"
#include "AppConfigPage_Attach.h"
#include "AppConfigPage_Keyboard.h"
#include "AppConfigPage_History.h"
#include "AppConfigPage_Search.h"
#include "AppConfigPage_Favorites.h"
#include "AppConfigPage_Clipper.h"
#include "AppConfigPage_Mcp.h"
#include "AppConfigPage_Appearance.h"
#include "AppConfigPage_Theme.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/helpers/ScreenHelper.h"


extern AppConfig mytetraConfig;


QList<AppConfigDialog *> AppConfigDialog::openDialogs;


AppConfigDialog::AppConfigDialog(const QString &firstPageName, QWidget *parent)
{
    // Создание диалога с parent обеспечивает отображение иконки приложения
    // в диалоговом окне и на панели задач
    configDialog=new ConfigDialog( parent );
    configDialog->setAttribute(Qt::WA_DeleteOnClose);
    configDialog->setWindowTitle(QObject::tr("MyTetra settings"));

    if(mytetraConfig.getInterfaceMode()=="mobile")
    {
        qDebug() << "Screen size X Y: " << ScreenHelper::getSizeX() << ScreenHelper::getSizeY();
        configDialog->setMinimumSize(ScreenHelper::getSizeX(), ScreenHelper::getSizeY());
        configDialog->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    }

    // Наполнение диалога виджетами
    // В качестве родителя надо указывать parent а не configDialog (разобраться почему)
    pageMain       =configDialog->addWidget(new AppConfigPage_Main( parent ),
                                            QObject::tr("Main"));
    pageAppearance=configDialog->addWidget(new AppConfigPage_Appearance( parent ),
                                            QObject::tr("Appearance"));
    pageTheme      =configDialog->addWidget(new AppConfigPage_Theme( parent ),
                                            QObject::tr("Themes"));
    pageCrypt      =configDialog->addWidget(new AppConfigPage_Crypt( parent ),
                                            QObject::tr("Crypt"));
    pageSynchro    =configDialog->addWidget(new AppConfigPage_Synchro( parent ),
                                            QObject::tr("Synchro"));
    pageRecordTable=configDialog->addWidget(new AppConfigPage_RecordTable( parent ),
                                            QObject::tr("Notes area"));
    pageAttach     =configDialog->addWidget(new AppConfigPage_Attach( parent ),
                                            QObject::tr("Attaches"));
    pageKeyboard   =configDialog->addWidget(new AppConfigPage_Keyboard( parent ),
                                            QObject::tr("Keyboard"));
    pageHistory    =configDialog->addWidget(new AppConfigPage_History( parent ),
                                            QObject::tr("History"));
    pageSearch     =configDialog->addWidget(new AppConfigPage_Search( parent ),
                                            QObject::tr("Search"));
    pageFavorites  =configDialog->addWidget(new AppConfigPage_Favorites( parent ),
                                            QObject::tr("Favorites"));
    pageClipper    =configDialog->addWidget(new AppConfigPage_Clipper( parent ),
                                            QObject::tr("Clipper"));
    pageMcp        =configDialog->addWidget(new AppConfigPage_Mcp( parent ),
                                            QObject::tr("MCP"));
    pageMisc       =configDialog->addWidget(new AppConfigPage_Misc( parent ),
                                            QObject::tr("Misc"));


    configDialog->updateListWidth();

    // Если указано имя виджета настроек, происходит переключение на него,
    // иначе будет выбран первый виджет (pageMain)
    if(firstPageName.size()>0)
        changePage(firstPageName);
}


AppConfigDialog::~AppConfigDialog()
{
    qDebug() << Q_FUNC_INFO;

    openDialogs.removeAll(this);
}


void AppConfigDialog::exec()
{
    qDebug() << "Before config dialog exec";

    openDialogs.append(this);
    configDialog->exec();
    openDialogs.removeAll(this);

    qDebug() << "Config dialog exec finish";
}


// Прямые тексты новым языком: заголовок, пункты списка страниц
// и все страницы. Вызывается при смене языка из страницы Main
void AppConfigDialog::retranslateUi(void)
{
    if(configDialog==nullptr)
        return;

    configDialog->setWindowTitle(QObject::tr("MyTetra settings"));

    pageMain->setText(QObject::tr("Main"));
    pageAppearance->setText(QObject::tr("Appearance"));
    pageTheme->setText(QObject::tr("Themes"));
    pageCrypt->setText(QObject::tr("Crypt"));
    pageSynchro->setText(QObject::tr("Synchro"));
    pageRecordTable->setText(QObject::tr("Notes area"));
    pageAttach->setText(QObject::tr("Attaches"));
    pageKeyboard->setText(QObject::tr("Keyboard"));
    pageHistory->setText(QObject::tr("History"));
    pageSearch->setText(QObject::tr("Search"));
    pageFavorites->setText(QObject::tr("Favorites"));
    pageClipper->setText(QObject::tr("Clipper"));
    pageMcp->setText(QObject::tr("MCP"));
    pageMisc->setText(QObject::tr("Misc"));

    QList<ConfigPage *> pages=configDialog->findChildren<ConfigPage *>();

    foreach(ConfigPage *page, pages)
        page->retranslateUi();
}


// Переход на нужную страницу настроек (переход совершается кодом извне)
// Данный метод нужен для возможности вызова конкретного окна настроек из любого места программы
void AppConfigDialog::changePage(QString name)
{
    QListWidgetItem *item=nullptr;

    if(name=="pageMain") item=pageMain;
    if(name=="pageAppearance") item=pageAppearance;
    if(name=="pageTheme") item=pageTheme;
    if(name=="pageCrypt") item=pageCrypt;
    if(name=="pageSynchro") item=pageSynchro;
    if(name=="pageRecordTable") item=pageRecordTable;
    if(name=="pageAttach") item=pageAttach;
    if(name=="pageKeyboard") item=pageKeyboard;
    if(name=="pageHistory") item=pageHistory;
    if(name=="pageSearch") item=pageSearch;
    if(name=="pageFavorites") item=pageFavorites;
    if(name=="pageClipper") item=pageClipper;
    if(name=="pageMcp") item=pageMcp;
    if(name=="pageMisc") item=pageMisc;

    if(item!=nullptr)
        configDialog->externalChangePage(item);
    else
        qDebug() << "AppConfigDialog::changePage cant find item for name: " << name;
}


void AppConfigDialog::setMenuListVisible(bool visible)
{
    configDialog->setMenuListVisible(visible);
}

#include <QStackedWidget>
#include <QObject>
#include <QDialog>
#include <QDebug>
#include <QWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDate>

#include "ConfigDialog.h"
#include "ConfigPage.h"
#include "AppConfigDialog.h"
#include "AppConfigPage_Main.h"
#include "AppConfigPage_Crypt.h"
#include "AppConfigPage_Misc.h"
#include "AppConfigPage_Synchro.h"
#include "AppConfigPage_RecordTable.h"
#include "AppConfigPage_Tree.h"
#include "AppConfigPage_Attach.h"
#include "AppConfigPage_Keyboard.h"
#include "AppConfigPage_History.h"
#include "AppConfigPage_Search.h"
#include "AppConfigPage_Favorites.h"
#include "AppConfigPage_Clipper.h"
#include "AppConfigPage_Mcp.h"
#include "AppConfigPage_Voice.h"
#include "AppConfigPage_Team.h"
#include "AppConfigPage_Appearance.h"
#include "AppConfigPage_Theme.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/helpers/ScreenHelper.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/EditorConfigDialog.h"
#include "libraries/wyedit/EditorConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/ShortcutManager.h"
#include "models/appConfig/SettingsTransfer.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern ShortcutManager shortcutManager;


QList<AppConfigDialog *> AppConfigDialog::openDialogs;


AppConfigDialog::AppConfigDialog(const QString &firstPageName, QWidget *parent)
{
    // Создание диалога с parent обеспечивает отображение иконки приложения
    // в диалоговом окне и на панели задач
    configDialog=new ConfigDialog( parent );
    configDialog->setAttribute(Qt::WA_DeleteOnClose);
    configDialog->setWindowTitle(QObject::tr("SyncTetra settings"));

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
    pageTree=configDialog->addWidget(new AppConfigPage_Tree( parent ),
                                            QObject::tr("Branch tree"));
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
    pageTeam       =configDialog->addWidget(new AppConfigPage_Team( parent ),
                                            QObject::tr("Teamwork"));
    pageVoice      =configDialog->addWidget(new AppConfigPage_Voice( parent ),
                                            QObject::tr("Voice notes"));
    pageMisc       =configDialog->addWidget(new AppConfigPage_Misc( parent ),
                                            QObject::tr("Misc"));

    // Кнопки переноса настроек в нижний ряд слева: экспорт и
    // импорт одним INI-файлом вместо ручного копирования конфигов.
    // Редакторный диалог их не ставит и не меняется
    exportSettingsButton=new QPushButton(QObject::tr("Export settings..."), configDialog);
    importSettingsButton=new QPushButton(QObject::tr("Import settings..."), configDialog);
    configDialog->addFooterButton(exportSettingsButton);
    configDialog->addFooterButton(importSettingsButton);

    QObject::connect(exportSettingsButton, &QPushButton::clicked,
                     configDialog, [this]() { onExportSettings(); });
    QObject::connect(importSettingsButton, &QPushButton::clicked,
                     configDialog, [this]() { onImportSettings(); });

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

    configDialog->setWindowTitle(QObject::tr("SyncTetra settings"));

    pageMain->setText(QObject::tr("Main"));
    pageAppearance->setText(QObject::tr("Appearance"));
    pageTheme->setText(QObject::tr("Themes"));
    pageCrypt->setText(QObject::tr("Crypt"));
    pageSynchro->setText(QObject::tr("Synchro"));
    pageRecordTable->setText(QObject::tr("Notes area"));
    pageTree->setText(QObject::tr("Branch tree"));
    pageAttach->setText(QObject::tr("Attaches"));
    pageKeyboard->setText(QObject::tr("Keyboard"));
    pageHistory->setText(QObject::tr("History"));
    pageSearch->setText(QObject::tr("Search"));
    pageFavorites->setText(QObject::tr("Favorites"));
    pageClipper->setText(QObject::tr("Clipper"));
    pageMcp->setText(QObject::tr("MCP"));
    pageTeam->setText(QObject::tr("Teamwork"));
    pageVoice->setText(QObject::tr("Voice notes"));
    pageMisc->setText(QObject::tr("Misc"));

    if(exportSettingsButton!=nullptr)
        exportSettingsButton->setText(QObject::tr("Export settings..."));

    if(importSettingsButton!=nullptr)
        importSettingsButton->setText(QObject::tr("Import settings..."));

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
    if(name=="pageTree") item=pageTree;
    if(name=="pageAttach") item=pageAttach;
    if(name=="pageKeyboard") item=pageKeyboard;
    if(name=="pageHistory") item=pageHistory;
    if(name=="pageSearch") item=pageSearch;
    if(name=="pageFavorites") item=pageFavorites;
    if(name=="pageClipper") item=pageClipper;
    if(name=="pageMcp") item=pageMcp;
    if(name=="pageTeam") item=pageTeam;
    if(name=="pageVoice") item=pageVoice;
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


// Выгрузка настроек в INI-файл: шрифты, цвета, список заметок,
// иконки, клавиатура и остальные переносимые параметры.
// Версии схем, пути базы/корзины и секреты шифрования не выгружаются
void AppConfigDialog::onExportSettings(void)
{
  const QString workDir=globalParameters.getWorkDirectory();

  const QString defaultName=workDir+QStringLiteral("/mytetra-settings-")+
    QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))+QStringLiteral(".ini");

  const QString fileName=QFileDialog::getSaveFileName(configDialog,
    QObject::tr("Export settings"), defaultName,
    QObject::tr("MyTetra settings (*.ini)"));

  if(fileName.isEmpty())
    return;

  EditorConfig *editorConfig=find_object<EditorConfig>(QStringLiteral("editorconfig"));

  if(editorConfig==nullptr)
  {
    QMessageBox::warning(configDialog,
      QObject::tr("Export settings"),
      QObject::tr("Editor settings are unavailable"));
    return;
  }

  QString error;

  if(!SettingsTransfer::exportSettings(mytetraConfig.settingsHandle(),
                                       editorConfig->settingsHandle(),
                                       workDir+QStringLiteral("/shortcut.ini"),
                                       fileName, &error))
  {
    QMessageBox::warning(configDialog,
      QObject::tr("Export settings"), error);
    return;
  }

  QMessageBox::information(configDialog,
    QObject::tr("Export settings"),
    QObject::tr("Settings were exported to %1").arg(fileName));
}


// Загрузка настроек из файла: резервная копия текущих, применение,
// перезапуск. Несохраненные правки на вкладках отбрасываются:
// диалог закрывается без apply, чтобы устаревшие значения из памяти
// не затерли только что загруженные
void AppConfigDialog::onImportSettings(void)
{
  if(QMessageBox::question(configDialog,
      QObject::tr("Import settings"),
      QObject::tr("Settings from the file will replace the current ones. "
                  "A backup copy will be created. "
                  "Unsaved changes on the tabs will be discarded. Continue?"),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No)!=QMessageBox::Yes)
    return;

  const QString workDir=globalParameters.getWorkDirectory();

  const QString fileName=QFileDialog::getOpenFileName(configDialog,
    QObject::tr("Import settings"), workDir,
    QObject::tr("MyTetra settings (*.ini)"));

  if(fileName.isEmpty())
    return;

  EditorConfig *editorConfig=find_object<EditorConfig>(QStringLiteral("editorconfig"));

  if(editorConfig==nullptr)
  {
    QMessageBox::warning(configDialog,
      QObject::tr("Import settings"),
      QObject::tr("Editor settings are unavailable"));
    return;
  }

  QString error;

  if(!SettingsTransfer::importSettings(mytetraConfig.settingsHandle(),
                                       editorConfig->settingsHandle(),
                                       workDir+QStringLiteral("/shortcut.ini"),
                                       fileName, &error))
  {
    QMessageBox::warning(configDialog,
      QObject::tr("Import settings"), error);
    return;
  }

  // Горячие клавиши перечитываются из нового файла сразу,
  // остальное (тема, шрифты, раскладка) подхватится перезапуском
  shortcutManager.reload();

  QMessageBox::information(configDialog,
    QObject::tr("Import settings"),
    QObject::tr("Settings were imported. Please restart the program."));

  configDialog->reject();
}

// Цель Windows — до ЛЮБЫХ инклудов: Qt-заголовки сами тянут windows.h,
// и поздний _WIN32_WINNT уже ничего не даёт (QueryFullProcessImageNameA
// из mcpPeerAlive() тогда не объявлена, MinGW32 падает)
#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <QDialog>
#include <QMap>
#include <QString>
#include <QDir>
#include <QString>
#include <QDesktopWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPrintDialog>
#include <QPrinter>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSplitter>
#include <QDockWidget>
#include <QStatusBar>
#include <QActionGroup>
#include <QColorDialog>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>

#ifdef Q_OS_WIN32
#include <windows.h>
#else
#include <signal.h>
#include <errno.h>
#endif
#include <QTime>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>


// Элемент таблицы с числовой сортировкой: по клику на заголовок
// QTableWidget сравнивает operator<, текст "156" < "20" строкой бы врал
class NumberTableItem : public QTableWidgetItem
{
public:
  explicit NumberTableItem(int value) : QTableWidgetItem(QString::number(value))
  {
    setData(Qt::UserRole, value);
  }

  bool operator<(const QTableWidgetItem &other) const override
  {
    return data(Qt::UserRole).toInt() < other.data(Qt::UserRole).toInt();
  }
};

#include "main.h"
#include "models/appConfig/AppConfig.h"
#include "MainWindow.h"
#include "views/printPreview/PrintPreview.h"
#include "views/appConfigWindow/AppConfigDialog.h"
#include "libraries/TrashMonitoring.h"
#include "views/tree/TreeScreen.h"
#include "views/telegram/TelegramImportDialog.h"
#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorToolBarAssistant.h"
#include "libraries/wyedit/EditorConfig.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/historyPanel/HistoryPanel.h"
#include "views/favoritesPanel/FavoritesPanel.h"
#include "views/backlinksPanel/BacklinksPanel.h"
#include "libraries/BacklinkIndex.h"
#include "views/tagsPanel/TagsPanel.h"
#include "views/contentGallery/ContentGallery.h"
#include "models/tree/TreeItem.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/GlobalParameters.h"
#include "views/consoleEmulator/CommandRunner.h"
#include "libraries/WalkHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/WindowSwitcher.h"
#include "views/actionLog/ActionLogScreen.h"
#include "views/databasesManagement/DatabasesManagementScreen.h"
#include "libraries/ShortcutManager.h"
#include "libraries/RandomInitter.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/CssHelper.h"
#include "libraries/mcp/McpTools.h"
#include "libraries/Favorites.h"
#include "models/recordTable/Record.h"


extern AppConfig mytetraConfig;
extern FixedParameters fixedParameters;
extern TrashMonitoring trashMonitoring;
extern GlobalParameters globalParameters;
extern WalkHistory walkHistory;
extern ActionLogger actionLogger;
extern ShortcutManager shortcutManager;
extern MainWindow *pMainWindow;


// Создание раздела меню File
void MainWindow::initFileMenu(void)
{
    // Создание меню
    fileMenu = new QMenu(tr("&File"), this);
    this->menuBar()->addMenu(fileMenu);

    actionFileMenuDatabasesManagement = new QAction(tr("Databases management"), this);
    fileMenu->addAction(actionFileMenuDatabasesManagement);

    actionFileMenuExportTreeItem = new QAction(tr("Export tree item"), this);
    fileMenu->addAction(actionFileMenuExportTreeItem);

    actionFileMenuImportTreeItem = new QAction(tr("Import tree item"), this);
    fileMenu->addAction(actionFileMenuImportTreeItem);
    actionFileMenuImportTelegram = new QAction(tr("Import Telegram chat..."), this);
    fileMenu->addAction(actionFileMenuImportTelegram);

    fileMenu->addSeparator();

    actionFileMenuPrint = new QAction(tr("&Print..."), this);
    fileMenu->addAction(actionFileMenuPrint);

    actionFileMenuPrintPreview = new QAction(tr("Print Preview..."), this);
    fileMenu->addAction(actionFileMenuPrintPreview);

    actionFileMenuExportPdf = new QAction(tr("&Export PDF..."), this);
    fileMenu->addAction(actionFileMenuExportPdf);

    fileMenu->addSeparator();

    actionFileMenuQuit = new QAction(tr("&Quit"), this);
    fileMenu->addAction(actionFileMenuQuit);
}


// Создание раздела меню Tools


// Создание раздела меню Tools
void MainWindow::initToolsMenu(void)
{
    // Создание меню
    toolsMenu = new QMenu(tr("&Tools"), this);
    this->menuBar()->addMenu(toolsMenu);

    actionToolsMenuFindInBase = new QAction(this); // Так как есть this, указатель не будет потерян основным окном
    toolsMenu->addAction(actionToolsMenuFindInBase);

    actionToolsMenuImagesGallery = new QAction(tr("Images &gallery"), this);
    toolsMenu->addAction(actionToolsMenuImagesGallery);

    actionToolsMenuFilesGallery = new QAction(tr("Attached &files"), this);
    toolsMenu->addAction(actionToolsMenuFilesGallery);

    actionToolsMenuActionLog = new QAction(tr("Action &log"), this);
    toolsMenu->addAction(actionToolsMenuActionLog);

    actionToolsMenuClipFromClipboard = new QAction(this);
    toolsMenu->addAction(actionToolsMenuClipFromClipboard);

    // Переключатель панели меток: видимость дока и галочка синхронны сами
    QAction *tagsPanelToggle=tagsPanelDock->toggleViewAction();
    tagsPanelToggle->setIcon(QIcon(":/resource/pic/tag.svg"));
    toolsMenu->addAction(tagsPanelToggle);

    shortcutManager.initAction("misc-tagsPanel", tagsPanelToggle);

    // Переключатель панели истории: видимость дока и галочка синхронны сами
    QAction *historyPanelToggle=historyPanelDock->toggleViewAction();
    historyPanelToggle->setIcon(QIcon(":/resource/pic/history.svg"));
    toolsMenu->addAction(historyPanelToggle);

    shortcutManager.initAction("misc-historyPanel", historyPanelToggle);

    // Переключатель панели входящих ссылок: видимость дока и галочка синхронны сами
    QAction *backlinksPanelToggle=backlinksPanelDock->toggleViewAction();
    backlinksPanelToggle->setIcon(QIcon(":/resource/pic/backlinks.svg"));
    toolsMenu->addAction(backlinksPanelToggle);

    shortcutManager.initAction("misc-backlinksPanel", backlinksPanelToggle);

    toolsMenu->addSeparator();

    if(mytetraConfig.getInterfaceMode()=="desktop")
    {
        actionToolsMenuPreferences = new QAction(tr("&Preferences"), this);
        toolsMenu->addAction(actionToolsMenuPreferences);
    }
    else
    {
        // Создание подменю
        preferencesSubMenu = new QMenu(tr("&Preferences"), this);
        initPreferencesMenu(preferencesSubMenu);
        toolsMenu->addMenu(preferencesSubMenu);
    }
}


// Заполнение подраздела меню Preferences (для мобильного интерфейса)


// Заполнение подраздела меню Preferences (для мобильного интерфейса)
void MainWindow::initPreferencesMenu(QMenu *menu)
{
    QMap< QString, QAction **> map;

    map[tr("Main")]       = &actionDirectPreferencesMain;
    map[tr("Appearance")] = &actionDirectPreferencesAppearance;
    map[tr("Crypt")]      = &actionDirectPreferencesCrypt;
    map[tr("Syncro")]     = &actionDirectPreferencesSyncro;
    map[tr("Notes Area")] = &actionDirectPreferencesRecordTable;
    map[tr("Attaches")]   = &actionDirectPreferencesAttach;
    map[tr("Keyboard")]   = &actionDirectPreferencesKeyboard;
    map[tr("History")]    = &actionDirectPreferencesHistory;
    map[tr("Misc")]       = &actionDirectPreferencesMisc;

    for(auto pageName : map.keys())
    {
        *map[pageName] = new QAction(pageName, this);
        menu->addAction( *map[pageName] );
    }
}


void MainWindow::runDirectPreferences(QAction *action)
{
    QMap< QString, QAction *> map;

    map["pageMain"]       = actionDirectPreferencesMain;
    map["pageAppearance"] = actionDirectPreferencesAppearance;
    map["pageCrypt"]      = actionDirectPreferencesCrypt;
    map["pageSynchro"]    = actionDirectPreferencesSyncro;
    map["pageRecordTable"]= actionDirectPreferencesRecordTable;
    map["pageAttach"]     = actionDirectPreferencesAttach;
    map["pageKeyboard"]   = actionDirectPreferencesKeyboard;
    map["pageHistory"]    = actionDirectPreferencesHistory;
    map["pageMisc"]       = actionDirectPreferencesMisc;

    AppConfigDialog *dialog=nullptr;

    for(auto pageName : map.keys())
    {
        if( map[pageName]==action )
        {
            dialog=new AppConfigDialog(pageName, this);
            break;
        }
    }

    if(dialog!=nullptr)
    {
        dialog->setMenuListVisible(false);
        dialog->exec();

        // Настройки могли поменять мастер-тумблеры (история и пр.):
        // состояние пунктов перечитывается сразу, а не по следующему клику
        if(recordTableScreen!=nullptr)
            recordTableScreen->toolsUpdate();
    }

    delete dialog;
}


// Создание раздела меню Help


// Создание раздела меню Help
void MainWindow::initHelpMenu(void)
{
    // Создание меню
    helpMenu = new QMenu(tr("&Help"), this);
    this->menuBar()->addMenu(helpMenu);

    actionHelpMenuAboutMyTetra = new QAction(tr("About MyTetra"), this);
    helpMenu->addAction(actionHelpMenuAboutMyTetra);

    actionHelpMenuAboutQt = new QAction(tr("About Qt"), this);
    helpMenu->addAction(actionHelpMenuAboutQt);

    actionHelpMenuTechnicalInfo = new QAction(tr("Technical info"), this);
    helpMenu->addAction(actionHelpMenuTechnicalInfo);
}


// Создание меню Темы: переключение интерфейса в один клик вместо
// похода в настройки. Пункты строятся из списка доступных тем,
// поэтому новая тема подхватится сама


// Создание меню Темы: переключение интерфейса в один клик вместо
// похода в настройки. Пункты строятся из списка доступных тем,
// поэтому новая тема подхватится сама
void MainWindow::initThemesMenu(void)
{
    themesMenu=new QMenu(tr("&Themes"), this);
    this->menuBar()->addMenu(themesMenu);

    // Таблица переводов имен тем как в диалоге настроек
    QMap<QString, QString> translateNames;
    translateNames["default"]=tr("Default");
    translateNames["dark"]=tr("Dark");
    translateNames["modern"]=tr("Modern");
    translateNames["midnight"]=tr("Midnight");
    translateNames["custom"]=tr("Custom");

    QActionGroup *themeGroup=new QActionGroup(themesMenu);
    themeGroup->setExclusive(true);

    for(auto name : fixedParameters.themesAvailableList)
    {
        QAction *themeAction=new QAction(translateNames.value(name, "Unknown"), themesMenu);
        themeAction->setData(name);
        themeAction->setCheckable(true);
        themeGroup->addAction(themeAction);
        themesMenu->addAction(themeAction);
    }

    themesMenu->addSeparator();

    // Настройка своей темы: то же окно, что страница Themes
    // в диалоге настроек. Вне эксклюзивной группы осознанно:
    // это действие, а не выбор темы
    themeConfigureAction=new QAction(tr("Configure theme..."), themesMenu);
    themesMenu->addAction(themeConfigureAction);
    connect(themeConfigureAction, &QAction::triggered,
            this,                &MainWindow::onThemeConfigure);

    syncThemeMenu();

    connect(themeGroup, &QActionGroup::triggered,
            this,        &MainWindow::onThemeMenuTriggered);

    connect(themesMenu, &QMenu::aboutToShow,
            this,        &MainWindow::syncThemeMenu);
}


// Пометка текущей темы. Вызывается при открытии меню, так как тему
// можно сменить и из диалога настроек мимо этого меню


// Пометка текущей темы. Вызывается при открытии меню, так как тему
// можно сменить и из диалога настроек мимо этого меню
void MainWindow::syncThemeMenu(void)
{
    if(themesMenu==nullptr)
        return;

    QString currentTheme=mytetraConfig.getInterfaceTheme();

    for(QAction *themeAction : themesMenu->actions())
    {
        // Разделитель и «Настроить тему» — не выборы темы
        if(themeAction==nullptr || themeAction->isSeparator() ||
           themeAction->data().toString().isEmpty())
            continue;

        themeAction->setChecked(themeAction->data().toString()==currentTheme);
    }
}


// Меню Вид между Tools и Themes: дублирует быстрые переключатели
// отображения (шапка заметки, избранное, цвет секрета, бумага), чтобы не
// лазить за ними в диалог настроек или контекстное меню редактора


// Меню Вид между Tools и Themes: дублирует быстрые переключатели
// отображения (шапка заметки, избранное, цвет секрета, бумага), чтобы не
// лазить за ними в диалог настроек или контекстное меню редактора
void MainWindow::initViewMenu(void)
{
    viewMenu=new QMenu(tr("&View"), this);
    this->menuBar()->addMenu(viewMenu);

    viewHeaderAction=new QAction(tr("Show note header"), this);
    viewHeaderAction->setCheckable(true);
    viewMenu->addAction(viewHeaderAction);

    viewFavoritesAction=new QAction(tr("Show favorites panel"), this);
    viewFavoritesAction->setCheckable(true);
    viewMenu->addAction(viewFavoritesAction);

    viewTreeFilterAction=new QAction(tr("Show branch filter"), this);
    viewTreeFilterAction->setCheckable(true);
    viewMenu->addAction(viewTreeFilterAction);

    viewToolsAction=new QAction(tr("Show editor toolbar"), this);
    viewToolsAction->setCheckable(true);
    viewToolsAction->setObjectName(QStringLiteral("viewToolsAction"));
    viewMenu->addAction(viewToolsAction);

    viewReadOnlyAction=new QAction(this);
    viewReadOnlyAction->setCheckable(true);
    viewMenu->addAction(viewReadOnlyAction);

    viewMenu->addSeparator();

    QAction *secretColorAction=new QAction(tr("Change secret color..."), this);
    viewSecretColorAction=secretColorAction;
    viewMenu->addAction(secretColorAction);

    QAction *paperColorAction=new QAction(tr("Change note background color..."), this);
    viewPaperColorAction=paperColorAction;
    viewMenu->addAction(paperColorAction);

    // Начальная пометка до коннектов: setChecked с тем же значением
    // toggled не дергает, лишних записей в конфиг нет
    syncViewMenu();

    connect(viewHeaderAction, &QAction::toggled,
            this,              &MainWindow::onViewHeaderToggled);
    connect(viewFavoritesAction, &QAction::toggled,
            this,                &MainWindow::onViewFavoritesToggled);
    connect(viewTreeFilterAction, &QAction::toggled,
            this,                  &MainWindow::onViewTreeFilterToggled);
    connect(viewToolsAction, &QAction::toggled,
            this,            &MainWindow::onViewToolsToggled);
    connect(viewReadOnlyAction, &QAction::toggled,
            this,               &MainWindow::onViewReadOnlyToggled);
    connect(secretColorAction, &QAction::triggered,
            this,              &MainWindow::onViewSecretColor);
    connect(paperColorAction, &QAction::triggered,
            this,              &MainWindow::onViewPaperColor);

    connect(viewMenu, &QMenu::aboutToShow,
            this,      &MainWindow::syncViewMenu);
}


// Пометка текущих состояний при каждом открытии: все меняется
// и мимо меню (диалог настроек, контекстное меню секрета)


// Пометка текущих состояний при каждом открытии: все меняется
// и мимо меню (диалог настроек, контекстное меню секрета)
void MainWindow::syncViewMenu(void)
{
    if(viewHeaderAction!=nullptr)
        viewHeaderAction->setChecked(mytetraConfig.get_recordHeaderVisible());

    if(viewFavoritesAction!=nullptr)
        viewFavoritesAction->setChecked(mytetraConfig.get_favoritesEnabled());

    if(viewTreeFilterAction!=nullptr)
        viewTreeFilterAction->setChecked(mytetraConfig.get_treeFilterVisible());

    // Панель может прятаться мимо меню (кнопка toggleTools, хоткей):
    // пометка подтягивается при каждом открытии
    if(viewToolsAction!=nullptr && editorScreen!=nullptr &&
       editorScreen->editorToolBarAssistant!=nullptr)
        viewToolsAction->setChecked(!editorScreen->editorConfig->get_hide_tools_lines());

    if(viewReadOnlyAction!=nullptr)
        viewReadOnlyAction->setChecked(mytetraConfig.isGlobalReadOnly());
}


void MainWindow::onViewHeaderToggled(bool checked)
{
    mytetraConfig.set_recordHeaderVisible(checked);

    if(editorScreen!=nullptr)
        editorScreen->updateRecordHeaderVisibility();
}


void MainWindow::onViewFavoritesToggled(bool checked)
{
    mytetraConfig.set_favoritesEnabled(checked);

    // Сам refresh решит показать или спрятать по наличию звездочек
    FavoritesPanel *favoritesPanel=find_object<FavoritesPanel>("favoritesPanel");
    if(favoritesPanel!=nullptr)
        favoritesPanel->refreshFavorites();
}


void MainWindow::onViewTreeFilterToggled(bool checked)
{
    mytetraConfig.set_treeFilterVisible(checked);

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen!=nullptr)
        treeScreen->setTreeFilterVisible(checked);
}


// Возврат спрятанной панели инструментов: чекбокс Вид всегда доступен,
// в отличие от кнопки toggleTools, которая уезжает вместе с панелью


// Возврат спрятанной панели инструментов: чекбокс Вид всегда доступен,
// в отличие от кнопки toggleTools, которая уезжает вместе с панелью
void MainWindow::onViewToolsToggled(bool checked)
{
    if(editorScreen==nullptr || editorScreen->editorToolBarAssistant==nullptr)
        return;

    editorScreen->editorToolBarAssistant->switchToolsVisibility(checked ? 1 : -1);
}


// Переключение глобального режима только чтения: навигация и просмотр
// разрешены, любые изменения базы запрещены


// Переключение глобального режима только чтения: навигация и просмотр
// разрешены, любые изменения базы запрещены
void MainWindow::onViewReadOnlyToggled(bool checked)
{
    mytetraConfig.set_readOnly(checked);

    applyReadOnly();
}


// Применить глобальный режим только чтения ко всем экранам.
// Вызывается при переключении и при старте программы


// Применить глобальный режим только чтения ко всем экранам.
// Вызывается при переключении и при старте программы
void MainWindow::applyReadOnly(void)
{
    const bool readOnly=mytetraConfig.get_readOnly();

    if(treeScreen!=nullptr)
        treeScreen->setReadOnly(readOnly);

    // Действия таблицы записей пересчитываются с учётом флага,
    // редактор — по текущей записи и флагу
    if(recordTableScreen!=nullptr)
        {
         recordTableScreen->toolsUpdate();
         recordTableScreen->refreshEditorReadOnly();

         // Кнопка синхронизации меняет базу (и дёргает её же автосинхронизация)
         recordTableScreen->setSyncroEnabled(!readOnly);
        }

    // Импорт ветки и управление базами меняют данные
    if(actionFileMenuImportTreeItem!=nullptr)
        actionFileMenuImportTreeItem->setEnabled(!readOnly);

    if(actionFileMenuImportTelegram!=nullptr)
        actionFileMenuImportTelegram->setEnabled(!readOnly);

    if(actionFileMenuDatabasesManagement!=nullptr)
        actionFileMenuDatabasesManagement->setEnabled(!readOnly);
}


void MainWindow::onViewSecretColor(void)
{
    const QColor current(mytetraConfig.get_secretColor());
    const QColor chosen=QColorDialog::getColor(current, this);

    if(!chosen.isValid() || chosen==current)
        return;

    mytetraConfig.set_secretColor(chosen.name());

    // Сразу на диск как в контекстном меню секрета
    mytetraConfig.sync();

    // Цвет действует только на Custom: быстрый выбор сам переводит
    // туда с текущей темой как базой, иначе клик ничего не меняет
    if(mytetraConfig.getInterfaceTheme()!="custom")
    {
        mytetraConfig.set_themeCustomBaseTheme(mytetraConfig.getInterfaceTheme());

        if(mytetraConfig.setInterfaceTheme("custom"))
            CssHelper::loadCurrentTheme();
    }

    if(editorScreen!=nullptr)
        editorScreen->repaintSecrets();
}


void MainWindow::onViewPaperColor(void)
{
    const QColor current(
        MetaEditor::effectiveNotePaperColor(mytetraConfig.getInterfaceTheme(),
                                           mytetraConfig.get_notePaperColor()));
    const QColor chosen=QColorDialog::getColor(current, this);

    if(!chosen.isValid() || chosen==current)
        return;

    mytetraConfig.set_notePaperColor(chosen.name());

    // Сразу на диск как в соседнем пункте про цвет секрета
    mytetraConfig.sync();

    // Бумага действует только на Custom: быстрый выбор сам переводит
    // туда с текущей темой как базой, иначе клик ничего не меняет
    if(mytetraConfig.getInterfaceTheme()!="custom")
    {
        mytetraConfig.set_themeCustomBaseTheme(mytetraConfig.getInterfaceTheme());

        if(mytetraConfig.setInterfaceTheme("custom"))
            CssHelper::loadCurrentTheme();
    }

    if(editorScreen!=nullptr)
        editorScreen->applyNoteColors();
}


// Клик по пункту меню Темы: применить выбранную тему сразу


// Клик по пункту меню Темы: применить выбранную тему сразу
void MainWindow::onThemeMenuTriggered(QAction *action)
{
    if(action==nullptr)
        return;

    QString themeName=action->data().toString();

    if(themeName==mytetraConfig.getInterfaceTheme())
        return;

    if(mytetraConfig.setInterfaceTheme(themeName))
    {
        CssHelper::loadCurrentTheme();

        // Своя бумага поверх темы переживает смену темы,
        // пустая настройка подхватывает цвет новой темы
        if(editorScreen!=nullptr)
            editorScreen->applyNoteColors();
    }
}


// Пункт «Настроить тему»: то же окно, что страница Themes
// в диалоге настроек. Пометка меню обновится при следующем
// открытии через aboutToShow


// Пункт «Настроить тему»: то же окно, что страница Themes
// в диалоге настроек. Пометка меню обновится при следующем
// открытии через aboutToShow
void MainWindow::onThemeConfigure(void)
{
    AppConfigDialog dialog("pageTheme", this);
    dialog.exec();
}


// Создание скрытых действий для работы прочих шорткатов уровня приложения


// Создание скрытых действий для работы прочих шорткатов уровня приложения
void MainWindow::initHiddenActions(void)
{
    // Действия добавляются в сам виджет
    // Для таких действий нет визуального представления, но горячие клавиши для них работают
    actionFocusTree = new QAction(this);
    this->addAction(actionFocusTree);

    actionFocusNoteTable = new QAction(this);
    this->addAction(actionFocusNoteTable);

    actionFocusEditor = new QAction(this);
    this->addAction(actionFocusEditor);
}


// Пересчитать и показать статистику базы в статусбаре.
// Вызывается по сигналам модели и точечно после правок

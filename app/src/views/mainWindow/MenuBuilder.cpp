#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QMap>
#include <QIcon>

#include "MenuBuilder.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/FixedParameters.h"
#include "libraries/ShortcutManager.h"
#include "libraries/helpers/DebugHelper.h"

extern AppConfig mytetraConfig;
extern FixedParameters fixedParameters;
extern ShortcutManager shortcutManager;


MenuBuilder::MenuBuilder(QMenuBar *menuBar, QObject *parent) : QObject(parent),
    m_menuBar(menuBar)
{

}


MenuBuilder::~MenuBuilder(void)
{

}


void MenuBuilder::buildAll(QAction *tagsToggle, QAction *historyToggle,
                           QAction *backlinksToggle, bool isDesktop)
{
    buildFileMenu();
    buildToolsMenu(tagsToggle, historyToggle, backlinksToggle, isDesktop);
    buildHelpMenu();
    buildThemesMenu();
    buildViewMenu();
}


QMenu *MenuBuilder::menu(MenuId id) const
{
    return m_menus.value(id, nullptr);
}


QAction *MenuBuilder::action(ActionId id) const
{
    return m_actions.value(id, nullptr);
}


void MenuBuilder::buildFileMenu(void)
{
    QMenu *fileMenu=new QMenu(tr("&File"), m_menuBar);
    m_menuBar->addMenu(fileMenu);
    m_menus[MenuId::File]=fileMenu;

    auto add=[&](ActionId id, const QString &text) -> QAction *
    {
        QAction *a=new QAction(text, this);
        fileMenu->addAction(a);
        m_actions[id]=a;
        return a;
    };

    add(ActionId::FileDatabases, tr("Databases management"));
    add(ActionId::FileExportTree, tr("Export tree item"));
    add(ActionId::FileImportTree, tr("Import tree item"));
    add(ActionId::FileImportTelegram, tr("Import Telegram chat..."));

    fileMenu->addSeparator();

    add(ActionId::FilePrint, tr("&Print..."));
    add(ActionId::FilePrintPreview, tr("Print Preview..."));
    add(ActionId::FileExportPdf, tr("&Export PDF..."));

    fileMenu->addSeparator();

    add(ActionId::FileQuit, tr("&Quit"));
}


void MenuBuilder::buildToolsMenu(QAction *tagsToggle, QAction *historyToggle,
                                 QAction *backlinksToggle, bool isDesktop)
{
    QMenu *toolsMenu=new QMenu(tr("&Tools"), m_menuBar);
    m_menuBar->addMenu(toolsMenu);
    m_menus[MenuId::Tools]=toolsMenu;

    // Find: текст выставляется в retranslate (как было: пустой конструктор)
    QAction *findAction=new QAction(this);
    toolsMenu->addAction(findAction);
    m_actions[ActionId::ToolsFindInBase]=findAction;

    auto add=[&](ActionId id, const QString &text) -> QAction *
    {
        QAction *a=new QAction(text, this);
        toolsMenu->addAction(a);
        m_actions[id]=a;
        return a;
    };

    add(ActionId::ToolsImagesGallery, tr("Images &gallery"));
    add(ActionId::ToolsFilesGallery, tr("Attached &files"));
    add(ActionId::ToolsActionLog, tr("Action &log"));

    QAction *clipAction=new QAction(this);
    toolsMenu->addAction(clipAction);
    m_actions[ActionId::ToolsClipFromClipboard]=clipAction;

    // Переключатели доков приходят готовыми от MainWindow (DI):
    // билдер их не ищет, только оформляет и кладет в меню
    tagsToggle->setIcon(QIcon(":/resource/pic/tag.svg"));
    toolsMenu->addAction(tagsToggle);
    shortcutManager.initAction("misc-tagsPanel", tagsToggle);

    historyToggle->setIcon(QIcon(":/resource/pic/history.svg"));
    toolsMenu->addAction(historyToggle);
    shortcutManager.initAction("misc-historyPanel", historyToggle);

    backlinksToggle->setIcon(QIcon(":/resource/pic/backlinks.svg"));
    toolsMenu->addAction(backlinksToggle);
    shortcutManager.initAction("misc-backlinksPanel", backlinksToggle);

    toolsMenu->addSeparator();

    if(isDesktop)
    {
        QAction *prefs=new QAction(tr("&Preferences"), this);
        toolsMenu->addAction(prefs);
        m_actions[ActionId::ToolsPreferences]=prefs;
    }
    else
    {
        // Создание подменю
        m_preferencesSubMenu=new QMenu(tr("&Preferences"), m_menuBar);
        buildPreferencesSubMenu(m_preferencesSubMenu);
        toolsMenu->addMenu(m_preferencesSubMenu);
    }
}


void MenuBuilder::buildPreferencesSubMenu(QMenu *menu)
{
    QMap<QString, ActionId> map;

    map[tr("Main")]       = ActionId::DirectMain;
    map[tr("Appearance")] = ActionId::DirectAppearance;
    map[tr("Crypt")]      = ActionId::DirectCrypt;
    map[tr("Syncro")]     = ActionId::DirectSyncro;
    map[tr("Notes Area")] = ActionId::DirectRecordTable;
    map[tr("Attaches")]   = ActionId::DirectAttach;
    map[tr("Keyboard")]   = ActionId::DirectKeyboard;
    map[tr("History")]    = ActionId::DirectHistory;
    map[tr("Misc")]       = ActionId::DirectMisc;

    for(auto pageName : map.keys())
    {
        QAction *a=new QAction(pageName, this);
        menu->addAction(a);
        m_actions[map[pageName]]=a;
    }
}


void MenuBuilder::buildHelpMenu(void)
{
    QMenu *helpMenu=new QMenu(tr("&Help"), m_menuBar);
    m_menuBar->addMenu(helpMenu);
    m_menus[MenuId::Help]=helpMenu;

    auto add=[&](ActionId id, const QString &text) -> QAction *
    {
        QAction *a=new QAction(text, this);
        helpMenu->addAction(a);
        m_actions[id]=a;
        return a;
    };

    add(ActionId::HelpAboutMyTetra, tr("About MyTetra"));
    add(ActionId::HelpAboutQt, tr("About Qt"));
    add(ActionId::HelpTechnicalInfo, tr("Technical info"));
}


void MenuBuilder::buildThemesMenu(void)
{
    QMenu *themesMenu=new QMenu(tr("&Themes"), m_menuBar);
    m_menuBar->addMenu(themesMenu);
    m_menus[MenuId::Themes]=themesMenu;

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
    QAction *configureAction=new QAction(tr("Configure theme..."), themesMenu);
    themesMenu->addAction(configureAction);
    m_actions[ActionId::ThemeConfigure]=configureAction;
    connect(configureAction, &QAction::triggered,
            this, &MenuBuilder::themeConfigureRequested);

    syncThemeMenu();

    connect(themeGroup, &QActionGroup::triggered,
            this, &MenuBuilder::themeChosen);

    connect(themesMenu, &QMenu::aboutToShow,
            this, &MenuBuilder::syncThemeMenu);
}


void MenuBuilder::syncThemeMenu(void)
{
    QMenu *themesMenu=m_menus.value(MenuId::Themes, nullptr);
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


void MenuBuilder::buildViewMenu(void)
{
    QMenu *viewMenu=new QMenu(tr("&View"), m_menuBar);
    m_menuBar->addMenu(viewMenu);
    m_menus[MenuId::View]=viewMenu;

    auto addCheck=[&](ActionId id, const QString &text) -> QAction *
    {
        QAction *a=new QAction(text, this);
        a->setCheckable(true);
        viewMenu->addAction(a);
        m_actions[id]=a;
        return a;
    };

    QAction *headerAction=addCheck(ActionId::ViewHeader, tr("Show note header"));
    QAction *favoritesAction=addCheck(ActionId::ViewFavorites, tr("Show favorites panel"));
    QAction *treeFilterAction=addCheck(ActionId::ViewTreeFilter, tr("Show branch filter"));

    QAction *toolsAction=addCheck(ActionId::ViewTools, tr("Show editor toolbar"));
    toolsAction->setObjectName(QStringLiteral("viewToolsAction"));

    QAction *readOnlyAction=new QAction(this);
    readOnlyAction->setCheckable(true);
    viewMenu->addAction(readOnlyAction);
    m_actions[ActionId::ViewReadOnly]=readOnlyAction;

    viewMenu->addSeparator();

    QAction *secretColorAction=new QAction(tr("Change secret color..."), this);
    viewMenu->addAction(secretColorAction);
    m_actions[ActionId::ViewSecretColor]=secretColorAction;

    QAction *paperColorAction=new QAction(tr("Change note background color..."), this);
    viewMenu->addAction(paperColorAction);
    m_actions[ActionId::ViewPaperColor]=paperColorAction;

    // Начальная пометка - делом MainWindow::syncViewMenu сразу после
    // buildAll (ему нужен editorScreen). Здесь только коннекты,
    // уходящие сигналами наружу: билдер не знает про MainWindow
    connect(headerAction, &QAction::toggled,
            this, &MenuBuilder::viewHeaderToggled);
    connect(favoritesAction, &QAction::toggled,
            this, &MenuBuilder::viewFavoritesToggled);
    connect(treeFilterAction, &QAction::toggled,
            this, &MenuBuilder::viewTreeFilterToggled);
    connect(toolsAction, &QAction::toggled,
            this, &MenuBuilder::viewToolsToggled);
    connect(readOnlyAction, &QAction::toggled,
            this, &MenuBuilder::viewReadOnlyToggled);
    connect(secretColorAction, &QAction::triggered,
            this, &MenuBuilder::secretColorRequested);
    connect(paperColorAction, &QAction::triggered,
            this, &MenuBuilder::paperColorRequested);

    connect(viewMenu, &QMenu::aboutToShow,
            this, &MenuBuilder::viewMenuShown);
}


void MenuBuilder::retranslate(void)
{
    // Зеркало MainWindow::retranslateUi (вызывается до
    // shortcutManager.retranslate()): прямой setText только там,
    // где его делал оригинал, иначе затрутся тексты на шорткатах
    QMenu *fileMenu=m_menus.value(MenuId::File, nullptr);
    if(fileMenu!=nullptr)
    {
        fileMenu->setTitle(tr("&File"));

        m_actions[ActionId::FileDatabases]->setText(tr("Databases management"));
        m_actions[ActionId::FileExportTree]->setText(tr("Export tree item"));
        m_actions[ActionId::FileImportTree]->setText(tr("Import tree item"));
        m_actions[ActionId::FileImportTelegram]->setText(tr("Import Telegram chat..."));
        m_actions[ActionId::FilePrintPreview]->setText(tr("Print Preview..."));
    }

    QMenu *toolsMenu=m_menus.value(MenuId::Tools, nullptr);
    if(toolsMenu!=nullptr)
    {
        toolsMenu->setTitle(tr("&Tools"));

        m_actions[ActionId::ToolsActionLog]->setText(tr("Action &log"));
        m_actions[ActionId::ToolsFilesGallery]->setText(tr("Attached &files"));

        if(m_actions.contains(ActionId::ToolsPreferences))
            m_actions[ActionId::ToolsPreferences]->setText(tr("&Preferences"));

        // Подменю Preferences (мобайл): ключи карты - переводы
        if(m_preferencesSubMenu!=nullptr)
        {
            m_preferencesSubMenu->setTitle(tr("&Preferences"));

            m_actions[ActionId::DirectMain]->setText(tr("Main"));
            m_actions[ActionId::DirectAppearance]->setText(tr("Appearance"));
            m_actions[ActionId::DirectCrypt]->setText(tr("Crypt"));
            m_actions[ActionId::DirectSyncro]->setText(tr("Syncro"));
            m_actions[ActionId::DirectRecordTable]->setText(tr("Notes Area"));
            m_actions[ActionId::DirectAttach]->setText(tr("Attaches"));
            m_actions[ActionId::DirectKeyboard]->setText(tr("Keyboard"));
            m_actions[ActionId::DirectHistory]->setText(tr("History"));
            m_actions[ActionId::DirectMisc]->setText(tr("Misc"));
        }
    }

    QMenu *themesMenu=m_menus.value(MenuId::Themes, nullptr);
    if(themesMenu!=nullptr)
    {
        themesMenu->setTitle(tr("&Themes"));

        QMap<QString, QString> translateNames;
        translateNames["default"]=tr("Default");
        translateNames["dark"]=tr("Dark");
        translateNames["modern"]=tr("Modern");
        translateNames["midnight"]=tr("Midnight");
        translateNames["custom"]=tr("Custom");

        for(QAction *themeAction : themesMenu->actions())
        {
            if(themeAction==nullptr || themeAction->isSeparator() ||
               themeAction->data().toString().isEmpty())
                continue;

            themeAction->setText(translateNames.value(themeAction->data().toString(), "Unknown"));
        }

        if(m_actions.contains(ActionId::ThemeConfigure))
            m_actions[ActionId::ThemeConfigure]->setText(tr("Configure theme..."));
    }

    QMenu *viewMenu=m_menus.value(MenuId::View, nullptr);
    if(viewMenu!=nullptr)
    {
        viewMenu->setTitle(tr("&View"));

        m_actions[ActionId::ViewHeader]->setText(tr("Show note header"));
        m_actions[ActionId::ViewFavorites]->setText(tr("Show favorites panel"));
        m_actions[ActionId::ViewTreeFilter]->setText(tr("Show branch filter"));
        m_actions[ActionId::ViewSecretColor]->setText(tr("Change secret color..."));
        m_actions[ActionId::ViewPaperColor]->setText(tr("Change note background color..."));

        // Пометки checkable-пунктов обновляет MainWindow::syncViewMenu
        // сразу после этого вызова (как было в retranslateUi)
    }

    QMenu *helpMenu=m_menus.value(MenuId::Help, nullptr);
    if(helpMenu!=nullptr)
    {
        helpMenu->setTitle(tr("&Help"));

        m_actions[ActionId::HelpAboutMyTetra]->setText(tr("About MyTetra"));
        m_actions[ActionId::HelpAboutQt]->setText(tr("About Qt"));
        m_actions[ActionId::HelpTechnicalInfo]->setText(tr("Technical info"));
    }
}

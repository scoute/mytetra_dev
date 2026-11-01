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

#include "main.h"
#include "models/appConfig/AppConfig.h"
#include "MainWindow.h"
#include "libraries/AppContext.h"
#include "TrayManager.h"
#include "MenuBuilder.h"
#include "McpGuiBridge.h"
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
#include "models/appConfig/AppConfigStorage.h"
#include "models/appConfig/AppConfigSync.h"
#include "models/appConfig/AppConfigServices.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigCrypto.h"
#include "models/appConfig/AppConfigUi.h"
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

MainWindow::MainWindow() : QMainWindow()
{
    // P3: корень для find_object - через AppContext, без файлового глобала.
    // Вызов первым делом: setupUI ниже уже ищет объекты от корня
    AppContext::instance().setMainWindow(this);
    setObjectName("mainwindow");

    installEventFilter(this);

    setupUI();

    // P3: меню строит MenuBuilder. DI: menubar для вставки,
    // тогглы доков и десктоп/мобайл режим параметрами
    menuBuilder=new MenuBuilder(menuBar(), this);
    menuBuilder->buildAll(tagsPanelDock->toggleViewAction(),
                          historyPanelDock->toggleViewAction(),
                          backlinksPanelDock->toggleViewAction(),
                          AppContext::instance().appConfig()->uiSection()->getInterfaceMode()==QStringLiteral("desktop"));

    // Начальная пометка пунктов Вид до коннектов: setChecked с тем же
    // значением toggled не дергает (как было внутри initViewMenu)
    syncViewMenu();

    // P3: мост MCP-сервер -> живой GUI. DI: модель из дерева, read-only
    // и mcp-флаг из конфига, сброс редактора как было через find_object
    mcpBridge=new McpGuiBridge(treeScreen->knowTreeModel, this);
    mcpBridge->isReadOnly=[]() { return AppContext::instance().appConfig()->recordTableSection()->isGlobalReadOnly(); };
    mcpBridge->mcpAllowed=[]() { return AppContext::instance().appConfig()->servicesSection()->get_mcpAllow(); };
    mcpBridge->setFavorites(AppContext::instance().favorites());
    mcpBridge->setBacklinkIndex(AppContext::instance().backlinkIndex());
    mcpBridge->clearEditor=[]() { find_object<MetaEditor>("editorScreen")->clearAll(); };
    connect(mcpBridge, &McpGuiBridge::sessionChanged, this, &MainWindow::updateMcpBanner);
    connect(mcpBridge, &McpGuiBridge::historyChanged, this, &MainWindow::updateMcpHistoryDialog);
    connect(mcpBridge, &McpGuiBridge::sessionStarted, this, &MainWindow::onMcpSessionStarted);
    connect(mcpBannerTimer, &QTimer::timeout, mcpBridge, &McpGuiBridge::checkPeers);

    initHiddenActions();
    setupSignals();
    assembly();

    setupShortcuts();

    // P3: трей - отдельный TrayManager с DI (хост - это окно как QWidget)
    trayManager=new TrayManager(this, this);
    connect(trayManager, &TrayManager::restoreRequested,
            this, &MainWindow::showWindow);
    connect(trayManager, &TrayManager::quitRequested,
            this, &MainWindow::applicationExit);
    trayManager->setupActions();
    trayManager->createTrayIcon();
    trayManager->setIcon();
    trayManager->show();

    // Инициализируется объект слежения за корзиной
    AppContext::instance().trashMonitoring()->init(AppContext::instance().appConfig()->storageSection()->get_trashdir());
    AppContext::instance().trashMonitoring()->update();

    // Старт веб-клиппера (перехват глобального хоткея по настройкам)
    clipper.start();

    // Применить сохранённый режим только чтения (если был включен)
    applyReadOnly();

    // Умное автосохранение: single-shot цепочка с интервалом из конфига
    autosaveTimer=new QTimer(this);
    autosaveTimer->setObjectName(QStringLiteral("autosaveTimer"));
    autosaveTimer->setSingleShot(true);
    connect(autosaveTimer, &QTimer::timeout, this, &MainWindow::onAutosaveTimeout);
    restartAutosaveTimer();

    // Закрывать ли по-настоящему окно при обнаружении сигнала closeEvent
    enableRealClose=false;

    // Инициализация генератора случайных чисел
    int x=this->mapFromGlobal(QCursor::pos()).x();
    int y=this->mapFromGlobal(QCursor::pos()).y();
    RandomInitter::init( static_cast<long>( x * y + (x % (y==0 ? 1 : y)) + x + y ) );
}

MainWindow::~MainWindow()
{
    saveAllState();

    // В лог записывается событие остановки программы
    AppContext::instance().actionLogger()->addAction("stopProgram");
}

void MainWindow::setupUI(void)
{
    // При создании объектов не указывается parent, так как он буден задан в момент вставки в layout в методе assembly()

    treeScreen=new TreeScreen(AppContext::instance().favorites(),
                                   AppContext::instance().backlinkIndex(),
                                   AppContext::instance().dispatcher(), this );
    treeScreen->setObjectName("treeScreen");
    AppContext::instance().globalParameters()->setTreeScreen(treeScreen);

    recordTableScreen=new RecordTableScreen(AppContext::instance().favorites(),
                                                       AppContext::instance().backlinkIndex(),
                                                       AppContext::instance().dispatcher(), this );
    recordTableScreen->setObjectName("recordTableScreen");
    AppContext::instance().globalParameters()->setRecordTableScreen(recordTableScreen);

    findScreenDisp=new FindScreen( this );
    findScreenDisp->setObjectName("findScreenDisp");
    AppContext::instance().globalParameters()->setFindScreen(findScreenDisp);

    editorScreen=new MetaEditor( this );
    editorScreen->setObjectName("editorScreen");
    editorScreen->showTextDispatcher=AppContext::instance().dispatcher();
    AppContext::instance().globalParameters()->setMetaEditor(editorScreen);

    statusBar=new QStatusBar( this );
    statusBar->setObjectName("statusBar");
    setStatusBar(statusBar);
    AppContext::instance().globalParameters()->setStatusBar(statusBar);

    // Статистика базы справа. Постоянный виджет: showMessage
    // его не затирает
    statsLabel=new QLabel( this );
    statsLabel->setObjectName("statsLabel");
    statsLabel->setCursor(Qt::PointingHandCursor);
    statsLabel->installEventFilter(this);
    statusBar->addPermanentWidget(statsLabel);
    updateStats();

    // Позиция курсора заметки слева. Обычный виджет: важные
    // showMessage-сообщения его перекрывают, потом он возвращается
    cursorLabel=new QLabel( this );
    cursorLabel->setObjectName("cursorLabel");
    statusBar->addWidget(cursorLabel);

    // Начальное значение: редактор и статусбар уже созданы
    if(editorScreen!=nullptr)
        editorScreen->updateCursorLabel();

    // Вспомогательный объект переключения окон, используется в мобильном интерфейсе
    windowSwitcher=new WindowSwitcher( this );
    windowSwitcher->setObjectName("windowSwitcher");
    AppContext::instance().globalParameters()->setWindowSwitcher(windowSwitcher);

    // Вспомогательный объект с виджетом синхронизации базы знаний
    syncroCommandRun=new CommandRunner( this );
    AppContext::instance().globalParameters()->setSyncroCommandRunner( syncroCommandRun );

    // Панель истории посещений в доке справа. По умолчанию скрыта,
    // переключается из меню Tools и горячей клавишей
    historyPanel=new HistoryPanel(this);
    historyPanel->setObjectName("historyPanel");

    historyPanelDock=new QDockWidget(tr("History"), this);
    historyPanelDock->setObjectName("historyPanelDock");
    historyPanelDock->setWidget(historyPanel);
    historyPanelDock->hide();

    // Панель входящих ссылок в доке справа. По умолчанию скрыта,
    // переключается из меню Tools
    backlinksPanel=new BacklinksPanel(AppContext::instance().backlinkIndex(), this);
    backlinksPanel->setObjectName("backlinksPanel");

    backlinksPanelDock=new QDockWidget(tr("Backlinks"), this);
    backlinksPanelDock->setObjectName("backlinksPanelDock");
    backlinksPanelDock->setWidget(backlinksPanel);
    backlinksPanelDock->hide();

    // Панель меток в доке справа. По умолчанию скрыта,
    // переключается из меню Tools и горячей клавишей
    tagsPanel=new TagsPanel(this);
    tagsPanel->setObjectName("tagsPanel");

    tagsPanelDock=new QDockWidget(tr("Tags"), this);
    tagsPanelDock->setObjectName("tagsPanelDock");
    tagsPanelDock->setWidget(tagsPanel);
    tagsPanelDock->hide();

    // todo: Для проверки, почему то в этом месте поиск объекта по имени не работает, разобраться.
    // MetaEditor *edView=find_object<MetaEditor>("editorScreen");
}

void MainWindow::setupSignals(void)
{
    connect(editorScreen, &MetaEditor::send_expand_edit_area, this, &MainWindow::onExpandEditArea);

    // Сигнал, генерирующийся при выходе из оконных систем X11 и Windows
    connect(qApp, &QApplication::commitDataRequest, this, &MainWindow::commitData);

    connect(qApp, &QApplication::focusChanged, this, &MainWindow::onFocusChanged);

    // Сигналы пунктов меню
    connect(menuBuilder->action(MenuBuilder::ActionId::FileDatabases), &QAction::triggered, this, &MainWindow::fileDatabasesManagement);
    connect(menuBuilder->action(MenuBuilder::ActionId::FileExportTree), &QAction::triggered, this, &MainWindow::fileExportBranch);
    connect(menuBuilder->action(MenuBuilder::ActionId::FileImportTree), &QAction::triggered, this, &MainWindow::fileImportBranch);
    connect(menuBuilder->action(MenuBuilder::ActionId::FileImportTelegram), &QAction::triggered, this, &MainWindow::fileImportTelegram);
    connect(menuBuilder->action(MenuBuilder::ActionId::FilePrint), &QAction::triggered, this, &MainWindow::filePrint);
    connect(menuBuilder->action(MenuBuilder::ActionId::FilePrintPreview), &QAction::triggered, this, &MainWindow::filePrintPreview);
    connect(menuBuilder->action(MenuBuilder::ActionId::FileExportPdf), &QAction::triggered, this, &MainWindow::filePrintPdf);
    connect(menuBuilder->action(MenuBuilder::ActionId::FileQuit), &QAction::triggered, this, &MainWindow::applicationExit);

    // Поиск по базе в стандартном меню
    connect(menuBuilder->action(MenuBuilder::ActionId::ToolsFindInBase), &QAction::triggered, this, &MainWindow::toolsFindInBase);
    connect(menuBuilder->action(MenuBuilder::ActionId::ToolsImagesGallery), &QAction::triggered, this, &MainWindow::toolsImagesGallery);
    connect(menuBuilder->action(MenuBuilder::ActionId::ToolsFilesGallery), &QAction::triggered, this, &MainWindow::toolsFilesGallery);
    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
    {
        // Кнопка поиска по базе в меню дерева разделов для мобильного интерфейса
        connect(treeScreen, &TreeScreen::treeScreenFindInBaseClicked, this, &MainWindow::toolsFindInBase);

        // Кнопка поиска по базе в меню редактора для мобильного интефейса
        connect(editorScreen, &MetaEditor::wyeditFindInBaseClicked, this, &MainWindow::toolsFindInBase);
    }

    // Мост из полоски поиска в заметке: запрос уходит в глобальный поиск.
    // Нужен всегда, а не только в мобильном режиме: у полоски есть кнопка
    connect(editorScreen, &MetaEditor::wyeditFindInBaseWithText, this, &MainWindow::toolsFindInBaseWithText);

    // Вызов окна просмотра лога
    connect(menuBuilder->action(MenuBuilder::ActionId::ToolsActionLog), &QAction::triggered, this, &MainWindow::onActionLogClicked);

    // Клиппер: заметка из буфера обмена в ветку unsorted_notes
    connect(menuBuilder->action(MenuBuilder::ActionId::ToolsClipFromClipboard), &QAction::triggered, this, &MainWindow::runClipperNow);

    // Вызов окна настроек
    if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="desktop")
    {
        connect(menuBuilder->action(MenuBuilder::ActionId::ToolsPreferences), &QAction::triggered, this, &MainWindow::toolsPreferences);
    }
    else
    {
        connect(menuBuilder->action(MenuBuilder::ActionId::DirectMain), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectMain)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectAppearance), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectAppearance)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectCrypt), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectCrypt)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectSyncro), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectSyncro)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectRecordTable), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectRecordTable)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectAttach), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectAttach)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectKeyboard), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectKeyboard)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectHistory), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectHistory)); } );

        connect(menuBuilder->action(MenuBuilder::ActionId::DirectMisc), &QAction::triggered,
                this, [this](){ runDirectPreferences(menuBuilder->action(MenuBuilder::ActionId::DirectMisc)); } );
    }

    connect(menuBuilder->action(MenuBuilder::ActionId::HelpAboutMyTetra), &QAction::triggered, this, &MainWindow::onClickHelpAboutMyTetra);
    connect(menuBuilder->action(MenuBuilder::ActionId::HelpAboutQt), &QAction::triggered, this, &MainWindow::onClickHelpAboutQt);
    connect(menuBuilder->action(MenuBuilder::ActionId::HelpTechnicalInfo), &QAction::triggered, this, &MainWindow::onClickHelpTechnicalInfo);

    // P3: тогглы вида и темы идут сигналами билдера (были внутри init*Menu)
    connect(menuBuilder, &MenuBuilder::viewHeaderToggled, this, &MainWindow::onViewHeaderToggled);
    connect(menuBuilder, &MenuBuilder::viewFavoritesToggled, this, &MainWindow::onViewFavoritesToggled);
    connect(menuBuilder, &MenuBuilder::viewTreeFilterToggled, this, &MainWindow::onViewTreeFilterToggled);
    connect(menuBuilder, &MenuBuilder::viewToolsToggled, this, &MainWindow::onViewToolsToggled);
    connect(menuBuilder, &MenuBuilder::viewHtmlSourceToggled, this, &MainWindow::onViewHtmlSourceToggled);
    connect(menuBuilder, &MenuBuilder::viewReadOnlyToggled, this, &MainWindow::onViewReadOnlyToggled);
    connect(menuBuilder, &MenuBuilder::secretColorRequested, this, &MainWindow::onViewSecretColor);
    connect(menuBuilder, &MenuBuilder::paperColorRequested, this, &MainWindow::onViewPaperColor);
    connect(menuBuilder, &MenuBuilder::themeChosen, this, &MainWindow::onThemeMenuTriggered);
    connect(menuBuilder, &MenuBuilder::themeConfigureRequested, this, &MainWindow::onThemeConfigure);
    connect(menuBuilder, &MenuBuilder::viewMenuShown, this, &MainWindow::syncViewMenu);

    connect(actionFocusTree, &QAction::triggered, this, &MainWindow::onClickFocusTree);
    connect(actionFocusNoteTable, &QAction::triggered, this, &MainWindow::onClickFocusNoteTable);
    connect(actionFocusEditor, &QAction::triggered, this, &MainWindow::onClickFocusEditor);

    // Связывание сигнала окончания выполнения команды синхронизации со слотом, срабатывающем при завершении выполнения команды
    connect(syncroCommandRun, &CommandRunner::finishWork,
            recordTableScreen, &RecordTableScreen::onSyncroCommandFinishWork);
    connect(syncroCommandRun, &CommandRunner::finishWork,
            this, &MainWindow::onSyncroCommandFinishWork);

    // Связывание сигнала вызова обработки открепляемых окон на предмет того,
    // что они отображают существующие записи
    connect(this, &MainWindow::doUpdateDetachedWindows,
            AppContext::instance().dispatcher(), &EditorShowTextDispatcher::closeWindowForNonExistentRecords,
            Qt::QueuedConnection);

    // Обновление горячих клавиш, если они были изменены
    connect(AppContext::instance().shortcutManager(), &ShortcutManager::updateWidgetShortcut, this, &MainWindow::setupShortcuts);

    // Статистика в статусбаре: ветки (структура) и записи
    // обновляются сигналами модели
    if(treeScreen!=nullptr && treeScreen->knowTreeModel!=nullptr)
    {
        KnowTreeModel *treeModel=treeScreen->knowTreeModel;

        connect(treeModel, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateStats);
        connect(treeModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateStats);
        connect(treeModel, &QAbstractItemModel::dataChanged, this, &MainWindow::updateStats);
        connect(treeModel, &QAbstractItemModel::modelReset, this, &MainWindow::updateStats);
        connect(treeModel, &QAbstractItemModel::layoutChanged, this, &MainWindow::updateStats);
    }
}

void MainWindow::assembly(void)
{
    vSplitter=new QSplitter(Qt::Vertical);
    vSplitter->addWidget(recordTableScreen); // Список конечных записей
    vSplitter->addWidget(editorScreen);      // Текст записи
    vSplitter->setCollapsible(0,false); // Список останавливается на одной записи (минимум вью), в ноль не уходит
    vSplitter->setCollapsible(1,false); // Содержимое записи не может смыкаться

    hSplitter=new QSplitter(Qt::Horizontal);
    hSplitter->addWidget(treeScreen); // Дерево веток
    hSplitter->addWidget(vSplitter);
    hSplitter->setCollapsible(0,false); // Дерево веток не может смыкаться
    hSplitter->setCollapsible(1,false); // Столбец со списком и содержимым записи не может смыкаться

    findSplitter=new QSplitter(Qt::Vertical);
    findSplitter->addWidget(hSplitter);
    findSplitter->addWidget(findScreenDisp);
    findSplitter->setCollapsible(0,false); // Верхняя часть не должна смыкаться
    findSplitter->setCollapsible(1,false); // Часть для поиска не должна смыкаться
    findSplitter->setObjectName("findsplitter");

    // Порядок доков сверху вниз: теги, история, обратные ссылки
    addDockWidget(Qt::RightDockWidgetArea, tagsPanelDock);
    addDockWidget(Qt::RightDockWidgetArea, historyPanelDock);
    addDockWidget(Qt::RightDockWidgetArea, backlinksPanelDock);

    // Кнопки-бирок во второй линии записей: те же переключатели
    // что в меню Tools, состояние синхронно само
    recordTableScreen->addExtraToolAction(tagsPanelDock->toggleViewAction());
    recordTableScreen->addExtraToolAction(historyPanelDock->toggleViewAction());
    recordTableScreen->addExtraToolAction(backlinksPanelDock->toggleViewAction());

    // Полоса MCP-сессии: сверху, крупно, немодально, без кнопок.
    // Инлайн-стиль вместо темы: красный виден и в светлой, и в тёмной.
    // Скрыть нельзя: висит всё время пока подключён хоть один MCP-пир
    mcpBannerBar=new QWidget(this);
    mcpBannerBar->setObjectName("mcpBannerBar");
    mcpBannerBar->setStyleSheet("QWidget { background-color: #B00020; }");

    QHBoxLayout *bannerLayout=new QHBoxLayout(mcpBannerBar);
    bannerLayout->setContentsMargins(8, 6, 8, 6);

    mcpBanner=new QLabel(mcpBannerBar);
    mcpBanner->setObjectName("mcpBanner");
    mcpBanner->setStyleSheet("QLabel { background-color: #B00020; color: #FFFFFF; border: none; }");
    mcpBanner->setAlignment(Qt::AlignCenter);
    mcpBanner->setWordWrap(true);
    QFont bannerFont=mcpBanner->font();
    bannerFont.setPointSize(bannerFont.pointSize()+6);
    bannerFont.setBold(true);
    mcpBanner->setFont(bannerFont);
    bannerLayout->addWidget(mcpBanner, 1);

    mcpBannerBar->hide();

    // Окно истории действий сессии: создаётся сразу, показывается
    // на старт сессии, на дисконнект скрывается и чистится
    mcpHistoryDialog=new McpHistoryDialog(this);
    mcpHistoryDialog->hide();
    mcpBanner->installEventFilter(this);

    // Минутный пинг пиров: мертвые процессы выкидываются,
    // живые остаются независимо от пауз нейронки
    mcpBannerTimer=new QTimer(this);
    mcpBannerTimer->start(60000);

    QWidget *centralContainer=new QWidget(this);
    QVBoxLayout *centralLayout=new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(mcpBannerBar);
    centralLayout->addWidget(findSplitter);

    setCentralWidget(centralContainer);

    // Обратный индекс ссылок: грузится из сайдкара data/backlinks.xml,
    // при первом запуске строится полным сканом базы
    AppContext::instance().backlinkIndex()->loadOrBuild();
}

void MainWindow::saveAllState(void)
{
    // Сохранение данных в поле редактирования
    saveTextarea();

    // Сохраняются данные сеанса работы
    saveWindowGeometry();
    saveTreePosition();
    saveRecordTablePosition();
    saveEditorCursorPosition();
    saveEditorScrollBarPosition();

    // Синхронизируется с диском конфиг программы
    AppContext::instance().appConfig()->sync();
}

// Слот, срабатывающий когда происходит выход из оконной системы

// Слот, срабатывающий когда происходит выход из оконной системы
void MainWindow::commitData(QSessionManager& manager)
{
    Q_UNUSED(manager)
    qDebug() << "Session manager send commit data signal.";

    applicationFastExit();
}

// Слот, обрабатывающий внешние сообщения
// Используется для обработки ссобщений, приходящих от других экземпляров MyTetra
// Разбор аргумента MCP-команд: весь текст после первого пробела это
// компактный JSON (пробелы внутри допустимы, переводов строк нет)

void MainWindow::messageHandler(QString message)
{
    qDebug() << "MainWindow recieved message: "+message;

    // Сообщение разбивается на команду и ее аргумент до обращений по индексу.
    // Раньше цепочка if/else if была оборвана (ветки show и hide висели
    // отдельно), а message.split(" ").at(1) вызывался до проверки наличия
    // аргумента и падал при сообщении без параметра
    QStringList messageParts=message.split(" ");

    if(messageParts.isEmpty())
        return;

    QString command=messageParts.at(0);

    // Значение аргумента команды, если он был передан
    QString commandArgument;
    if(messageParts.count()>1)
        commandArgument=messageParts.at(1);

    // MCP выключен в настройках: все внешние mcp-команды игнорируются.
    // Отдельный if до цепочки чтобы не рвать else if
    if(command.startsWith("mcp") && !AppContext::instance().appConfig()->servicesSection()->get_mcpAllow())
        return;

    if(command=="show")
    {
        showWindow();
    }

    else if(command=="hide")
    {
        hide();
    }

    else if(command=="quit")
    {
        applicationExit();
    }

    else if(command=="reload")
    {
        reload();
    }

    else if(command=="openNote")
    {
        // Если аргумент опции --openNote не обнаружен
        if(commandArgument.isEmpty())
            return;

        // Нахождение ветки, в которой лежит запись с указанным идентификатором
        QStringList pathToRecord=treeScreen->knowTreeModel->getRecordPath(commandArgument);

        this->setTreeAndRecordtablePositions(pathToRecord, commandArgument);
    }

    else if(command=="addNoteDialog")
    {
        // Глобальный режим только чтения: создание записей запрещено
        if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
            return;
        // Определение, было ли окно MyTetra скрыто при обработке сообщения
        bool isHidden=this->isHidden();

        // Окно показыается, чтобы небыло некорректного выхода QtSingleApplication при свернутом окне после добавления записи
        if(isHidden)
        {
            this->showWindow();
        }

        // Нажимается кнопка добавления записи
        if( recordTableScreen->actionAddNewToEnd->isEnabled() )
        {
            recordTableScreen->actionAddNewToEnd->trigger();
        }
        else
        {
            qDebug() << "Can not initial add new note dialog. Add new note action is disable now. Try again.";
        }

        // Окно скрывается
        if(isHidden)
        {
            this->hide();
        }
    }

    else if(command=="openTreeItem")
    {
        // Если аргумент опции --openTreeItem (устаревшее --openBranch) не обнаружен
        if(commandArgument.isEmpty())
            return;

        // Установка курсора в дереве
        treeScreen->setCursorToId( commandArgument );
    }

    else if(command=="clipboard")
    {
        // Глобальный режим только чтения: клиппер не создаёт записи
        if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
            return;

        // Клиппер: заметка из буфера обмена в ветку unsorted_notes.
        // Аргумент команды это ссылка на источник, может отсутствовать
        clipper.clipNowWithUrl(commandArgument);
    }

    // MCP-команды записи от внешнего AI-клиента: применяются к живой
    // модели (со всем несохраненным состоянием GUI) и сохраняются.
    // Основной путь - синхронный респондер (handleMcpMessage ->
    // handleMcpCommand с возвратом итога). Ниже легаси-путь на случай
    // незаданного респондера: тот же обработчик, итог игнорируется.
    // При коллизии id вставка не делается чтобы MCP не вернул чужой id
    // (проверка сама перегенерирует и повторит)
    else if(command=="mcpCreateBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpCreateNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpUpdateNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Удаление из-под AI: тот же состав что ручное удаление записи,
    // минус диалог подтверждения (его заменяет флаг режима сервера).
    // Редактор намеренно не чистится: его буфер доживает до перевыбора,
    // как при любом стороннем изменении файла
    else if(command=="mcpDeleteNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpDeleteBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpRenameBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpMoveBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpMoveNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Звездочка от внешнего AI: тот же состав что клик по звезде,
    // минус вопросы. Идёт через синглетон: он сохраняет сам
    // (сайдкар или XML) и дёргает сигнал для панели избранного
    else if(command=="mcpFavoriteNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpUnfavoriteNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Ввоз ветки из обменника от внешнего AI: проверки те же что
    // на сервере (плохой обменник вместо модального диалога -
    // тихий отказ). Ввоз сам сохраняет дерево в конце
    else if(command=="mcpImportBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpRenameTag")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpDeleteTag")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Пакетное применение от внешнего AI: те же applyXxx что
    // одиночки (через общий диспетчер), но сейв и вспышка один раз
    // на весь пакет. Побочки одиночек повторяются: clearAll редактора
    // перед каждым moveNote, закрытие открепленных окон собирается
    // и шлется разово. Стоп на первой ошибке: пакет неатомарен,
    // примененное сохраняется. import/export в пакет не входят
    else if(command=="mcpApplyBatch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Присутствие MCP: не правки, только Hello/Bye. В read-only тоже
    // работает — баннер предупреждает, ничего не меняя
    else if(command=="mcpHello")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpBye")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Строка истории действий MCP: только накопление в памяти,
    // на диск ничего не пишется
    else if(command=="mcpHistory")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        mcpBridge->handleMcpCommand(command, McpGuiBridge::parseMcpCommandArgs(message), dummyResult, dummyError);
    }
}

// Синхронный ответ на mcp-команду для протокола запрос-ответ.
// Упаковка итога handleMcpCommand в компактный JSON

// Пересчитать и показать статистику базы в статусбаре.
// Вызывается по сигналам модели и точечно после правок
void MainWindow::updateStats(void)
{
    if(statsLabel==nullptr)
        return;

    int branches=0;
    int records=0;
    int files=0;
    int images=0;
    int links=0;
    int tags=0;

    if(treeScreen!=nullptr && treeScreen->knowTreeModel!=nullptr)
        treeScreen->knowTreeModel->getBaseStats(branches, records, files, images, links, tags);

    statsLabel->setText(tr("Branches: %1 | Records: %2 | Tags: %3 | Files: %4 | Images: %5 | Internal links: %6").arg(branches).arg(records).arg(tags).arg(files).arg(images).arg(links));
    statsLabel->setToolTip(tr("Base statistics: branches, records, tags, files, images and links. Links are mytetra:// note links. Click opens per-note details."));
}

// Внешняя MCP-правка: баннер и так висит всю сессию, здесь только
// пересчёт (текст с режимами, видимость, блокировка)

// Перезапуск цепочки умного автосохранения с текущим конфигом
void MainWindow::restartAutosaveTimer(void)
{
    if(autosaveTimer==nullptr)
        return;

    const int minutes=AppContext::instance().appConfig()->syncSection()->get_autosaveInterval();

    if(!AppContext::instance().appConfig()->syncSection()->get_autosaveEnable() || minutes<1)
    {
        autosaveTimer->stop();
        return;
    }

    autosaveTimer->start(minutes*60*1000);
}

// Срабатывание умного автосохранения: сохраняется только реально
// правленная запись (флаг редактора), а снапшот истории дедуплицирует
// по тексту. Без правок ни файла ни коммита. Открытый модальный диалог
// круг пропускает: иначе сохранился бы промежуточный предпросмотр

// Срабатывание умного автосохранения: сохраняется только реально
// правленная запись (флаг редактора), а снапшот истории дедуплицирует
// по тексту. Без правок ни файла ни коммита. Открытый модальный диалог
// круг пропускает: иначе сохранился бы промежуточный предпросмотр
void MainWindow::onAutosaveTimeout(void)
{
    if(editorScreen!=nullptr && editorScreen->getTextareaModified() &&
       QApplication::activeModalWidget()==nullptr)
        saveTextarea();

    restartAutosaveTimer();
}

// Блокировка UI на время MCP-сессии поверх глобального read-only.
// Своё досохраняется до блокировки, иначе потеряется на навигации.
// MCP-записи смотрят сырой флаг и продолжают писать.
// Идемпотентна: повтор не дёргает сохранение и перераскладку

// Разбивка статистики по заметкам: кто сколько ссылок держит.
// Двойной клик прыгает к заметке
void MainWindow::showStatsDetails(void)
{
    if(treeScreen==nullptr || treeScreen->knowTreeModel==nullptr)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Statistics"));
    dialog.resize(640, 400);

    QTableWidget *table=buildStatsTableWidget(&dialog);

    connect(table, &QTableWidget::cellDoubleClicked, this, [this, table](int row, int)
    {
        QTableWidgetItem *branchItem=table->item(row, 0);

        if(branchItem==nullptr)
            return;

        QString recordId=branchItem->data(Qt::UserRole).toString();

        if(recordId.isEmpty() || treeScreen==nullptr || treeScreen->knowTreeModel==nullptr)
            return;

        QStringList treePath=treeScreen->knowTreeModel->getRecordPath(recordId);

        if(treePath.isEmpty())
            return;

        setTreeAndRecordtablePositions(treePath, recordId);
    });

    QVBoxLayout *layout=new QVBoxLayout(&dialog);
    layout->addWidget(table);

    dialog.exec();
}

void MainWindow::setupShortcuts(void)
{
    qDebug() << "Setup shortcut for" << this->objectName();

    // Ctrl+P открывает предпросмотр печати (там же кнопка прямой печати).
    // Прямая печать без предпросмотра висит на misc-printDirect
    AppContext::instance().shortcutManager()->initAction("misc-print", menuBuilder->action(MenuBuilder::ActionId::FilePrintPreview) );
    AppContext::instance().shortcutManager()->initAction("misc-printDirect", menuBuilder->action(MenuBuilder::ActionId::FilePrint) );
    AppContext::instance().shortcutManager()->initAction("misc-exportPdf", menuBuilder->action(MenuBuilder::ActionId::FileExportPdf) );
    AppContext::instance().shortcutManager()->initAction("misc-quit", menuBuilder->action(MenuBuilder::ActionId::FileQuit) );

    AppContext::instance().shortcutManager()->initAction("misc-findInBase", menuBuilder->action(MenuBuilder::ActionId::ToolsFindInBase) );

    AppContext::instance().shortcutManager()->initAction("misc-imagesGallery", menuBuilder->action(MenuBuilder::ActionId::ToolsImagesGallery) );

    AppContext::instance().shortcutManager()->initAction("misc-clipFromClipboard", menuBuilder->action(MenuBuilder::ActionId::ToolsClipFromClipboard) );

    AppContext::instance().shortcutManager()->initAction("misc-readOnly", menuBuilder->action(MenuBuilder::ActionId::ViewReadOnly) );

    AppContext::instance().shortcutManager()->initAction("misc-focusTree", actionFocusTree );
    AppContext::instance().shortcutManager()->initAction("misc-focusNoteTable", actionFocusNoteTable );
    AppContext::instance().shortcutManager()->initAction("misc-focusEditor", actionFocusEditor );
}

// Перевести статичные тексты новым языком. Вызывается до
// AppContext::instance().shortcutManager()->retranslate(): тексты на шорткатах освежит сигнал,
// а прямой setText их бы затер

// Перевести статичные тексты новым языком. Вызывается до
// AppContext::instance().shortcutManager()->retranslate(): тексты на шорткатах освежит сигнал,
// а прямой setText их бы затер
void MainWindow::retranslateUi(void)
{
    // P3: меню и пункты переводит билдер (зеркало старого кода).
    // Вызывается до AppContext::instance().shortcutManager()->retranslate() как раньше
    menuBuilder->retranslate();

    // Переключатели доков истории и меток: текст тоже идет через шорткаты,
    // но initAction для них звался один раз при построении (как было)
    AppContext::instance().shortcutManager()->initAction("misc-historyPanel", historyPanelDock->toggleViewAction());
    AppContext::instance().shortcutManager()->initAction("misc-tagsPanel", tagsPanelDock->toggleViewAction());

    // Пометки checkable-пунктов Вид (было в конце view-блока retranslateUi)
    syncViewMenu();

    // P3: пункты трея переводит сам менеджер
    trayManager->retranslate();

    historyPanelDock->setWindowTitle(tr("History"));

    tagsPanelDock->setWindowTitle(tr("Tags"));

    // Дочерние экраны со статичными текстами
    if(treeScreen!=nullptr)
        treeScreen->retranslateUi();

    if(recordTableScreen!=nullptr)
        recordTableScreen->retranslateUi();

    if(editorScreen!=nullptr)
        editorScreen->retranslateUi();

    FindScreen *findScreen=find_object<FindScreen>("findScreenDisp");

    if(findScreen!=nullptr)
        findScreen->retranslateUi();

    if(historyPanel!=nullptr)
        historyPanel->retranslateUi();

    if(tagsPanel!=nullptr)
        tagsPanel->retranslateUi();

    FavoritesPanel *favoritesPanel=find_object<FavoritesPanel>("favoritesPanel");

    if(favoritesPanel!=nullptr)
        favoritesPanel->retranslateUi();

    BacklinksPanel *backlinksPanel=find_object<BacklinksPanel>("backlinksPanel");

    if(backlinksPanel!=nullptr)
        backlinksPanel->retranslateUi();

    // Видимый баннер MCP — тоже
    if(mcpBanner!=nullptr && mcpBanner->isVisible())
        mcpBanner->setText(mcpBridge->bannerText());

    if(mcpHistoryDialog!=nullptr)
        mcpHistoryDialog->retranslateUi();
}

// Сохранить текущую статью

// Сохранить текущую статью
bool MainWindow::fileSave(void)
{
    // todo: сделать функционал

    return true;
}

// Сохранить текущую статью как файл

// Сохранить текущую статью как файл
bool MainWindow::fileSaveAs(void)
{
    // todo: сделать функционал
    return true;
}

// Напечатать текущую статью

// Напечатать текущую статью
void MainWindow::filePrint(void)
{
#ifndef QT_NO_PRINTER
    QPrinter printer(QPrinter::HighResolution);
    printer.setFullPage(true);

    QPrintDialog *dlg = new QPrintDialog(&printer, this);
    dlg->setWindowTitle(tr("Print Document"));

    if (dlg->exec() == QDialog::Accepted)
        editorScreen->getTextareaDocument()->print(&printer);

    delete dlg;
#endif
}

// Предпросмотр печати текущей статьи

// Предпросмотр печати текущей статьи
void MainWindow::filePrintPreview(void)
{
    PrintPreview *preview = new PrintPreview(editorScreen->getTextareaDocument(), this);

    preview->setWindowModality(Qt::WindowModal);
    preview->setAttribute(Qt::WA_DeleteOnClose);
    preview->show();
}

// Вывод текущей статьи в PDF файл

// Вывод текущей статьи в PDF файл
void MainWindow::filePrintPdf(void)
{
#ifndef QT_NO_PRINTER
    QString fileName = QFileDialog::getSaveFileName(this, "Export PDF",
                                                    QString(), "*.pdf");
    if (!fileName.isEmpty())
    {
        if(QFileInfo(fileName).suffix().isEmpty())
            fileName.append(".pdf");

        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        editorScreen->getTextareaDocument()->print(&printer);
    }
#endif
}

// Раздел меню File, управление базами данных

// Раздел меню File, управление базами данных
void MainWindow::fileDatabasesManagement(void)
{
    DatabasesManagementScreen databasesManagementScreen(AppContext::instance().backlinkIndex(), this);
    databasesManagementScreen.exec();
}

// Раздел меню File, экспорт текущей ветки и всех ее подветок

// Раздел меню File, экспорт текущей ветки и всех ее подветок
void MainWindow::fileExportBranch(void)
{
    // Создается окно выбора директории назначения
    QFileDialog directorySelectDialog(this);
    directorySelectDialog.setFileMode(QFileDialog::Directory);
    directorySelectDialog.setWindowTitle(tr("Select an empty directory to export data"));
    directorySelectDialog.setDirectory("/");

    if( directorySelectDialog.exec() )
        if( !directorySelectDialog.directory().absolutePath().isEmpty() ) // Если была выбрана директория (а не нажат Cancel)
            treeScreen->exportBranchToDirectory( directorySelectDialog.directory().absolutePath() );
}

void MainWindow::fileImportTelegram(void)
{
    // Глобальный режим только чтения: импорт меняет базу
    if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
        return;

    TelegramImportDialog dialog(this);
    dialog.exec();
}

void MainWindow::fileImportBranch(void)
{
    // Глобальный режим только чтения: импорт меняет базу
    if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
        return;

    // Создается окно выбора директории, откуда необходимо сделать импорт
    QFileDialog directorySelectDialog(this);
    directorySelectDialog.setFileMode(QFileDialog::Directory);
    directorySelectDialog.setWindowTitle(tr("Select directory to import data"));
    directorySelectDialog.setDirectory("/");

    if( directorySelectDialog.exec() )
        if( !directorySelectDialog.directory().absolutePath().isEmpty() ) // Если была выбрана директория (а не нажат Cancel)
            treeScreen->importBranchFromDirectory( directorySelectDialog.directory().absolutePath() );
}

// Слот - Нормальный выход из программы

// Слот - Нормальный выход из программы
void MainWindow::applicationExit(void)
{
    saveAllState();

    // Если происходит первая инициализация выхода из программы
    // И если в конфиге настроено, что нужно синхронизироваться при выходе
    // И задана команда синхронизации
    if(enableRealClose==false)
        if(AppContext::instance().appConfig()->syncSection()->get_synchroonexit())
            if(AppContext::instance().appConfig()->syncSection()->get_synchrocommand().trimmed().length()>0)
            {
                enableRealClose=true;
                synchronization(); // В конце синхронизации будет вызван слот onSyncroCommandFinishWork()
            }

    // Запуск выхода из программы
    enableRealClose=true;
    emit close();
}

// Быстрый выход из программы, без возможности синхронизации

// Быстрый выход из программы, без возможности синхронизации
void MainWindow::applicationFastExit(void)
{
    saveAllState();

    // Запуск выхода из программы
    enableRealClose=true;
    emit close();
}

void MainWindow::toolsFindInBase(void)
{
    // Определяется ссылка на виджет поиска
    FindScreen *findScreenRel=find_object<FindScreen>("findScreenDisp");

    if( !(findScreenRel->isVisible()) )
        findScreenRel->widgetShow();
    else
        findScreenRel->widgetHide();
}

// Уход из полоски поиска в заметке в глобальный поиск с текстом запроса

// Уход из полоски поиска в заметке в глобальный поиск с текстом запроса
void MainWindow::toolsFindInBaseWithText(const QString &text)
{
    FindScreen *findScreenRel=find_object<FindScreen>("findScreenDisp");

    if( !(findScreenRel->isVisible()) )
        findScreenRel->widgetShow();

    findScreenRel->setFindText(text);
}

void MainWindow::toolsImagesGallery(void)
{
    ContentGallery *gallery=new ContentGallery(ContentGallery::GalleryMode::Images, this);
    gallery->setAttribute(Qt::WA_DeleteOnClose);
    gallery->show();
}

void MainWindow::toolsFilesGallery(void)
{
    ContentGallery *gallery=new ContentGallery(ContentGallery::GalleryMode::Attaches, this);
    gallery->setAttribute(Qt::WA_DeleteOnClose);
    gallery->show();
}

void MainWindow::toolsPreferences(void)
{
    AppConfigDialog dialog("", this); // this нужен чтобы пробрасывать иконку приложения
    dialog.exec();

    // Настройки могли поменять хоткей клиппера — перечитать
    clipper.rereadSettings();
}

void MainWindow::onActionLogClicked()
{
    ActionLogScreen actionLogScreen(this);
    actionLogScreen.exec();
}

// Показ всплывающего сообщения в системном трее.
// Используется фоновыми задачами (клиппер), которым некого спросить,
// но надо сообщить пользователю о результате

// Клиппер из меню Tools: вставка из буфера в unsorted_notes сейчас.
// Ссылка на источник ищется в самом тексте буфера
void MainWindow::runClipperNow(void)
{
    clipper.clipNow();
}

Clipper *MainWindow::getClipper(void)
{
    return &clipper;
}

// Слот, срабатывающий при нажатии в редакторе кнопки "распахнуть"
// Флаг показывает, что нужно сделать
// true - распахнуть область отводимую редактору
// false - сделать область, отводимую редактору, обычной

// Слот, срабатывающий при нажатии в редакторе кнопки "распахнуть"
// Флаг показывает, что нужно сделать
// true - распахнуть область отводимую редактору
// false - сделать область, отводимую редактору, обычной
void MainWindow::onExpandEditArea(bool flag)
{
    if(flag)
    {
        AppContext::instance().globalParameters()->getTreeScreen()->hide();
        AppContext::instance().globalParameters()->getRecordTableScreen()->hide();
    }
    else
    {
        AppContext::instance().globalParameters()->getTreeScreen()->show();
        AppContext::instance().globalParameters()->getRecordTableScreen()->show();
    }
}

void MainWindow::onClickHelpAboutMyTetra(void)
{
    QString version=QString::number(APPLICATION_RELEASE_VERSION)+"."+QString::number(APPLICATION_RELEASE_SUBVERSION)+"."+QString::number(APPLICATION_RELEASE_MICROVERSION);

    QString infoProgramName;
    QString infoVersion;
    QString infoAuthor;
    QString infoEmail;
    QString infoLicense;

    infoProgramName="<b>MyTetra</b> - smart manager<br/>for information collecting<br/><br/>";
    infoVersion="v."+version+"<br/><br/>";
    infoAuthor="Author: Sergey M. Stepanov<br/>";
    infoEmail="Author Email:<i>xintrea@gmail.com</i><br/><br/>";
    infoLicense="GNU General Public License v.3.0<br/><br/>";

    QString info=infoProgramName+
            infoVersion+
            infoAuthor+
            infoEmail+
            infoLicense;

    QMessageBox::about(this,
                       "MyTetra v."+version,
                       info);
}

void MainWindow::onClickHelpAboutQt(void)
{
    QMessageBox::aboutQt(this);
}

void MainWindow::onClickHelpTechnicalInfo(void)
{
    QString infoTargetOs;
    QString infoProgramFile;
    QString infoWorkDirectory;
    QString infoDevicePixelRatio;
    QString infoPhysicalDpi;
    QString infoPhysicalDpiX;
    QString infoPhysicalDpiY;

    infoTargetOs="Target OS: "+AppContext::instance().globalParameters()->getTargetOs()+"<br/>";
    infoProgramFile="Program file: "+AppContext::instance().globalParameters()->getMainProgramFile()+"<br/>";
    infoWorkDirectory="Work directory: "+AppContext::instance().globalParameters()->getWorkDirectory()+"<br/>";

#if QT_VERSION >= 0x050000 && QT_VERSION < 0x060000
    infoDevicePixelRatio="Device pixel ratio: "+(QString::number( qApp->devicePixelRatio(), 'f', 2 ))+"<br/>";
    infoPhysicalDpi="Physical DPI (from screen): "+(QString::number( QApplication::screens().at(0)->physicalDotsPerInch(), 'f', 2 ))+"<br/>";
#endif

    infoPhysicalDpiX="Physical DPI X (from desktop): "+(QString::number( qApp->desktop()->physicalDpiX(), 'f', 2 ))+"<br/>";
    infoPhysicalDpiY="Physical DPI Y (from desktop): "+(QString::number( qApp->desktop()->physicalDpiY(), 'f', 2 ))+"<br/>";

    QString info=infoTargetOs+
            infoProgramFile+
            infoWorkDirectory+
            infoDevicePixelRatio+
            infoPhysicalDpi+
            infoPhysicalDpiX+
            infoPhysicalDpiY;

    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setIcon( QMessageBox::Information );
    msgBox->setWindowTitle(tr("Technical info"));
    msgBox->setText(tr("<b>Technical info</b>"));
    msgBox->setInformativeText(info);
    msgBox->exec();
}

void MainWindow::onClickFocusTree(void)
{
    treeScreen->setFocusToBaseWidget();
}

void MainWindow::onClickFocusNoteTable(void)
{
    recordTableScreen->setFocusToBaseWidget();
}

void MainWindow::onClickFocusEditor(void)
{
    editorScreen->setFocusToBaseWidget();
}

// Перечитывание всей базы знаний

// Перечитывание всей базы знаний
void MainWindow::reload(void)
{
  reloadSaveStage();

  // Стадия Load идет сразу после стадии Save, флаг о наличии задержки не выставляется
  reloadLoadStage(false);
}

// Перечитывание дерева знаний, стадия Save

// Перечитывание дерева знаний, стадия Save
void MainWindow::reloadSaveStage(void)
{
  // Сохраняются данные в поле редактирования
  saveTextarea();

  // Сохраняются данные о курсорах в дереве и таблице конечных записей
  saveTreePosition();
  saveRecordTablePosition();
  saveEditorCursorPosition();
  saveEditorScrollBarPosition();
}

// Перечитывание дерева знаний, стадия Load

// Перечитывание дерева знаний, стадия Load
void MainWindow::reloadLoadStage(bool isLongTimeReload)
{
    // Блокируется история
    AppContext::instance().walkHistory()->setDrop(true);

    // Стадия Load может запуститься сильно позже по времени после стадии Save.
    // И пользователь можейт уйти с записи, которую он редактировал на стадии Save.
    // Поэтому для такого случая перед перечитыванием модели нужно обязательно
    // сохранить положение пользователя в дереве, чтобы восстановить именно
    // актуальное состояние после перечитывания модели
    if( isLongTimeReload )
    {
        saveTreePosition();
        saveRecordTablePosition();
        saveEditorCursorPosition();
        saveEditorScrollBarPosition();
    }

    // Если дерево успешно перечитано и обнаружено, что оно было изменено
    // (При этом модель будет полностью пересоздана, все индексы будут новые)
    if( treeScreen->reloadKnowTree() )
    {
        // Чтобы при дальнейшей работе небыло сегфолтов, так как индексы изменились,
        // нужно восстановить положение пользователя в дереве знаний
        restoreTreePosition();
        restoreRecordTablePosition();
        restoreEditorCursorPosition();
        restoreEditorScrollBarPosition();
    }

    // Разблокируется история посещений элементов
    AppContext::instance().walkHistory()->setDrop(false);
}

// Старт синхронизации

// Старт синхронизации
void MainWindow::synchronization(bool visible)
{
    // Глобальный режим только чтения: синхронизация может менять базу
    if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
        return;

    // Если кнопка синхронизации заблокирована, начинать синхронизацию нельзя
    if(!recordTableScreen->isSyncroEnabled())
        return;

    // Блокируется кнопка синхронизации, чтобы два раза случайно не нажать синхронизацию (окно синхронизации не модально)
    recordTableScreen->setSyncroEnabled(false);

    AppContext::instance().actionLogger()->addAction("startSyncro");

    reloadSaveStage();

    // Считывается команда синхронизации
    QString command=AppContext::instance().appConfig()->syncSection()->get_synchrocommand();

    // Если команда синхронизации пуста
    if(command.trimmed().length()==0)
    {
        QMessageBox::warning(this,
                             tr("MyTetra: can't synchronization"),
                             tr("Do not set synchronization command.<br>Check the setting in \"Sync\" section in \"Tools\" menu"),
                             QMessageBox::Close);

        // Кнопка синхронизации разблокируется, чтобы ее можно было снова нажать
        recordTableScreen->setSyncroEnabled(true);

        return;
    }

    // Макрос %a заменяется на путь к директории базы данных
    // QString databasePath=AppContext::instance().globalParameters()->getWorkDirectory()+"/"+AppContext::instance().appConfig()->storageSection()->get_tetradir();
    QDir databaseDir( AppContext::instance().appConfig()->storageSection()->get_tetradir() );
    QString databasePath=databaseDir.canonicalPath();

    command.replace("%a", databasePath);

    // Запуск команды синхронизации
    auto runner = AppContext::instance().globalParameters()->getSyncroCommandRunnner();
    runner->setWindowTitle(tr("MyTetra synchronization"));
    runner->setMessageText(tr("Synchronization in progress, please wait..."));
    runner->setCommand(command);
    runner->run(visible);
}

// Завершение синхронизации

// Завершение синхронизации
void MainWindow::onSyncroCommandFinishWork()
{
    // Функция перечитывания дерева знаний вызывается с флагом,
    // что от предыдущей стадии была большая задержка
    reloadLoadStage(true);

    // После того, как дерево перечитано, содержимое открепляемых окон обновляется,
    // так как содержимое записей могло поменяться (т. к. получены новые изменения)
    AppContext::instance().dispatcher()->updateAllWindows();

    // Возможно, что после обновления имеются открепляемые окна, показывающие
    // записи, которые были удалены при синхронизации. Такие окна должны быть закрыты.
    // Проверка и закрытие таких окон делается в отдельном потоке, потому вызов
    // делается через сингнал-слот
    emit doUpdateDetachedWindows();

    AppContext::instance().actionLogger()->addAction("stopSyncro");

    // В конце синхронизации нужно проверить, не происходит ли выход из программы
    if(enableRealClose==true)
    {
        this->applicationExit();
    }
}

// Слот закрытия окна
void MainWindow::closeEvent(QCloseEvent *event)
{
    if(enableRealClose==false)
    {
        if(QSystemTrayIcon::isSystemTrayAvailable()==false)
        {
            return;
        }

        // При приходе события закрыть окно, событие игнорируется
        // и окно просто делается невидимым. Это нужно чтобы при закрытии окна
        // программа не завершала работу
        if(trayManager->isVisible())
        {
            hide();
            event->ignore();
            return;
        }
    }

    // Здесь код оказывается если далее должно однозначно происходить закрытие программы

    // Так как закрытие программы может происходить не с первого раза, например при
    // настройке синхронизации при закрытии программы, то действия
    // которые должны выполняться при закрытии должны срабатывать только единожды
    if(exitCounter==0)
    {
        // Запоминается список и состояния открепляемых окон,
        // данное действие нельзя делать в saveAllState(), вызываемое из деструктора, так как если
        // все открепляемые окна не будут закрыты (см. следующею команду), то деструктор
        // главного окна не будет вызываться объектом приложения
        AppContext::instance().dispatcher()->saveOpenWindows();

        // Закрытие открепляемых окон, даже если главное окно не является родителем для
        // открепляемых окон, чтобы не осталось "висячих" открепляемых окон, которые
        // не будут давать закрыться приложению
        AppContext::instance().dispatcher()->closeAllWindowsForExit();

        exitCounter++;
    }

    event->accept();
}

bool MainWindow::eventFilter( QObject *o, QEvent * e )
{
    // Q_UNUSED(o);
    // qDebug() << "Event: " << e->type();

    // Отлавливание потери фокуса
    // QEvent::ActivationChange
    if(o==this && e->type() == QEvent::WindowDeactivate)
    {
        qDebug() << "Main window focus deactivate, save all state.";
        saveAllState();
    }

    // Клик по баннеру MCP открывает окно истории сессии
    if(o==mcpBanner && e->type()==QEvent::MouseButtonRelease)
    {
        if(mcpHistoryDialog!=nullptr)
        {
            mcpHistoryDialog->show();
            mcpHistoryDialog->raise();
        }

        return true;
    }

    // Клик по строке статистики открывает разбивку по заметкам
    if(o==statsLabel && e->type()==QEvent::MouseButtonRelease)
    {
        QMouseEvent *mouseEvent=static_cast<QMouseEvent *>(e);

        if(mouseEvent->button()==Qt::LeftButton)
        {
            showStatsDetails();
            return true;
        }
    }

    // Продолжать оработку событий дальше
    // return false;
    return QMainWindow::eventFilter(o, e);
}    

// Обработчик отлавливает нажатия клавиш

// Обработчик отлавливает нажатия клавиш
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Проброс нажатия клавиши Ctrl в редактор, даже если он не активный (не в фокусе)
    if( event->key() == Qt::Key_Control )
        if(editorScreen!=nullptr)
            emit globalPressKey(Qt::Key_Control);

    QMainWindow::keyPressEvent(event);
}

// Обработчик отлавливает отжатия клавиш

// Обработчик отлавливает отжатия клавиш
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // Если отжата клавиша Ctrl
    if( event->key() == Qt::Key_Control )
        if(editorScreen!=nullptr)
            emit globalReleaseKey(Qt::Key_Control);

    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::goWalkHistoryPrevious(void)
{
    editorScreen->saveTextarea();

    // Идентификатор текущей записи запоминается в историю
    QString id=editorScreen->getMiscField("id");
    AppContext::instance().walkHistory()->add(id,
                    editorScreen->getCursorPosition(),
                    editorScreen->getScrollBarPosition(),
                    WALK_HISTORY_GO_PREVIOUS);

    goWalkHistory();
}

void MainWindow::goWalkHistoryNext(void)
{
    editorScreen->saveTextarea();

    // Идентификатор текущей записи запоминается в историю
    QString id=editorScreen->getMiscField("id");
    AppContext::instance().walkHistory()->add(id,
                    editorScreen->getCursorPosition(),
                    editorScreen->getScrollBarPosition(),
                    WALK_HISTORY_GO_NEXT);

    goWalkHistory();
}

void MainWindow::goWalkHistory(void)
{
    // Выясняется идентификатор записи, на которую надо переключиться
    QString id=AppContext::instance().walkHistory()->getId();

    if(id.length()==0)
        return;

    // Выясняется путь к ветке, где находится данная запись
    QStringList path=treeScreen->knowTreeModel->getRecordPath(id);

    // Проверяем, есть ли такая ветка
    if(treeScreen->knowTreeModel->isItemValid(path)==false)
        return;

    // Выясняется позицию записи в таблице конечных записей
    TreeItem *item =treeScreen->knowTreeModel->getItem(path);

    // Проверяем, есть ли такая позиция
    if(item->recordtableGetTableData()->isRecordExists(id)==false)
        return;

    // Установка позиции в дереве и в таблице конечных записей
    this->setTreeAndRecordtablePositions(path, id);

    if(AppContext::instance().appConfig()->cryptoSection()->getRememberCursorAtHistoryNavigation())
    {
        editorScreen->setCursorPosition( AppContext::instance().walkHistory()->getCursorPosition(id) );
        editorScreen->setScrollBarPosition( AppContext::instance().walkHistory()->getScrollBarPosition(id) );
    }
}

// Метод, вызываемый из всех точек интерфейса, в которых происходит
// переход к другой записи. Метод вызывается до перехода, чтобы сохранить
// текст редактируемой записи

// Метод, вызываемый из всех точек интерфейса, в которых происходит
// переход к другой записи. Метод вызывается до перехода, чтобы сохранить
// текст редактируемой записи
void MainWindow::saveTextarea(void)
{
    // Глобальный режим только чтения
    if(AppContext::instance().appConfig()->recordTableSection()->get_readOnly())
        return;

    QString id=editorScreen->getMiscField("id");

    qDebug() << "MainWindow::saveTextarea() : id :" << id;

    // Текст правили или только ходили по веткам: индекс и статистика
    // нужны лишь в первом случае, иначе это полный рескан базы
    // на каждый клик (тысячи чтений text.html с диска)
    const bool modified=editorScreen->getTextareaModified();

    editorScreen->saveTextarea();

    AppContext::instance().walkHistory()->add(id,
                    editorScreen->getCursorPosition(),
                    editorScreen->getScrollBarPosition());

    if(!modified)
      return;

    // Текст на диске обновился: перепарсить исходящие ссылки записи
    if(!id.isEmpty())
      AppContext::instance().backlinkIndex()->updateSource(id);

    // Текст мог принести/убрать картинки и ссылки: одна строка
    // в кеше без полного прохода, подпись из него же
    if(treeScreen!=nullptr && treeScreen->knowTreeModel!=nullptr && !id.isEmpty())
      treeScreen->knowTreeModel->refreshRecordStats(id);

    updateStats();
}

// Слот, обрабатывающий смену фокуса на виджетах

// Слот, обрабатывающий смену фокуса на виджетах
void MainWindow::onFocusChanged(QWidget *widgetFrom, QWidget *widgetTo)
{
    Q_UNUSED (widgetFrom)

    if(widgetTo==nullptr)
        return;

    qDebug() << "MainWindow::onFocusChanged() to " << widgetTo->objectName();

    return; // Временно ничего не делает
}

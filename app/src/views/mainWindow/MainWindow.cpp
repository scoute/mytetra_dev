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


MainWindow::MainWindow() : QMainWindow()
{
    extern QObject *pMainWindow;
    pMainWindow=this;
    setObjectName("mainwindow");

    installEventFilter(this);

    setupUI();
    initFileMenu();
    initToolsMenu();
    initViewMenu();
    initThemesMenu();
    initHelpMenu();
    initHiddenActions();
    setupSignals();
    assembly();

    setupShortcuts();

    setupIconActions();
    createTrayIcon();
    setIcon();
    if(QSystemTrayIcon::isSystemTrayAvailable())
        trayIcon->show();

    // Инициализируется объект слежения за корзиной
    trashMonitoring.init(mytetraConfig.get_trashdir());
    trashMonitoring.update();

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
    actionLogger.addAction("stopProgram");
}


void MainWindow::setupUI(void)
{
    // При создании объектов не указывается parent, так как он буден задан в момент вставки в layout в методе assembly()

    treeScreen=new TreeScreen( this );
    treeScreen->setObjectName("treeScreen");
    globalParameters.setTreeScreen(treeScreen);

    recordTableScreen=new RecordTableScreen( this );
    recordTableScreen->setObjectName("recordTableScreen");
    globalParameters.setRecordTableScreen(recordTableScreen);

    findScreenDisp=new FindScreen( this );
    findScreenDisp->setObjectName("findScreenDisp");
    globalParameters.setFindScreen(findScreenDisp);

    editorScreen=new MetaEditor( this );
    editorScreen->setObjectName("editorScreen");
    globalParameters.setMetaEditor(editorScreen);

    statusBar=new QStatusBar( this );
    statusBar->setObjectName("statusBar");
    setStatusBar(statusBar);
    globalParameters.setStatusBar(statusBar);

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
    globalParameters.setWindowSwitcher(windowSwitcher);

    // Вспомогательный объект с виджетом синхронизации базы знаний
    syncroCommandRun=new CommandRunner( this );
    globalParameters.setSyncroCommandRunner( syncroCommandRun );

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
    backlinksPanel=new BacklinksPanel(this);
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
    connect(actionFileMenuDatabasesManagement, &QAction::triggered, this, &MainWindow::fileDatabasesManagement);
    connect(actionFileMenuExportTreeItem, &QAction::triggered, this, &MainWindow::fileExportBranch);
    connect(actionFileMenuImportTreeItem, &QAction::triggered, this, &MainWindow::fileImportBranch);
    connect(actionFileMenuImportTelegram, &QAction::triggered, this, &MainWindow::fileImportTelegram);
    connect(actionFileMenuPrint, &QAction::triggered, this, &MainWindow::filePrint);
    connect(actionFileMenuPrintPreview, &QAction::triggered, this, &MainWindow::filePrintPreview);
    connect(actionFileMenuExportPdf, &QAction::triggered, this, &MainWindow::filePrintPdf);
    connect(actionFileMenuQuit, &QAction::triggered, this, &MainWindow::applicationExit);


    // Поиск по базе в стандартном меню
    connect(actionToolsMenuFindInBase, &QAction::triggered, this, &MainWindow::toolsFindInBase);
    connect(actionToolsMenuImagesGallery, &QAction::triggered, this, &MainWindow::toolsImagesGallery);
    connect(actionToolsMenuFilesGallery, &QAction::triggered, this, &MainWindow::toolsFilesGallery);
    if(mytetraConfig.getInterfaceMode()=="mobile")
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
    connect(actionToolsMenuActionLog, &QAction::triggered, this, &MainWindow::onActionLogClicked);

    // Клиппер: заметка из буфера обмена в ветку unsorted_notes
    connect(actionToolsMenuClipFromClipboard, &QAction::triggered, this, &MainWindow::runClipperNow);

    // Вызов окна настроек
    if(mytetraConfig.getInterfaceMode()=="desktop")
    {
        connect(actionToolsMenuPreferences, &QAction::triggered, this, &MainWindow::toolsPreferences);
    }
    else
    {
        connect(actionDirectPreferencesMain, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesMain); } );

        connect(actionDirectPreferencesAppearance, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesAppearance); } );

        connect(actionDirectPreferencesCrypt, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesCrypt); } );

        connect(actionDirectPreferencesSyncro, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesSyncro); } );

        connect(actionDirectPreferencesRecordTable, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesRecordTable); } );

        connect(actionDirectPreferencesAttach, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesAttach); } );

        connect(actionDirectPreferencesKeyboard, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesKeyboard); } );

        connect(actionDirectPreferencesHistory, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesHistory); } );

        connect(actionDirectPreferencesMisc, &QAction::triggered,
                this, [this](){ runDirectPreferences(actionDirectPreferencesMisc); } );
    }

    connect(actionHelpMenuAboutMyTetra, &QAction::triggered, this, &MainWindow::onClickHelpAboutMyTetra);
    connect(actionHelpMenuAboutQt, &QAction::triggered, this, &MainWindow::onClickHelpAboutQt);
    connect(actionHelpMenuTechnicalInfo, &QAction::triggered, this, &MainWindow::onClickHelpTechnicalInfo);

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
            EditorShowTextDispatcher::instance(), &EditorShowTextDispatcher::closeWindowForNonExistentRecords,
            Qt::QueuedConnection);

    // Обновление горячих клавиш, если они были изменены
    connect(&shortcutManager, &ShortcutManager::updateWidgetShortcut, this, &MainWindow::setupShortcuts);

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
    vSplitter->setCollapsible(0,true); // Список записей сворачивается в ноль
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
    connect(mcpBannerTimer, &QTimer::timeout, this, &MainWindow::onMcpPeerTick);
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
    BacklinkIndex::instance().loadOrBuild();
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
    mytetraConfig.sync();
}


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
static QJsonObject parseMcpCommandArgs(const QString &message)
{
  int spacePos=message.indexOf(' ');

  if(spacePos<0)
    return QJsonObject();

  QJsonParseError parseError;
  QJsonDocument doc=QJsonDocument::fromJson(message.mid(spacePos+1).toUtf8(),
                                            &parseError);

  if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
    return QJsonObject();

  return doc.object();
}


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
    if(command.startsWith("mcp") && !mytetraConfig.get_mcpAllow())
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
        if(mytetraConfig.get_readOnly())
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
        if(mytetraConfig.get_readOnly())
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
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpCreateNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpUpdateNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
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
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpDeleteBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpRenameBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpMoveBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpMoveNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Звездочка от внешнего AI: тот же состав что клик по звезде,
    // минус вопросы. Идёт через синглетон: он сохраняет сам
    // (сайдкар или XML) и дёргает сигнал для панели избранного
    else if(command=="mcpFavoriteNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpUnfavoriteNote")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Ввоз ветки из обменника от внешнего AI: проверки те же что
    // на сервере (плохой обменник вместо модального диалога -
    // тихий отказ). Ввоз сам сохраняет дерево в конце
    else if(command=="mcpImportBranch")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpRenameTag")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpDeleteTag")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
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
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Присутствие MCP: не правки, только Hello/Bye. В read-only тоже
    // работает — баннер предупреждает, ничего не меняя
    else if(command=="mcpHello")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    else if(command=="mcpBye")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }

    // Строка истории действий MCP: только накопление в памяти,
    // на диск ничего не пишется
    else if(command=="mcpHistory")
    {
        // Легаси-путь (респондер не задан): тот же обработчик, итог игнорируется
        QJsonObject dummyResult;
        QString dummyError;
        handleMcpCommand(command, parseMcpCommandArgs(message), dummyResult, dummyError);
    }
}


// Синхронный ответ на mcp-команду для протокола запрос-ответ.
// Упаковка итога handleMcpCommand в компактный JSON
QByteArray MainWindow::handleMcpMessage(const QString &message)
{
  QStringList messageParts=message.split(" ");

  if(messageParts.isEmpty())
    return QJsonDocument(QJsonObject{{"ok", false},
                                     {"error", "Empty message"}}).toJson(QJsonDocument::Compact);

  const QString command=messageParts.at(0);

  if(!mytetraConfig.get_mcpAllow())
    return QJsonDocument(QJsonObject{{"ok", false},
                                     {"error", "MCP is disabled in settings"}}).toJson(QJsonDocument::Compact);

  QJsonObject args=parseMcpCommandArgs(message);
  QJsonObject result;
  QString error;

  if(!handleMcpCommand(command, args, result, error))
    return QJsonDocument(QJsonObject{{"ok", false},
                                     {"error", error.isEmpty() ? "Command failed" : error}})
      .toJson(QJsonDocument::Compact);

  result["ok"]=true;

  return QJsonDocument(result).toJson(QJsonDocument::Compact);
}


// Применение одной mcp-команды к живой модели с возвратом итога.
// Тела веток - те же applyXxx что были в messageHandler, но тихие
// return заменены на текст ошибки: сервер отдаёт её агенту как есть,
// вместо гадания "did not apply... retry"
bool MainWindow::handleMcpCommand(const QString &command,
                                  const QJsonObject &args,
                                  QJsonObject &result,
                                  QString &error)
{
  result=QJsonObject();
  error=QString();

  if(command=="mcpCreateBranch")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject created=McpTools::applyCreateBranch(treeScreen->knowTreeModel,
                                                    args.value("parent_id").toString(),
                                                    args.value("id").toString(),
                                                    args.value("name").toString(),
                                                    error);

    if(!error.isEmpty())
      return false;

    if(created.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result=created;
    return true;
  }

  else if(command=="mcpCreateNote")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject created=McpTools::applyCreateNote(treeScreen->knowTreeModel,
                                                  args.value("branch_id").toString(),
                                                  args.value("id").toString(),
                                                  args.value("name").toString(),
                                                  args.value("html").toString(),
                                                  args.value("tags").toString(),
                                                  error);

    if(!error.isEmpty())
      return false;

    if(created.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result=created;
    return true;
  }

  else if(command=="mcpUpdateNote")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QStringList updatedFields;

    // Открытый в редакторе текст не трогается: редактор держит свой
    // буфер до перевыбора (last writer wins), как при любом
    // стороннем изменении файла
    const bool ok=McpTools::applyUpdateNote(treeScreen->knowTreeModel,
                                            args.value("id").toString(),
                                            args.value("has_name").toBool(false),
                                            args.value("name").toString(),
                                            args.value("has_html").toBool(false),
                                            args.value("html").toString(),
                                            args.value("has_tags").toBool(false),
                                            args.value("tags").toString(),
                                            error,
                                            updatedFields);

    if(!ok)
    {
      if(error.isEmpty())
        error="Note was not updated";

      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result["id"]=args.value("id").toString();

    QJsonArray fields;
    foreach(const QString &field, updatedFields)
      fields.append(field);
    result["updated_fields"]=fields;

    return true;
  }

  else if(command=="mcpDeleteNote")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(id.isEmpty())
    {
      error="Empty id";
      return false;
    }

    if(!McpTools::applyDeleteNote(treeScreen->knowTreeModel, id, error))
    {
      if(error.isEmpty())
        error="Note was not deleted";

      return false;
    }

    emit treeScreen->knowTreeModel->doCloseDetachedWindowByIdSet(QSet<QString>() << id);

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result["id"]=id;
    return true;
  }

  else if(command=="mcpDeleteBranch")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(id.isEmpty())
    {
      error="Empty id";
      return false;
    }

    QJsonObject deleted=McpTools::applyDeleteBranch(treeScreen->knowTreeModel,
                                                    id, error);

    if(!error.isEmpty())
      return false;

    if(deleted.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result=deleted;
    return true;
  }

  else if(command=="mcpRenameBranch")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject renamed=McpTools::applyRenameBranch(treeScreen->knowTreeModel,
                                                    args.value("id").toString(),
                                                    args.value("name").toString(),
                                                    error);

    if(!error.isEmpty())
      return false;

    if(renamed.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result=renamed;
    return true;
  }

  else if(command=="mcpMoveBranch")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonObject moved=McpTools::applyMoveBranch(treeScreen->knowTreeModel,
                                                args.value("id").toString(),
                                                args.value("target_id").toString(),
                                                args.value("as_child").toBool(true),
                                                error);

    if(!error.isEmpty())
      return false;

    if(moved.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result=moved;
    return true;
  }

  else if(command=="mcpMoveNote")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    // Как при удалении: редактор может держать указатель внутрь
    // таблицы-источника, его состояние сбрасывается до переноса
    find_object<MetaEditor>("editorScreen")->clearAll();

    QJsonObject moved=McpTools::applyMoveNote(treeScreen->knowTreeModel,
                                              args.value("id").toString(),
                                              args.value("branch_id").toString(),
                                              error);

    if(!error.isEmpty())
      return false;

    if(moved.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result=moved;
    return true;
  }

  else if(command=="mcpFavoriteNote")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(!treeScreen->knowTreeModel->isRecordIdExists(id))
    {
      error="Unknown note_id: "+id;
      return false;
    }

    Record *record=treeScreen->knowTreeModel->getRecord(id);

    if(record==nullptr || record->getField("crypt")=="1")
    {
      error="Note is not available";
      return false;
    }

    Favorites::instance().setFavorite(id, true);

    if(!Favorites::instance().isFavorite(id))
    {
      error="Note was not starred";
      return false;
    }

    flashMcpWrite();

    result["id"]=id;
    result["favorite"]=true;
    return true;
  }

  else if(command=="mcpUnfavoriteNote")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString id=args.value("id").toString();

    if(!treeScreen->knowTreeModel->isRecordIdExists(id))
    {
      error="Unknown note_id: "+id;
      return false;
    }

    Favorites::instance().setFavorite(id, false);

    if(Favorites::instance().isFavorite(id))
    {
      error="Note is still starred";
      return false;
    }

    flashMcpWrite();

    result["id"]=id;
    result["favorite"]=false;
    return true;
  }

  else if(command=="mcpImportBranch")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    const QString parentId=args.value("parent_id").toString();
    const QString path=args.value("path").toString();

    if(!parentId.isEmpty())
    {
      TreeItem *parentItem=treeScreen->knowTreeModel->getItemById(parentId);

      if(parentItem==nullptr || parentItem->getField("crypt")=="1")
      {
        error="Parent branch is not available";
        return false;
      }
    }

    // Плохой обменник вместо модального диалога - отказ с причиной
    const QString dirError=McpTools::checkImportDir(path);

    if(!dirError.isEmpty())
    {
      error=dirError;
      return false;
    }

    QJsonObject imported=McpTools::applyImportBranch(treeScreen->knowTreeModel,
                                                     parentId, path, error);

    if(!error.isEmpty())
      return false;

    if(imported.isEmpty())
    {
      error="Internal error: empty result";
      return false;
    }

    flashMcpWrite();

    result=imported;
    return true;
  }

  else if(command=="mcpRenameTag")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    int renamed=0;
    int skipped=0;

    if(!McpTools::applyRenameTag(treeScreen->knowTreeModel,
                                 args.value("old").toString(),
                                 args.value("new").toString(),
                                 error, renamed, skipped))
    {
      if(error.isEmpty())
        error="Tag was not renamed";

      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result["renamed"]=renamed;
    result["skipped"]=skipped;
    return true;
  }

  else if(command=="mcpDeleteTag")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    int cleared=0;
    int skipped=0;

    if(!McpTools::applyDeleteTag(treeScreen->knowTreeModel,
                                 args.value("tag").toString(),
                                 error, cleared, skipped))
    {
      if(error.isEmpty())
        error="Tag was not deleted";

      return false;
    }

    treeScreen->knowTreeModel->save();
    flashMcpWrite();

    result["cleared"]=cleared;
    result["skipped"]=skipped;
    return true;
  }

  else if(command=="mcpApplyBatch")
  {
    if(mytetraConfig.isGlobalReadOnly())
    {
      error="Database is in global read-only mode";
      return false;
    }

    if(args.isEmpty())
    {
      error="Broken command arguments";
      return false;
    }

    QJsonArray operations=args.value("operations").toArray();

    if(operations.isEmpty())
    {
      error="Empty operations list";
      return false;
    }

    QSet<QString> closeWindows;
    bool needSave=false;
    QJsonArray opResults;

    for(const QJsonValue &opValue : operations)
    {
      const QJsonObject op=opValue.toObject();
      const QString tool=op.value("tool").toString();
      const QJsonObject opArgs=op.value("arguments").toObject();

      // import/export в пакет не входят
      if(tool==QStringLiteral("import_branch") ||
         tool==QStringLiteral("export_branch"))
      {
        error="Unsupported in batch: "+tool;
        break;
      }

      // Как в одиночке: редактор может держать указатель внутрь
      // таблицы-источника, его состояние сбрасывается до переноса
      if(tool==QStringLiteral("move_note"))
        find_object<MetaEditor>("editorScreen")->clearAll();

      // Как в одиночке: открепленные окна удаляемой записи
      if(tool==QStringLiteral("delete_note"))
        closeWindows.insert(opArgs.value("note_id").toString());

      QJsonObject dummy;

      if(!McpTools::applyBatchOp(treeScreen->knowTreeModel,
                                 tool, opArgs, dummy, error))
        break;

      QJsonObject opResult;
      opResult["tool"]=tool;
      opResult["ok"]=true;
      opResults.append(opResult);

      // Избранное сохраняет само (синглетон/сайдкар), остальное
      // требует сейва модели
      if(tool!=QStringLiteral("favorite_note") &&
         tool!=QStringLiteral("unfavorite_note"))
        needSave=true;
    }

    if(!closeWindows.isEmpty())
      emit treeScreen->knowTreeModel->doCloseDetachedWindowByIdSet(closeWindows);

    if(needSave)
      treeScreen->knowTreeModel->save();

    flashMcpWrite();

    // Стоп на первой ошибке: пакет неатомарен, примененное сохраняется
    result["applied"]=opResults.size();
    result["total"]=operations.size();
    result["operations"]=opResults;

    if(!error.isEmpty() || opResults.size()!=operations.size())
    {
      if(error.isEmpty())
        error="Batch stopped before all operations";

      return false;
    }

    return true;
  }

  else if(command=="mcpHello")
  {
    const QString pid=args.value("pid").toString();

    if(pid.isEmpty())
    {
      error="Empty pid";
      return false;
    }

    // Старт сессии с нуля: история чистая, окно появляется само
    const bool sessionStarts=mcpPeers.isEmpty();

    mcpPeers[pid]=QDateTime::currentMSecsSinceEpoch();
    mcpPeerModes[pid]=args.value("mode").toString();
    updateMcpBanner();

    if(sessionStarts)
    {
      mcpHistory.clear();
      updateMcpHistoryDialog();

      if(mcpHistoryDialog!=nullptr)
        mcpHistoryDialog->show();
    }

    return true;
  }

  else if(command=="mcpBye")
  {
    const QString pid=args.value("pid").toString();

    if(pid.isEmpty())
    {
      error="Empty pid";
      return false;
    }

    mcpPeers.remove(pid);
    mcpPeerModes.remove(pid);
    updateMcpBanner();

    return true;
  }

  else if(command=="mcpHistory")
  {
    addMcpHistoryEntry(args.value("kind").toString(),
                       args.value("tool").toString(),
                       args.value("summary").toString());

    return true;
  }

  error="Unknown command: "+command;
  return false;
}


// Восстанавливается геометрия окна и позиции основных разделителей
void MainWindow::restoreWindowGeometry(void)
{
    if(globalParameters.getTargetOs()=="android")
        setWindowState(Qt::WindowMaximized); // Для Андроида окно просто разворачивается на весь экран
    else
        restoreGeometry( mytetraConfig.get_mainwingeometry() );

    // Размеры применяются только при совпадении числа виджетов:
    // в режиме "список под деревом" у vSplitter один виджет, списки
    // из другого режима сюда не подходят
    if(mytetraConfig.get_vspl_size_list().size()==vSplitter->count())
        vSplitter->setSizes(mytetraConfig.get_vspl_size_list());

    if(mytetraConfig.get_hspl_size_list().size()==hSplitter->count())
        hSplitter->setSizes(mytetraConfig.get_hspl_size_list());

    findSplitter->setSizes(mytetraConfig.get_findsplitter_size_list());
}


// Перестановка списка записей: справа от дерева (дефолт) либо под
// деревом в левой колонке. Вызывается при старте после восстановления
// геометрии и вживую из настроек. Существующие высоты виджетов
// сохраняются как есть, поэтому переключение туда-обратно не прыгает
void MainWindow::applyRecordTablePosition(void)
{
    const bool under=mytetraConfig.get_recordTableUnderTree();
    const bool isUnder=(leftColumnSplitter!=nullptr);

    if(under==isUnder)
        return;

    if(under)
    {
        // Список записей переезжает из правой колонки под дерево.
        // vSplitter остаётся с одним редактором.
        // Важно: только addWidget/insertWidget, никакого replaceWidget -
        // после него сплиттер навсегда игнорирует setSizes (стенд).
        // addWidget сам вытаскивает виджет из старого сплиттера
        leftColumnSplitter=new QSplitter(Qt::Vertical, this);
        leftColumnSplitter->setObjectName("leftColumnSplitter");

        leftColumnSplitter->addWidget(treeScreen);
        leftColumnSplitter->addWidget(recordTableScreen);
        hSplitter->insertWidget(0, leftColumnSplitter);

        leftColumnSplitter->setCollapsible(0, false);
        leftColumnSplitter->setCollapsible(1, false);

        // Размеры - после показа: до show() раздавать некому.
        // Живьём окно уже показано - ставим сразу, на холодном старте
        // ждём показа через синглшот-цепочку со сходимостью
        if(isVisible())
            applyRecordTableSizes();
        else
        {
            recordTableSizesRetry=0;
            QTimer::singleShot(0, this, &MainWindow::applyRecordTableSizes);
        }
    }
    else
    {
        // Список записей возвращается в правую колонку над редактором.
        // Порядок важен: сначала записи в vSplitter, потом дерево в
        // начало hSplitter. Без replaceWidget, по той же причине
        vSplitter->insertWidget(0, recordTableScreen);
        hSplitter->insertWidget(0, treeScreen);
        vSplitter->setCollapsible(0, true);
        vSplitter->setCollapsible(1, false);

        // Синхронное удаление: виджеты уже переехали, детей нет.
        // deleteLater оставлял бы пустой сплиттер болтаться до
        // следующего прохода событийной петли
        delete leftColumnSplitter;
        leftColumnSplitter=nullptr;

        // vSplitter c одним редактором хранил одно число: вернуть
        // обе панели к осмысленным долям текущей высоты.
        // Назад переключаются только живьём (окно показано),
        // поэтому immediate setSizes здесь работает
        const int total=vSplitter->height();

        if(total>0)
            vSplitter->setSizes(QList<int>() << total/3 << total-total/3);
    }

    recordTableScreen->toolsUpdate();
}


// Отложенное применение размеров после перестановки списка записей.
// Вызывается через singleShot(0): setSizes сразу после replaceWidget
// сплиттер игнорирует, а до show() раздавать некому. К моменту вызова
// окно уже показано, размеры живые. Идемпотентно: при повторных
// вызовах и смене режима туда-сюда пересчитывает текущее состояние,
// сохранённые списки имеют приоритет
void MainWindow::applyRecordTableSizes(void)
{
    if(leftColumnSplitter==nullptr)
        return;

    // Ширина левой колонки: сохранённая, иначе треть окна
    int leftW=-1;
    const QList<int> hs=hSplitter->sizes();

    if(hs.size()>0 && hs.at(0)>0)
        leftW=hs.at(0);

    if(leftW<=0)
    {
        int total=hSplitter->width();

        if(total<=0)
            total=900;

        leftW=total/3;
    }

    hSplitter->setSizes(QList<int>() << leftW << (hSplitter->width()-leftW>0 ? hSplitter->width()-leftW : leftW*2));

    // Высота дерева и списка внутри колонки: сохранённые, иначе поровну
    const QList<int> saved=mytetraConfig.get_leftspl_size_list();

    if(saved.size()==leftColumnSplitter->count())
        leftColumnSplitter->setSizes(saved);
    else
    {
        const int total=leftColumnSplitter->height();
        const int each=(total>0) ? total/2 : -1;

        if(each>0)
            leftColumnSplitter->setSizes(QList<int>() << each << (total-each));
    }

    // Сходимость: если окно ещё не разложено (холодный старт без
    // сохранённой геометрии), выставленные размеры может снести
    // первый проход раскладки. Проверяем видимый итог и повторяем,
    // но не бесконечно
    if(leftColumnSplitter->width()<=0 && recordTableSizesRetry<20)
    {
        recordTableSizesRetry++;
        QTimer::singleShot(100, this, &MainWindow::applyRecordTableSizes);
    }
    else
        recordTableSizesRetry=0;
}


// Запоминается геометрия окна и позиции основных разделителей
void MainWindow::saveWindowGeometry(void)
{
    qDebug() << "Save window geometry and splitter sizes";

    mytetraConfig.set_mainwingeometry( saveGeometry() );

    // Размеры vSplitter осмысленны только с двумя виджетами
    // (в режиме "список под деревом" там один редактор)
    if(vSplitter->count()==2)
        mytetraConfig.set_vspl_size_list(vSplitter->sizes());

    mytetraConfig.set_hspl_size_list(hSplitter->sizes());

    // Размеры левой колонки (дерево над списком) - только в этом режиме
    if(leftColumnSplitter!=nullptr)
        mytetraConfig.set_leftspl_size_list(leftColumnSplitter->sizes());


    // Запоминается размер сплиттера только при видимом виджете поиска,
    // т.к. если виджета поиска невидно, будет запомнен нуливой размер

    // if(findScreenDisp->isVisible()) - так делать нельзя, т.к.
    // данный метод вызывается из декструктора главного окна, и к этому моменту
    // виджет уже невиден

    if(mytetraConfig.get_findscreen_show())
        mytetraConfig.set_findsplitter_size_list(findSplitter->sizes());
}


void MainWindow::restoreTreePosition(void)
{
    // Путь к последнему выбранному в дереве элементу
    QStringList path=mytetraConfig.get_tree_position();

    qDebug() << "MainWindow::restoreTreePosition() : " << path;

    this->setTreePosition(path);
}


void MainWindow::saveTreePosition(void)
{
    // Получение QModelIndex выделенного в дереве элемента
    QModelIndex index=treeScreen->getCurrentItemIndex();

    // Получаем указатель вида TreeItem
    TreeItem *item =treeScreen->knowTreeModel->getItem(index);

    // Сохраняем путь к элементу item
    mytetraConfig.set_tree_position(item->getPath());
}


void MainWindow::setTreePosition(QStringList path)
{
    if(treeScreen->knowTreeModel->isItemValid(path)==false)
        return;

    // Получаем указатель на элемент вида TreeItem, используя путь, этот указатель может вернуть и корень (если путь пустой)
    TreeItem *item =treeScreen->knowTreeModel->getItem(path);

    // qDebug() << "Set tree position to " << item->getField("name") << " id " << item->getField("id");

    // Из указателя на элемент TreeItem получаем QModelIndex
    QModelIndex setto=treeScreen->knowTreeModel->getIndexByItem(item);

    // Курсор устанавливается в нужную позицию
    treeScreen->setCursorToIndex(setto);
}


bool MainWindow::isTreePositionCrypt()
{
    QStringList path=mytetraConfig.get_tree_position();

    if(treeScreen->knowTreeModel->isItemValid(path)==false)
        return false;

    // Получаем указатель на элемент вида TreeItem, используя путь
    TreeItem *item =treeScreen->knowTreeModel->getItem(path);

    if(item->getField("crypt")=="1")
        return true;
    else
        return false;
}


void MainWindow::restoreRecordTablePosition(void)
{
    QString id=mytetraConfig.get_recordtable_selected_record_id();

    if(id.length()>0)
        this->setRecordtablePositionById(id);
}


void MainWindow::saveRecordTablePosition(void)
{
    QString id=recordTableScreen->getFirstSelectionId();

    mytetraConfig.set_recordtable_selected_record_id(id);
}


void MainWindow::setRecordtablePositionById(QString id)
{
    recordTableScreen->setSelectionToId(id);
}


void MainWindow::setTreeAndRecordtablePositions(QStringList treePath, QString recordId)
{
    // Прыжок извне при активном фильтре дерева: цель может быть скрыта,
    // тогда фильтр сбрасывается здесь, до позиционирования
    if(treeScreen!=nullptr)
        treeScreen->ensurePathVisible(treePath);

    // В дереве выставляется ветка с заданным путем
    // При этом автоматически вызовется слот TreeScreen::onKnowtreeClicked(),
    // так как происходит выбор в selection-модели
    this->setTreePosition(treePath);

    // Так как выбор ветки в дереве приводит к обновлению таблицы списка записей
    // только на следующей итерации обработки основного цикла событий,
    // то здась цикл событий обновляется, чтобы нормально сработал
    // дальнейший вызов setRecordtablePositionById(), чтобы он выставлял запись
    // уже на обновленной таблице конечных записей
    QCoreApplication::processEvents();

    // В таблице конечных записей выставляется заданная запись
    this->setRecordtablePositionById(recordId);
}


void MainWindow::saveEditorCursorPosition(void)
{
    int n=editorScreen->getCursorPosition();

    mytetraConfig.setEditorCursorPosition(n);
}


void MainWindow::restoreEditorCursorPosition(void)
{
    int n=mytetraConfig.getEditorCursorPosition();

    editorScreen->setCursorPosition(n);
}


void MainWindow::saveEditorScrollBarPosition(void)
{
    int n=editorScreen->getScrollBarPosition();

    mytetraConfig.setEditorScrollBarPosition(n);
}


void MainWindow::restoreEditorScrollBarPosition(void)
{
    int n=mytetraConfig.getEditorScrollBarPosition();

    editorScreen->setScrollBarPosition(n);
}


void MainWindow::restoreFindOnBaseVisible(void)
{
    bool n=mytetraConfig.get_findscreen_show();

    // Определяется ссылка на виджет поиска
    FindScreen *findScreenRel=find_object<FindScreen>("findScreenDisp");

    if(n)
        findScreenRel->show();
    else
        findScreenRel->hide();
}


void MainWindow::restoreAllWindowState(void)
{
    globalParameters.getWindowSwitcher()->disableSwitch();

    restoreFindOnBaseVisible();
    restoreWindowGeometry();

    // Раскладка списка записей применяется после геометрии:
    // ей нужны живые размеры виджетов
    applyRecordTablePosition();

    restoreTreePosition();

    // Так как выбор ветки в дереве приводит к обновлению таблицы списка записей
    // только на следующей итерации обработки основного цикла событий,
    // то здась цикл событий обновляется, чтобы нормально сработал
    // дальнейший вызов setRecordtablePositionById(), чтобы он выставлял запись
    // уже на обновленной таблице конечных записей
    QCoreApplication::processEvents();

    restoreRecordTablePosition();


    restoreEditorCursorPosition();
    restoreEditorScrollBarPosition();

    globalParameters.getWindowSwitcher()->enableSwitch();
}


void MainWindow::restoreDockableWindowsState()
{
    // Восстанавливаются открепляемые окна
    EditorShowTextDispatcher::instance()->restoreOpenWindows();
}


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
void MainWindow::onViewToolsToggled(bool checked)
{
    if(editorScreen==nullptr || editorScreen->editorToolBarAssistant==nullptr)
        return;

    editorScreen->editorToolBarAssistant->switchToolsVisibility(checked ? 1 : -1);
}


// Переключение глобального режима только чтения: навигация и просмотр
// разрешены, любые изменения базы запрещены
void MainWindow::onViewReadOnlyToggled(bool checked)
{
    mytetraConfig.set_readOnly(checked);

    applyReadOnly();
}


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
void MainWindow::onThemeConfigure(void)
{
    AppConfigDialog dialog("pageTheme", this);
    dialog.exec();
}


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
void MainWindow::flashMcpWrite(void)
{
    updateMcpBanner();
}


// Текст баннера: сессия активна и режимы пиров в скобках сразу
// после «активна». UI заблокирован всегда пока висит баннер,
// независимо от режима сервера: его записи идут мимо блокировки
// своим каналом
QString MainWindow::mcpBannerText(void)
{
    QStringList modes=mcpPeerModes.values();
    modes.removeAll(QString());
    modes.removeDuplicates();
    modes.sort();

    QString modesText;

    if(!modes.isEmpty())
        modesText=" ("+modes.join(", ")+")";

    QStringList peers;

    for(auto it=mcpPeers.constBegin(); it!=mcpPeers.constEnd(); ++it)
    {
      const QString mode=mcpPeerModes.value(it.key());

      peers.append(mode.isEmpty() ? it.key() : it.key()+QStringLiteral(" ")+mode);
    }

    QString text=tr("MCP session is active%1, MyTetra works in read-only mode").arg(modesText);

    if(!peers.isEmpty())
      text+=tr("; peers: %1").arg(peers.join(QStringLiteral(", ")));

    return text;
}


// Баннер MCP-сессии: висит всё время пока жив хоть один пир.
// TTL-килла нет осознанно: долгие нейронные сессии с паузами
// убивались бы на ровном месте вместе с историей. Пир уходит
// только по явному Bye; убитый -9 висит до перезапуска программы.
// Пусто — баннер гаснет и UI разблокируется, есть пиры — баннер
// виден и UI заблокирован на запись до конца сессии
// Имя своего бинаря: MCP-сервер это та же программа, так что
// чужой процесс под recycled pid опознается по имени
static QString mcpOwnAppName(void)
{
  return QFileInfo(QCoreApplication::applicationFilePath()).baseName();
}


// Жив ли пир: процесс с таким pid существует И это наша программа.
// Имя сверяется чтобы переиспользованный pid чужого процесса не
// держал висящий баннер. Не прочиталось имя — считаем живым:
// лучше лишний баннер, чем выкинутый живой пир
static bool mcpPeerAlive(const QString &pid)
{
  const qint64 id=pid.toLongLong();

  if(id<=0)
    return false;

#ifdef Q_OS_WIN32
  HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                            static_cast<DWORD>(id));

  if(handle==nullptr)
    return GetLastError()==ERROR_ACCESS_DENIED;

  char imagePath[MAX_PATH];
  DWORD imageSize=sizeof(imagePath);
  const bool nameRead=(QueryFullProcessImageNameA(handle, 0,
                                                  imagePath, &imageSize)!=0);
  CloseHandle(handle);

  if(!nameRead)
    return true;

  return QFileInfo(QString::fromLocal8Bit(imagePath)).baseName()
         .compare(mcpOwnAppName(), Qt::CaseInsensitive)==0;
#elif defined(Q_OS_LINUX)
  if(::kill(static_cast<pid_t>(id), 0)!=0)
    return errno==EPERM;

  QFile commFile(QStringLiteral("/proc/%1/comm").arg(id));

  if(!commFile.open(QIODevice::ReadOnly | QIODevice::Text))
    return true;

  // comm режется ядром до 15 символов: сравнивается столько же
  const QString comm=QString::fromLocal8Bit(commFile.readAll()).trimmed();

  return comm==mcpOwnAppName().left(15);
#else
  if(::kill(static_cast<pid_t>(id), 0)==0)
    return true;

  return errno==EPERM;
#endif
}


void MainWindow::onMcpPeerTick(void)
{
  QList<QString> dead;

  for(auto it=mcpPeers.constBegin(); it!=mcpPeers.constEnd(); ++it)
    if(!mcpPeerAlive(it.key()))
      dead.append(it.key());

  foreach(const QString &pid, dead)
  {
    mcpPeers.remove(pid);
    mcpPeerModes.remove(pid);
  }

  updateMcpBanner();
}


void MainWindow::updateMcpBanner(void)
{
    if(mcpBannerBar==nullptr || mcpBanner==nullptr)
        return;

    if(mcpPeers.isEmpty())
    {
        setMcpUiLock(false);
        mcpBannerBar->hide();

        // Дисконнект: история исчезает сразу и нигде не сохраняется
        mcpHistory.clear();
        updateMcpHistoryDialog();

        if(mcpHistoryDialog!=nullptr)
            mcpHistoryDialog->close();
    }
    else
    {
        setMcpUiLock(true);
        mcpBanner->setText(mcpBannerText());
        mcpBannerBar->show();
    }
}


// Перезапуск цепочки умного автосохранения с текущим конфигом
void MainWindow::restartAutosaveTimer(void)
{
    if(autosaveTimer==nullptr)
        return;

    const int minutes=mytetraConfig.get_autosaveInterval();

    if(!mytetraConfig.get_autosaveEnable() || minutes<1)
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
void MainWindow::setMcpUiLock(bool locked)
{
    if(mytetraConfig.isMcpUiLocked()==locked)
        return;

    if(locked)
        saveTextarea();

    mytetraConfig.setMcpUiLock(locked);
    applyReadOnly();
}


// Накопление истории действий MCP-сессии. Хвост обрезан до трёхсот:
// окну нужен обзор, а не архив. На диск ничего не пишется
void MainWindow::addMcpHistoryEntry(const QString &kind,
                                    const QString &tool,
                                    const QString &summary)
{
    McpHistoryEntry entry;
    entry.msecs=QDateTime::currentMSecsSinceEpoch();
    entry.kind=kind.isEmpty() ? QStringLiteral("other") : kind;
    entry.tool=tool;
    entry.summary=summary;

    mcpHistory.append(entry);

    // История действий MCP для диалога: сессия агента легко
    // дает сотни вызовов, сотни строк обрезали бы середину работы
    while(mcpHistory.size()>5000)
        mcpHistory.removeFirst();

    updateMcpHistoryDialog();
}


void MainWindow::updateMcpHistoryDialog(void)
{
    if(mcpHistoryDialog!=nullptr)
        mcpHistoryDialog->setHistory(mcpHistory);
}


// Таблица разбивки статистики: строится отдельно чтобы сортировку
// можно было проверить без модального окна
QTableWidget *MainWindow::buildStatsTableWidget(QWidget *parent)
{
    QList<BaseStatRow> rows;

    if(treeScreen!=nullptr && treeScreen->knowTreeModel!=nullptr)
        rows=treeScreen->knowTreeModel->getBaseStatRows();

    QTableWidget *table=new QTableWidget(parent);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(QStringList() << tr("Branch") << tr("Note") << tr("Internal links") << tr("Images") << tr("Files") << tr("Tags"));
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setRowCount(rows.size());

    // Компактные строки в одну текстовую строку как в истории и метках:
    // дефолтная высота секции QTableWidget заметно выше шрифта
    const int rowHeight=table->fontMetrics().height()+2;
    table->verticalHeader()->setMinimumSectionSize(rowHeight);
    table->verticalHeader()->setDefaultSectionSize(rowHeight);

    for(int r=0; r<rows.size(); ++r)
    {
        const BaseStatRow &row=rows.at(r);

        QTableWidgetItem *branchItem=new QTableWidgetItem(row.branchPath);
        branchItem->setData(Qt::UserRole, row.recordId);
        table->setItem(r, 0, branchItem);
        table->setItem(r, 1, new QTableWidgetItem(row.noteName));
        table->setItem(r, 2, new NumberTableItem(row.links));
        table->setItem(r, 3, new NumberTableItem(row.images));
        table->setItem(r, 4, new NumberTableItem(row.files));
        table->setItem(r, 5, new NumberTableItem(row.tags));
    }

    table->resizeColumnsToContents();
    table->sortByColumn(2, Qt::DescendingOrder);

    // Первый клик по столбцу сортирует по убыванию (там обычно
    // интереснее крупные), повторный клик переключает порядок.
    // Ручной обработчик вместо автосортировки: встроенная всегда
    // начинает с возрастания. Порядок помнится свой, а не читается
    // с индикатора: встроенный слот срабатывает раньше нашего
    // и успевает индикатор перевернуть
    connect(table->horizontalHeader(), &QHeaderView::sectionClicked, table,
            [table, lastColumn=2, lastOrder=Qt::DescendingOrder](int logicalIndex) mutable
    {
      Qt::SortOrder order=Qt::DescendingOrder;

      if(logicalIndex==lastColumn && lastOrder==Qt::DescendingOrder)
        order=Qt::AscendingOrder;

      lastColumn=logicalIndex;
      lastOrder=order;

      table->sortByColumn(logicalIndex, order);
    });

    return table;
}


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
    shortcutManager.initAction("misc-print", actionFileMenuPrintPreview );
    shortcutManager.initAction("misc-printDirect", actionFileMenuPrint );
    shortcutManager.initAction("misc-exportPdf", actionFileMenuExportPdf );
    shortcutManager.initAction("misc-quit", actionFileMenuQuit );

    shortcutManager.initAction("misc-findInBase", actionToolsMenuFindInBase );

    shortcutManager.initAction("misc-imagesGallery", actionToolsMenuImagesGallery );

    shortcutManager.initAction("misc-clipFromClipboard", actionToolsMenuClipFromClipboard );

    shortcutManager.initAction("misc-readOnly", viewReadOnlyAction );

    shortcutManager.initAction("misc-focusTree", actionFocusTree );
    shortcutManager.initAction("misc-focusNoteTable", actionFocusNoteTable );
    shortcutManager.initAction("misc-focusEditor", actionFocusEditor );
}


// Перевести статичные тексты новым языком. Вызывается до
// shortcutManager.retranslate(): тексты на шорткатах освежит сигнал,
// а прямой setText их бы затер
void MainWindow::retranslateUi(void)
{
    if(fileMenu!=nullptr)
    {
        fileMenu->setTitle(tr("&File"));

        actionFileMenuDatabasesManagement->setText(tr("Databases management"));
        actionFileMenuExportTreeItem->setText(tr("Export tree item"));
        actionFileMenuImportTreeItem->setText(tr("Import tree item"));
        actionFileMenuImportTelegram->setText(tr("Import Telegram chat..."));
        actionFileMenuPrintPreview->setText(tr("Print Preview..."));
    }

    if(toolsMenu!=nullptr)
    {
        toolsMenu->setTitle(tr("&Tools"));

        actionToolsMenuActionLog->setText(tr("Action &log"));

        actionToolsMenuFilesGallery->setText(tr("Attached &files"));

        // Переключатель дока истории: текст тоже идет через шорткаты,
        // но initAction для него звался один раз при построении
        shortcutManager.initAction("misc-historyPanel", historyPanelDock->toggleViewAction());

        // Переключатель дока меток: текст тоже идет через шорткаты,
        // но initAction для него звался один раз при построении
        shortcutManager.initAction("misc-tagsPanel", tagsPanelDock->toggleViewAction());

        if(actionToolsMenuPreferences!=nullptr)
            actionToolsMenuPreferences->setText(tr("&Preferences"));

        if(preferencesSubMenu!=nullptr)
        {
            preferencesSubMenu->setTitle(tr("&Preferences"));

            // Пары действие-текст как в initPreferencesMenu
            const struct { QAction **slot; const char *text; } pages[]={
                {&actionDirectPreferencesMain, "Main"},
                {&actionDirectPreferencesAppearance, "Appearance"},
                {&actionDirectPreferencesCrypt, "Crypt"},
                {&actionDirectPreferencesSyncro, "Syncro"},
                {&actionDirectPreferencesRecordTable, "Notes Area"},
                {&actionDirectPreferencesAttach, "Attaches"},
                {&actionDirectPreferencesKeyboard, "Keyboard"},
                {&actionDirectPreferencesHistory, "History"},
                {&actionDirectPreferencesMisc, "Misc"},
            };

            for(unsigned i=0; i<sizeof(pages)/sizeof(pages[0]); ++i)
                if(*pages[i].slot!=nullptr)
                    (*pages[i].slot)->setText(tr(pages[i].text));
        }
    }

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

        if(themeConfigureAction!=nullptr)
            themeConfigureAction->setText(tr("Configure theme..."));
    }

    if(viewMenu!=nullptr)
    {
        viewMenu->setTitle(tr("&View"));

        if(viewHeaderAction!=nullptr)
            viewHeaderAction->setText(tr("Show note header"));

        if(viewFavoritesAction!=nullptr)
            viewFavoritesAction->setText(tr("Show favorites panel"));

        if(viewTreeFilterAction!=nullptr)
            viewTreeFilterAction->setText(tr("Show branch filter"));

        if(viewSecretColorAction!=nullptr)
            viewSecretColorAction->setText(tr("Change secret color..."));

        if(viewPaperColorAction!=nullptr)
            viewPaperColorAction->setText(tr("Change note background color..."));

        // syncViewMenu обновит пометки checkable-пунктов
        syncViewMenu();
    }

    if(helpMenu!=nullptr)
    {
        helpMenu->setTitle(tr("&Help"));

        actionHelpMenuAboutMyTetra->setText(tr("About MyTetra"));
        actionHelpMenuAboutQt->setText(tr("About Qt"));
        actionHelpMenuTechnicalInfo->setText(tr("Technical info"));
    }

    actionTrayRestore->setText(tr("&Restore window"));
    actionTrayMaximize->setText(tr("Ma&ximize window"));
    actionTrayMinimize->setText(tr("Mi&nimize window"));
    actionTrayQuit->setText(tr("&Quit"));

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
        mcpBanner->setText(mcpBannerText());

    if(mcpHistoryDialog!=nullptr)
        mcpHistoryDialog->retranslateUi();
}


// Сохранить текущую статью
bool MainWindow::fileSave(void)
{
    // todo: сделать функционал

    return true;
}


// Сохранить текущую статью как файл
bool MainWindow::fileSaveAs(void)
{
    // todo: сделать функционал
    return true;
}


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
void MainWindow::filePrintPreview(void)
{
    PrintPreview *preview = new PrintPreview(editorScreen->getTextareaDocument(), this);

    preview->setWindowModality(Qt::WindowModal);
    preview->setAttribute(Qt::WA_DeleteOnClose);
    preview->show();
}


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
void MainWindow::fileDatabasesManagement(void)
{
    DatabasesManagementScreen databasesManagementScreen(this);
    databasesManagementScreen.exec();
}


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
    if(mytetraConfig.get_readOnly())
        return;

    TelegramImportDialog dialog(this);
    dialog.exec();
}


void MainWindow::fileImportBranch(void)
{
    // Глобальный режим только чтения: импорт меняет базу
    if(mytetraConfig.get_readOnly())
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
void MainWindow::applicationExit(void)
{
    saveAllState();

    // Если происходит первая инициализация выхода из программы
    // И если в конфиге настроено, что нужно синхронизироваться при выходе
    // И задана команда синхронизации
    if(enableRealClose==false)
        if(mytetraConfig.get_synchroonexit())
            if(mytetraConfig.get_synchrocommand().trimmed().length()>0)
            {
                enableRealClose=true;
                synchronization(); // В конце синхронизации будет вызван слот onSyncroCommandFinishWork()
            }

    // Запуск выхода из программы
    enableRealClose=true;
    emit close();
}


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
void MainWindow::showTrayMessage(const QString &title, const QString &text)
{
    if(QSystemTrayIcon::isSystemTrayAvailable()==false)
        return;

    if(trayIcon==nullptr || trayIcon->isVisible()==false)
        return;

    trayIcon->showMessage(title, text, QSystemTrayIcon::Warning);
}


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
void MainWindow::onExpandEditArea(bool flag)
{
    if(flag)
    {
        globalParameters.getTreeScreen()->hide();
        globalParameters.getRecordTableScreen()->hide();
    }
    else
    {
        globalParameters.getTreeScreen()->show();
        globalParameters.getRecordTableScreen()->show();
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

    infoTargetOs="Target OS: "+globalParameters.getTargetOs()+"<br/>";
    infoProgramFile="Program file: "+globalParameters.getMainProgramFile()+"<br/>";
    infoWorkDirectory="Work directory: "+globalParameters.getWorkDirectory()+"<br/>";

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
void MainWindow::reload(void)
{
  reloadSaveStage();

  // Стадия Load идет сразу после стадии Save, флаг о наличии задержки не выставляется
  reloadLoadStage(false);
}


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
void MainWindow::reloadLoadStage(bool isLongTimeReload)
{
    // Блокируется история
    walkHistory.setDrop(true);

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
    walkHistory.setDrop(false);
}


// Старт синхронизации
void MainWindow::synchronization(bool visible)
{
    // Глобальный режим только чтения: синхронизация может менять базу
    if(mytetraConfig.get_readOnly())
        return;

    // Если кнопка синхронизации заблокирована, начинать синхронизацию нельзя
    if(!recordTableScreen->isSyncroEnabled())
        return;

    // Блокируется кнопка синхронизации, чтобы два раза случайно не нажать синхронизацию (окно синхронизации не модально)
    recordTableScreen->setSyncroEnabled(false);

    actionLogger.addAction("startSyncro");

    reloadSaveStage();

    // Считывается команда синхронизации
    QString command=mytetraConfig.get_synchrocommand();

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
    // QString databasePath=globalParameters.getWorkDirectory()+"/"+mytetraConfig.get_tetradir();
    QDir databaseDir( mytetraConfig.get_tetradir() );
    QString databasePath=databaseDir.canonicalPath();

    command.replace("%a", databasePath);

    // Запуск команды синхронизации
    auto runner = globalParameters.getSyncroCommandRunnner();
    runner->setWindowTitle(tr("MyTetra synchronization"));
    runner->setMessageText(tr("Synchronization in progress, please wait..."));
    runner->setCommand(command);
    runner->run(visible);
}


// Завершение синхронизации
void MainWindow::onSyncroCommandFinishWork()
{
    // Функция перечитывания дерева знаний вызывается с флагом,
    // что от предыдущей стадии была большая задержка
    reloadLoadStage(true);

    // После того, как дерево перечитано, содержимое открепляемых окон обновляется,
    // так как содержимое записей могло поменяться (т. к. получены новые изменения)
    EditorShowTextDispatcher::instance()->updateAllWindows();

    // Возможно, что после обновления имеются открепляемые окна, показывающие
    // записи, которые были удалены при синхронизации. Такие окна должны быть закрыты.
    // Проверка и закрытие таких окон делается в отдельном потоке, потому вызов
    // делается через сингнал-слот
    emit doUpdateDetachedWindows();

    actionLogger.addAction("stopSyncro");

    // В конце синхронизации нужно проверить, не происходит ли выход из программы
    if(enableRealClose==true)
    {
        this->applicationExit();
    }
}


void MainWindow::setupIconActions(void)
{
    actionTrayRestore = new QAction(tr("&Restore window"), this);
    connect(actionTrayRestore, &QAction::triggered, this, &MainWindow::showWindow);

    actionTrayMaximize = new QAction(tr("Ma&ximize window"), this);
    connect(actionTrayMaximize, &QAction::triggered, this, &MainWindow::showMaximized);

    actionTrayMinimize = new QAction(tr("Mi&nimize window"), this);
    connect(actionTrayMinimize, &QAction::triggered, this, &MainWindow::hide);

    actionTrayQuit = new QAction(tr("&Quit"), this);
    connect(actionTrayQuit, &QAction::triggered, this, &MainWindow::applicationExit);
}


void MainWindow::showWindow()
{
    activateWindow();
    showNormal();
    raise();

    // Восстанавливается поведение открепляемых окон
    EditorShowTextDispatcher::instance()->restoreBehavior();
}


void MainWindow::createTrayIcon(void)
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(actionTrayRestore);
    trayIconMenu->addAction(actionTrayMaximize);
    trayIconMenu->addAction(actionTrayMinimize);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(actionTrayQuit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
}


void MainWindow::setIcon(void)
{
    connect(trayIcon, &QSystemTrayIcon::activated,
            this,     &MainWindow::iconActivated);

    QIcon icon = QIcon(":/resource/pic/logo.svg");
    trayIcon->setIcon(icon);
    setWindowIcon(icon);

    // tray_icon->setToolTip(iconComboBox->itemText(index));
}


void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(QSystemTrayIcon::isSystemTrayAvailable()==false)
        return;

    qDebug() << "Click on tray icon";

    switch (reason)
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        // Если окно видно
        if(isVisible())
        {
            // Если окно неактивно, значит активно другое (и возможно оно перекрывает окно MyTetra)
            // Не работает в Windows, в Linux не проверял
            // Причина неработоспособности - при клике на иконку, окно MyTera всегда становится неактивным (т.к. активен систрей)
            // И условие срабатывает всегда. Доделать или отказаться
            // if(QGuiApplication::applicationState() == Qt::ApplicationInactive)
            // {
            //   activateWindow();
            //   return;
            // }

            if(isMinimized())
            {
                qDebug() << "If visible and minimized";
                showWindow();
                return;
            }
            else
            {
                qDebug() << "Hide";
                hide();
                return;
            }
        }
        else
        {
            qDebug() << "If not visible";
            showWindow();
            return;
            // if(isMinimized()) showNormal();
            // else show();
        }
    default:
        ;
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
        if(trayIcon->isVisible())
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
        EditorShowTextDispatcher::instance()->saveOpenWindows();

        // Закрытие открепляемых окон, даже если главное окно не является родителем для
        // открепляемых окон, чтобы не осталось "висячих" открепляемых окон, которые
        // не будут давать закрыться приложению
        EditorShowTextDispatcher::instance()->closeAllWindowsForExit();

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
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Проброс нажатия клавиши Ctrl в редактор, даже если он не активный (не в фокусе)
    if( event->key() == Qt::Key_Control )
        if(editorScreen!=nullptr)
            emit globalPressKey(Qt::Key_Control);

    QMainWindow::keyPressEvent(event);
}


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
    walkHistory.add(id,
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
    walkHistory.add(id,
                    editorScreen->getCursorPosition(),
                    editorScreen->getScrollBarPosition(),
                    WALK_HISTORY_GO_NEXT);

    goWalkHistory();
}


void MainWindow::goWalkHistory(void)
{
    // Выясняется идентификатор записи, на которую надо переключиться
    QString id=walkHistory.getId();

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

    if(mytetraConfig.getRememberCursorAtHistoryNavigation())
    {
        editorScreen->setCursorPosition( walkHistory.getCursorPosition(id) );
        editorScreen->setScrollBarPosition( walkHistory.getScrollBarPosition(id) );
    }
}


// Метод, вызываемый из всех точек интерфейса, в которых происходит
// переход к другой записи. Метод вызывается до перехода, чтобы сохранить
// текст редактируемой записи
void MainWindow::saveTextarea(void)
{
    // Глобальный режим только чтения
    if(mytetraConfig.get_readOnly())
        return;

    QString id=editorScreen->getMiscField("id");

    qDebug() << "MainWindow::saveTextarea() : id :" << id;

    // Текст правили или только ходили по веткам: индекс и статистика
    // нужны лишь в первом случае, иначе это полный рескан базы
    // на каждый клик (тысячи чтений text.html с диска)
    const bool modified=editorScreen->getTextareaModified();

    editorScreen->saveTextarea();

    walkHistory.add(id,
                    editorScreen->getCursorPosition(),
                    editorScreen->getScrollBarPosition());

    if(!modified)
      return;

    // Текст на диске обновился: перепарсить исходящие ссылки записи
    if(!id.isEmpty())
      BacklinkIndex::instance().updateSource(id);

    // Текст мог принести/убрать картинки и ссылки: одна строка
    // в кеше без полного прохода, подпись из него же
    if(treeScreen!=nullptr && treeScreen->knowTreeModel!=nullptr && !id.isEmpty())
      treeScreen->knowTreeModel->refreshRecordStats(id);

    updateStats();
}


// Слот, обрабатывающий смену фокуса на виджетах
void MainWindow::onFocusChanged(QWidget *widgetFrom, QWidget *widgetTo)
{
    Q_UNUSED (widgetFrom)

    if(widgetTo==nullptr)
        return;

    qDebug() << "MainWindow::onFocusChanged() to " << widgetTo->objectName();

    return; // Временно ничего не делает
}


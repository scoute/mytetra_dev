#include <QSignalBlocker>
#include <QDialog>
#include <QString>
#include <QDir>
#include <QDesktopWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPrintDialog>
#include <QPrinter>
#include <QCloseEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>
#include <QCoreApplication>
#include <QDateTime>
#include <QHostInfo>
#include <QSslCertificate>
#include <QClipboard>
#include <QApplication>

#include "main.h"
#include "models/appConfig/AppConfig.h"
#include "MainWindow.h"
#include "views/printPreview/PrintPreview.h"
#include "views/appConfigWindow/AppConfigDialog.h"
#include "libraries/TrashMonitoring.h"
#include "views/tree/TreeScreen.h"
#include "views/record/MetaEditor.h"
#include "views/recordTable/RecordTableScreen.h"
#include "models/tree/TreeItem.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/GlobalParameters.h"
#include "views/consoleEmulator/CommandRunner.h"
#include "libraries/WalkHistory.h"
#include "libraries/ActionLogger.h"
#include "libraries/WindowSwitcher.h"
#include "views/actionLog/ActionLogScreen.h"
#include "views/sync/SyncDevicesDialog.h"
#include "views/sync/SyncPairDialogs.h"
#include "views/sync/SyncProgressWindow.h"
#include "views/sync/SyncPreviewDialog.h"
#include "views/databasesManagement/DatabasesManagementScreen.h"
#include "libraries/ShortcutManager.h"
#include "libraries/RandomInitter.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"
#include "libraries/sync/SyncHub.h"
#include "libraries/sync/SyncSession.h"
#include "libraries/sync/SyncClient.h"
#include "libraries/sync/SyncIdentity.h"
#include "libraries/sync/SyncPairFlow.h"
#include "libraries/sync/SyncPairing.h"
#include "libraries/sync/SyncPairCrypto.h"
#include "libraries/sync/SyncTls.h"
#include "libraries/sync/SyncHandshake.h"
#include "libraries/sync/SyncSidecar.h"
#include "libraries/sync/SyncNoteIo.h"
#include "libraries/sync/SyncDebug.h"


extern AppConfig mytetraConfig;
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
    initSyncMenu();
    initHelpMenu();
    initHiddenActions();
    setupSignals();
    assembly();

    // Сервер синка: восстановить запомненную галку. Молча: если порт
    // занят — флаг сбрасываем, модалку при старте не показываем
    if(mytetraConfig.get_syncServerAutostart())
    {
        if(startSyncServer(syncDefaultPort))
        {
            actionSyncMenuSyncServer->setChecked(true);
            actionSyncMenuSyncServer->setToolTip(
                tr("Listening on port %1").arg(syncHub->port()));
        }
        else
        {
            // Не молчим: иначе пользователь думает, что сервер работает
            mytetraConfig.set_syncServerAutostart(false);
            actionSyncMenuSyncServer->setToolTip(tr("Not running"));

            if(!isSyncHeadlessMode())
                QTimer::singleShot(500, this, [this]() {
                    QMessageBox::warning(
                        this, tr("Sync server"),
                        tr("%1\nAutostart is disabled.")
                        .arg(m_syncServerError.isEmpty()
                             ? tr("Cannot start the sync server on port %1.\n"
                                  "Another sync server may be running.")
                                   .arg(syncDefaultPort)
                             : m_syncServerError));
                });
        }
    }

    setupShortcuts();

    setupIconActions();
    createTrayIcon();
    refreshSyncStatusUi();
    setIcon();
    if(QSystemTrayIcon::isSystemTrayAvailable())
        trayIcon->show();

    // Инициализируется объект слежения за корзиной
    trashMonitoring.init(mytetraConfig.get_trashdir());
    trashMonitoring.update();

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

    // Виды отцепляются от моделей здесь, в теле деструктора, пока все
    // живо: дальше ~QWidget->hideChildren пересчитывает геометрию хедеров,
    // и этот пересчет идет header->proxy->model->table, а данные дерева
    // к тому моменту уже мертвы (был SIGSEGV в RecordTableData::size).
    // setTableData(nullptr) идет через begin/endReset — виды гаснут штатно
    if(treeScreen!=nullptr)
        treeScreen->detachModels();

    if(recordTableScreen!=nullptr)
        recordTableScreen->detachModels();

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

    // Вспомогательный объект переключения окон, используется в мобильном интерфейсе
    windowSwitcher=new WindowSwitcher( this );
    windowSwitcher->setObjectName("windowSwitcher");
    globalParameters.setWindowSwitcher(windowSwitcher);

    // Вспомогательный объект с виджетом синхронизации базы знаний
    syncroCommandRun=new CommandRunner( this );
    globalParameters.setSyncroCommandRunner( syncroCommandRun );

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
    connect(actionFileMenuPrint, &QAction::triggered, this, &MainWindow::filePrint);
    connect(actionFileMenuPrintPreview, &QAction::triggered, this, &MainWindow::filePrintPreview);
    connect(actionFileMenuExportPdf, &QAction::triggered, this, &MainWindow::filePrintPdf);
    connect(actionFileMenuQuit, &QAction::triggered, this, &MainWindow::applicationExit);


    // Поиск по базе в стандартном меню
    connect(actionToolsMenuFindInBase, &QAction::triggered, this, &MainWindow::toolsFindInBase);
    if(mytetraConfig.getInterfaceMode()=="mobile")
    {
        // Кнопка поиска по базе в меню дерева разделов для мобильного интерфейса
        connect(treeScreen, &TreeScreen::treeScreenFindInBaseClicked, this, &MainWindow::toolsFindInBase);

        // Кнопка поиска по базе в меню редактора для мобильного интефейса
        connect(editorScreen, &MetaEditor::wyeditFindInBaseClicked, this, &MainWindow::toolsFindInBase);
    }

    // Вызов окна просмотра лога
    connect(actionToolsMenuActionLog, &QAction::triggered, this, &MainWindow::onActionLogClicked);

    // Синхронизация по Wi-Fi
    connect(actionSyncMenuSyncNow, &QAction::triggered, this, [this]() {
        synchronization(true);
    });
    connect(actionSyncMenuSyncServer, &QAction::toggled, this, &MainWindow::onSyncServerToggle);
    connect(actionSyncMenuBlockPairing, &QAction::toggled,
            this, &MainWindow::onSyncBlockPairingToggled);
    connect(actionSyncMenuSyncPairing, &QAction::triggered, this, &MainWindow::onSyncPairing);
    connect(actionSyncMenuSyncDevices, &QAction::triggered, this, &MainWindow::onSyncDevices);
    connect(&mytetraConfig, &AppConfig::syncPairRequestArrived,
            this, &MainWindow::onSyncPairRequest);
    connect(&mytetraConfig, &AppConfig::syncDevicePaired, this,
            [this](const QString &id, const QString &name) {
        const QString label=name.isEmpty() ? id.left(8) : name;

        if(statusBar!=nullptr)
            statusBar->showMessage(tr("Paired with %1").arg(label), 8000);

        refreshSyncStatusUi();
    });
    connect(&mytetraConfig, &AppConfig::syncDevicesChanged,
            this, &MainWindow::refreshSyncStatusUi);
    connect(&mytetraConfig, &AppConfig::syncServerSettingsChanged,
            this, &MainWindow::refreshSyncStatusUi);

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
}


void MainWindow::assembly(void)
{
    vSplitter=new QSplitter(Qt::Vertical);
    vSplitter->addWidget(recordTableScreen); // Список конечных записей
    vSplitter->addWidget(editorScreen);      // Текст записи
    vSplitter->setCollapsible(0,false); // Список конечных записей не может смыкаться
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

    setCentralWidget(findSplitter);
}


void MainWindow::saveAllState(void)
{
    // Сохранение данных в поле редактирования
    saveTextarea();

    // Сохраняются данные сеанса работы
    saveWindowGeometry();

    // Headless sync-режим (--sync-server/--sync-to/--sync-status/--sync-forget):
    // позицию в дереве и редакторе не трогаем. Иначе сейв перезапишет
    // tree_position текущим (после commit-reload это корень или шифроветка),
    // а следующий headless-старт кликнет шифроветку и повиснет в запросе
    // пароля, которого некому ввести
    if(!isSyncHeadlessMode())
    {
        saveTreePosition();
        saveRecordTablePosition();
        saveEditorCursorPosition();
        saveEditorScrollBarPosition();
    }

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
}


// Восстанавливается геометрия окна и позиции основных разделителей
void MainWindow::restoreWindowGeometry(void)
{
    if(globalParameters.getTargetOs()=="android")
        setWindowState(Qt::WindowMaximized); // Для Андроида окно просто разворачивается на весь экран
    else
        restoreGeometry( mytetraConfig.get_mainwingeometry() );

    vSplitter->setSizes(mytetraConfig.get_vspl_size_list());
    hSplitter->setSizes(mytetraConfig.get_hspl_size_list());
    findSplitter->setSizes(mytetraConfig.get_findsplitter_size_list());
}


// Запоминается геометрия окна и позиции основных разделителей
void MainWindow::saveWindowGeometry(void)
{
    qDebug() << "Save window geometry and splitter sizes";

    mytetraConfig.set_mainwingeometry( saveGeometry() );

    mytetraConfig.set_vspl_size_list(vSplitter->sizes());
    mytetraConfig.set_hspl_size_list(hSplitter->sizes());


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


// Headless sync-режим определяется флагами запуска, а не состоянием:
// в нем нет дисплея, поэтому рестор и сейв UI-позиции пропускаются
bool MainWindow::isSyncHeadlessMode(void)
{
    return globalParameters.getSyncHeadlessMode();
}


void MainWindow::restoreAllWindowState(void)
{
    globalParameters.getWindowSwitcher()->disableSwitch();

    restoreFindOnBaseVisible();
    restoreWindowGeometry();


    // Headless sync-режим: дерево/записи/курсор не восстанавливаем.
    // Восстановление кликает сохраненную ветку, а это может быть шифроветка —
    // дальше модальный запрос пароля и вечный вис без дисплея (см. saveAllState)
    if(isSyncHeadlessMode())
    {
        globalParameters.getWindowSwitcher()->enableSwitch();
        return;
    }

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
    QMenu *menu = new QMenu(tr("&File"), this);
    this->menuBar()->addMenu(menu);

    actionFileMenuDatabasesManagement = new QAction(tr("Databases management"), this);
    menu->addAction(actionFileMenuDatabasesManagement);

    actionFileMenuExportTreeItem = new QAction(tr("Export tree item"), this);
    menu->addAction(actionFileMenuExportTreeItem);

    actionFileMenuImportTreeItem = new QAction(tr("Import tree item"), this);
    menu->addAction(actionFileMenuImportTreeItem);

    menu->addSeparator();

    actionFileMenuPrint = new QAction(tr("&Print..."), this);
    menu->addAction(actionFileMenuPrint);

    actionFileMenuPrintPreview = new QAction(tr("Print Preview..."), this);
    menu->addAction(actionFileMenuPrintPreview);

    actionFileMenuExportPdf = new QAction(tr("&Export PDF..."), this);
    menu->addAction(actionFileMenuExportPdf);

    menu->addSeparator();

    actionFileMenuQuit = new QAction(tr("&Quit"), this);
    menu->addAction(actionFileMenuQuit);
}


// Создание раздела меню Tools
void MainWindow::initToolsMenu(void)
{
    // Создание меню
    QMenu *menu = new QMenu(tr("&Tools"), this);
    this->menuBar()->addMenu(menu);

    actionToolsMenuFindInBase = new QAction(this); // Так как есть this, указатель не будет потерян основным окном
    menu->addAction(actionToolsMenuFindInBase);

    actionToolsMenuActionLog = new QAction(tr("Action &log"), this);
    menu->addAction(actionToolsMenuActionLog);

    menu->addSeparator();

    if(mytetraConfig.getInterfaceMode()=="desktop")
    {
        actionToolsMenuPreferences = new QAction(tr("&Preferences"), this);
        menu->addAction(actionToolsMenuPreferences);
    }
    else
    {
        // Создание подменю
        QMenu *subMenu = new QMenu(tr("&Preferences"), this);
        initPreferencesMenu(subMenu);
        menu->addMenu(subMenu);
    }
}


// Создание отдельного меню Sync: синхронизация — подсистема,
// а не утилита. Сервер на этом устройстве, клиент к другому,
// устройства, спаривание с показом фингерпринтов. MVP: ручной запуск
void MainWindow::initSyncMenu(void)
{
    QMenu *menu = new QMenu(tr("&Sync"), this);
    this->menuBar()->addMenu(menu);

    // Главная задача — из меню, названного её именем (раньше жила только
    // в иконке тулбара таблицы записей).
    actionSyncMenuSyncNow = new QAction(tr("&Synchronize now..."), this);
    menu->addAction(actionSyncMenuSyncNow);

    actionSyncMenuLastRun = new QAction(tr("No sync yet"), this);
    actionSyncMenuLastRun->setEnabled(false);
    menu->addAction(actionSyncMenuLastRun);
    menu->addSeparator();

    actionSyncMenuSyncServer = new QAction(tr("Sync server"), this);
    actionSyncMenuSyncServer->setCheckable(true);
    menu->addAction(actionSyncMenuSyncServer);

    actionSyncMenuBlockPairing = new QAction(tr("Block new pairings"), this);
    actionSyncMenuBlockPairing->setCheckable(true);
    actionSyncMenuBlockPairing->setToolTip(
        tr("Refuse new devices. Existing pairs keep syncing. "
           "Turning it on cancels pairing in progress."));
    menu->addAction(actionSyncMenuBlockPairing);

    actionSyncMenuSyncDevices = new QAction(tr("Sync &devices..."), this);
    menu->addAction(actionSyncMenuSyncDevices);

    actionSyncMenuSyncPairing = new QAction(tr("Sync pairing info..."), this);
    menu->addAction(actionSyncMenuSyncPairing);
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
    }

    delete dialog;
}


// Создание раздела меню Help
void MainWindow::initHelpMenu(void)
{
    // Создание меню
    QMenu *menu = new QMenu(tr("&Help"), this);
    this->menuBar()->addMenu(menu);

    actionHelpMenuAboutMyTetra = new QAction(tr("About MyTetra"), this);
    menu->addAction(actionHelpMenuAboutMyTetra);

    actionHelpMenuAboutQt = new QAction(tr("About Qt"), this);
    menu->addAction(actionHelpMenuAboutQt);

    actionHelpMenuTechnicalInfo = new QAction(tr("Technical info"), this);
    menu->addAction(actionHelpMenuTechnicalInfo);
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


void MainWindow::setupShortcuts(void)
{
    qDebug() << "Setup shortcut for" << this->objectName();

    shortcutManager.initAction("misc-print", actionFileMenuPrint );
    shortcutManager.initAction("misc-exportPdf", actionFileMenuExportPdf );
    shortcutManager.initAction("misc-quit", actionFileMenuQuit );

    shortcutManager.initAction("misc-findInBase", actionToolsMenuFindInBase );

    shortcutManager.initAction("misc-focusTree", actionFocusTree );
    shortcutManager.initAction("misc-focusNoteTable", actionFocusNoteTable );
    shortcutManager.initAction("misc-focusEditor", actionFocusEditor );
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

    preview->setModal(true);
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


void MainWindow::fileImportBranch(void)
{
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


void MainWindow::toolsPreferences(void)
{
    AppConfigDialog dialog("", this); // this нужен чтобы пробрасывать иконку приложения
    dialog.exec();
}


void MainWindow::onActionLogClicked()
{
    ActionLogScreen actionLogScreen(this);
    actionLogScreen.exec();
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
    // Нативный режим кнопки: встроенный Wi-Fi-синк вместо внешней команды.
    // Кнопка — интерактивно, таймер — тихим фоновым тиком
    if(mytetraConfig.get_syncToolbarNative())
    {
        if(visible)
            runToolbarNativeSync(true);
        else
            backgroundSyncTick();

        return;
    }

    // Если кнопка синхронизации заблокирована, начинать синхронизацию нельзя
    if(!recordTableScreen->actionSyncro->isEnabled())
        return;

    // Блокируется кнопка синхронизации, чтобы два раза случайно не нажать синхронизацию (окно синхронизации не модально)
    recordTableScreen->actionSyncro->setEnabled(false);

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
        recordTableScreen->actionSyncro->setEnabled(true);

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


// Кнопка синхронизации в нативном режиме (см. настройку syncToolbarNative)
void MainWindow::runToolbarNativeSync(bool interactive)
{
    // Нативный синк уже идёт (кнопка модальна, а таймер — нет)
    if(m_nativeSyncBusy)
    {
        qDebug() << "Toolbar native sync: busy, skip";
        return;
    }

    m_nativeSyncBusy=true;

    // Сброс флага на любом выходе (меню/отмена/итог): повторный вход
    // таймера или кнопки во время синка пропускается
    struct BusyReset
    {
        MainWindow *w;
        ~BusyReset(void) { w->m_nativeSyncBusy=false; }
    };
    const BusyReset busyReset{this};
    Q_UNUSED(busyReset);
    // Только взаимные пары с известным адресом (как кнопка Sync now
    // в окне устройств: секрет в кольце + lastMutual)
    QList<AppConfig::SyncDeviceInfo> peers;

    for(const AppConfig::SyncDeviceInfo &info : mytetraConfig.get_syncDevices())
    {
        if(SyncIdentity::secretForPeer(&mytetraConfig, info.id).isEmpty())
            continue;

        if(info.lastMutual<=0)
            continue;

        if(info.lastIp.isEmpty())
            continue;

        peers << info;
    }

    if(peers.isEmpty())
    {
        // Таймер молча пропускает: окно устройств из фона не открываем
        if(!interactive)
        {
            qDebug() << "Toolbar native sync: no pairs, skip (timer)";
            return;
        }

        QMessageBox::information(
            this, tr("Sync"),
            tr("No mutually paired devices.\nPair a device first (Sync devices window)."));
        onSyncDevices();
        return;
    }

    AppConfig::SyncDeviceInfo target;
    bool haveTarget=false;

    if(peers.size()==1)
    {
        // Пара единственная — окно синка сразу, без лишних кликов
        target=peers.first();
        haveTarget=true;
    }
    else if(!interactive)
    {
        // Таймер: выбирать некому, меню из фона не показываем
        qDebug() << QStringLiteral("Toolbar native sync: %1 pairs, skip (timer)").arg(peers.size());
        return;
    }
    else
    {
        // Пар несколько — выбираем, с кем синкаться
        QMenu menu(this);

        for(const AppConfig::SyncDeviceInfo &info : peers)
        {
            const QString label=QStringLiteral("%1 (%2)").arg(
                info.name.isEmpty() ? info.id : info.name, info.lastIp);
            QAction *act=menu.addAction(label);
            act->setData(info.id);
        }

        QAction *picked=menu.exec(QCursor::pos());

        if(picked==nullptr)
            return;

        const QString pickedId=picked->data().toString();

        for(const AppConfig::SyncDeviceInfo &info : peers)
            if(info.id==pickedId)
            {
                target=info;
                haveTarget=true;
                break;
            }

        if(!haveTarget)
            return;
    }

    // Тот же нативный путь, что окно Sync devices: стратегия, окно
    // выбора, итог — всё внутри runSyncToHeadless
    bool ok=false;
    const QString result=runSyncToHeadless(
        target.lastIp, QString(), static_cast<quint16>(target.port), ok);

    showSyncReport(this, ok ? tr("Sync complete") : tr("Sync"),
                   trimSyncReportForDialog(result), !ok);
}


// Фоновый тик таймера в нативе (см. backgroundSyncTick в заголовке)
void MainWindow::backgroundSyncTick(void)
{
    using Category=SyncPreview::Category;
    using Action=SyncPreview::Action;

    // Только натив: внешняя команда идёт старым путём и сюда не попадает
    if(m_nativeSyncBusy)
    {
        qDebug() << "Background sync: busy, skip";
        return;
    }

    // Стратегия не улажена — деструктивный выбор за пользователя не делаем
    if(mytetraConfig.get_syncAskStrategy())
    {
        qDebug() << "Background sync: strategy pending, skip";
        return;
    }

    const bool bgPush=mytetraConfig.get_syncBgPush();
    const bool bgPull=mytetraConfig.get_syncBgPull();

    // Оба направления выкл — фон выключен совсем
    if(!bgPush && !bgPull)
    {
        qDebug() << "Background sync: directions off, skip";
        return;
    }

    // Не чаще раза в 15 минут: полный проход по базе каждый тик
    // диск и сеть грузит, чаще — захлебнётся
    const qint64 now=QDateTime::currentMSecsSinceEpoch();

    if(now-m_bgLastTickMs<15*60*1000)
    {
        qDebug() << "Background sync: too soon, skip";
        return;
    }

    // Те же взаимные пары, что у кнопки (секрет в кольце + lastMutual)
    QList<AppConfig::SyncDeviceInfo> peers;

    for(const AppConfig::SyncDeviceInfo &info : mytetraConfig.get_syncDevices())
    {
        if(SyncIdentity::secretForPeer(&mytetraConfig, info.id).isEmpty())
            continue;

        if(info.lastMutual<=0)
            continue;

        if(info.lastIp.isEmpty())
            continue;

        peers << info;
    }

    if(peers.isEmpty())
    {
        qDebug() << "Background sync: no pairs, skip";
        return;
    }

    // Round-robin: каждый тик — следующая пара, все по кругу подтянутся
    if(m_bgNextPeer>=peers.size())
        m_bgNextPeer=0;

    const AppConfig::SyncDeviceInfo target=peers.at(m_bgNextPeer);
    m_bgNextPeer++;

    // Тихие решения: едут только создания в разрешённых направлениях,
    // остальное — Skip (дождётся ручного синка с окном). Удаления так
    // не едут вообще: в финальный план не попадают, guard не нужен
    SyncPreview::Decisions silent;
    silent.category[Category::Create]=Action::Apply;
    silent.category[Category::Rename]=Action::Skip;
    silent.category[Category::Edit]=Action::Skip;
    silent.category[Category::MyDelete]=Action::Skip;
    silent.category[Category::TheirDelete]=Action::Skip;

    m_nativeSyncBusy=true;
    m_bgLastTickMs=now;

    bool ok=false;
    const QString result=runSyncToHeadless(
        target.lastIp, QString(), static_cast<quint16>(target.port), ok,
        nullptr, QString(), &silent, bgPush, bgPull);

    m_nativeSyncBusy=false;

    // Итог — только в лог/строку статуса, модалок из фона нет.
    // Ошибку видно в строке состояния и в меню Sync (last sync).
    if(ok)
        qDebug() << "Background sync ok:" << result.left(200);
    else
    {
        qDebug() << "Background sync failed:" << result.left(500);

        if(statusBar!=nullptr)
            statusBar->showMessage(
                tr("Background sync failed: %1").arg(result.left(140)),
                10000);
    }

    refreshSyncStatusUi();
}


// Нативный синк уже перечитал модель (save+reload внутри сессии):
// закрываем/обновляем откреплённые окна, которые держали старые Record.
void MainWindow::setNativeSyncBusy(bool busy)
{
    m_nativeSyncBusy=busy;
}


void MainWindow::refreshSyncStatusUi(void)
{
    if(actionSyncMenuBlockPairing!=nullptr)
    {
        const QSignalBlocker block(actionSyncMenuBlockPairing);
        actionSyncMenuBlockPairing->setChecked(
            mytetraConfig.get_syncPairingBlocked());
    }

    if(actionSyncMenuLastRun==nullptr)
        return;

    qint64 newest=0;
    AppConfig::SyncDeviceInfo latest;

    for(const AppConfig::SyncDeviceInfo &info : mytetraConfig.get_syncDevices())
        if(info.lastSync>newest)
        {
            newest=info.lastSync;
            latest=info;
        }

    if(newest<=0)
    {
        actionSyncMenuLastRun->setText(tr("No sync yet"));
        actionSyncMenuLastRun->setToolTip(QString());
        if(trayIcon!=nullptr)
            trayIcon->setToolTip(tr("MyTetra"));
        return;
    }

    const QString name=latest.name.isEmpty() ? latest.id.left(8) : latest.name;
    const QString text=tr("Last sync: %1 · %2 · %3")
        .arg(QDateTime::fromMSecsSinceEpoch(newest)
                 .toString(QStringLiteral("dd.MM hh:mm")),
             name,
             latest.lastSyncOk ? tr("ok") : tr("error"));

    actionSyncMenuLastRun->setText(text);
    actionSyncMenuLastRun->setToolTip(latest.lastSyncSummary);

    if(trayIcon!=nullptr)
        trayIcon->setToolTip(tr("MyTetra — %1").arg(text));
}


void MainWindow::finishNativeSyncUi(void)
{
    EditorShowTextDispatcher::instance()->updateAllWindows();
    emit doUpdateDetachedWindows();
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
    QString id=editorScreen->getMiscField("id");

    qDebug() << "MainWindow::saveTextarea() : id :" << id;

    editorScreen->saveTextarea();

    walkHistory.add(id,
                    editorScreen->getCursorPosition(),
                    editorScreen->getScrollBarPosition());
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



// Порт sync-сервера по умолчанию. Фиксированный для MVP:
// обнаружение позже, пока ручной ввод IP на клиенте
const quint16 MainWindow::syncDefaultPort=47777;


bool MainWindow::startSyncServer(quint16 port, const QString &pairingPin)
{
    m_syncServerError.clear();

    const bool tlsWanted=mytetraConfig.get_syncTlsEnabled();
    const QString tlsCert=tlsWanted
        ? mytetraConfig.get_syncTlsCertPath() : QString();

    if(syncHub!=nullptr && syncHub->isRunning())
    {
        // Уже слушаем другой порт/PIN/TLS (автостарт с 47777 и пустым PIN,
        // затем --sync-server --sync-port/--sync-pair): пересоздаём,
        // иначе headless-спаривание и порт молча игнорируются.
        if(syncHub->port()==port && m_syncHubPin==pairingPin
           && syncHub->tlsEnabled()==tlsWanted && m_syncHubTlsCert==tlsCert)
            return true;

        syncHub->stop();
        delete syncHub;
        syncHub=nullptr;
    }

    if(syncHub==nullptr)
    {
        m_syncHubPin=pairingPin;
        syncHub=new SyncHub(treeScreen->knowTreeModel,
                            &mytetraConfig,
                            SyncSidecar::instance(),
                            mytetraConfig.get_tetradir(),
                            this,
                            pairingPin);
    }

    // TLS грузим до listen: если сертификат/ключ битые, сервер не
    // поднимается вообще (fail closed), а не откатывается в HTTP
    syncHub->clearTls();
    m_syncHubTlsCert.clear();

    if(tlsWanted)
    {
        QString tlsError;

        if(!syncHub->setTls(mytetraConfig.get_syncTlsCertPath(),
                            mytetraConfig.get_syncTlsKeyPath(), &tlsError))
        {
            m_syncServerError=tlsError;
            qWarning() << "Sync server TLS:" << tlsError;
            return false;
        }

        m_syncHubTlsCert=tlsCert;
    }

    if(!syncHub->start(port))
    {
        m_syncServerError=tr("Cannot listen on port %1: it may be busy.")
            .arg(port);
        return false;
    }

    return true;
}


void MainWindow::stopSyncServer(void)
{
    if(syncHub!=nullptr)
        syncHub->stop();

    // Коды спаривания живут в памяти сервера: остановка их гасит
    SyncPairing::clearAll();
}


bool MainWindow::isSyncServerRunning(void) const
{
    return syncHub!=nullptr && syncHub->isRunning();
}


void MainWindow::onSyncServerToggle(bool checked)
{
    if(checked)
    {
        if(!startSyncServer(syncDefaultPort))
        {
            QMessageBox::warning(this, tr("Sync"),
                                 m_syncServerError.isEmpty()
                                     ? tr("Cannot start sync server.")
                                     : m_syncServerError);
            actionSyncMenuSyncServer->setChecked(false);
            mytetraConfig.set_syncServerAutostart(false);
            return;
        }

        mytetraConfig.set_syncServerAutostart(true);
        actionSyncMenuSyncServer->setToolTip(
            tr("Listening on port %1").arg(syncHub->port()));
        statusBar->showMessage(tr("Sync server started"), 3000);
    }
    else
    {
        stopSyncServer();

        mytetraConfig.set_syncServerAutostart(false);
        actionSyncMenuSyncServer->setToolTip(tr("Not running"));
        statusBar->showMessage(tr("Sync server stopped"), 3000);
    }
}


QString MainWindow::runSyncToHeadless(const QString &host,
                                      const QString &pin,
                                      quint16 port,
                                      bool &ok,
                                      QString *errorCode,
                                      const QString &scopeRootId,
                                      const SyncPreview::Decisions *silentDecisions,
                                      bool silentPush,
                                      bool silentPull)
{
    SYNC_TRACE_EXTRA(QStringLiteral("host=%1 model=%2 tid=%3")
                     .arg(host, syncPtr(treeScreen->knowTreeModel), syncTid()));
    ok=false;

    if(errorCode!=nullptr)
        errorCode->clear();

    // Снапшот кольца на входе: данные разрешены только если пара
    // с ЭТИМ пиром существовала ДО запуска (гейт 089). Прогон,
    // создавший/починивший пару по PIN, — "попытка установить pair":
    // останавливаемся здесь, данные пойдут следующим запуском.
    // Кольцо: у человека 4-5 устройств, секреты попарные
    const QMap<QString, QString> ringBefore=mytetraConfig.get_syncSecretsRing();
    const QString groupBefore=mytetraConfig.get_syncGroupId();

    SyncClient client(&mytetraConfig);
    client.setTlsMode(static_cast<SyncClient::TlsMode>(
        mytetraConfig.get_syncTlsClientMode()));
    client.setTlsPin(mytetraConfig.get_syncTlsPin(host, port));
    client.setTlsTrustCallback(
        [this](const QString &host, quint16 port, const QSslCertificate &,
               const QString &fingerprint) {
        if(isSyncHeadlessMode())
            return false;

        return SyncPairDialogs::askCertTrust(this, host, port, fingerprint);
    });

    // Предсессионное hello молчало в логах: зависший до сессии клиент
    // не оставлял следов. Дублируем шаги сюда под тем же флагом
    if(syncDebugEnabled())
        qDebug() << "SyncStep: pre-hello" << host << port;

    SyncHelloResult hello=client.hello(host, port);

    if(syncDebugEnabled())
        qDebug() << "SyncStep: pre-hello"
                 << (hello.ok ? QStringLiteral("ok")
                              : QStringLiteral("failed: ")+hello.errorCode);

    // Пара: рабочий секрет уже есть либо input — одноразовый код
    // (16 base32) / легаси-PIN. Общий путь с SyncSession
    const bool codeMode=SyncPairCrypto::isValidCode(
        SyncPairCrypto::normalizeCode(pin));

    // Код генерируется на сервере по pairRequest. GUI-пути уже сделали
    // этот запрос до диалога; headless --sync-to с кодом — нет
    if(codeMode && !hello.remoteDeviceId.isEmpty()
       && SyncIdentity::secretForPeer(&mytetraConfig,
                                      hello.remoteDeviceId).isEmpty())
        client.hello(host, port, true);

    const SyncPairFlow::Result pair=SyncPairFlow::ensurePairing(
        &mytetraConfig, client, host, port, hello, pin);

    if(!pair.ok)
    {
        QString fail=tr("Handshake failed: %1").arg(pair.error);

        if(errorCode!=nullptr)
            *errorCode=pair.errorCode;

        // Подсказки для живых разборок: чьи версии, какой перекос часов,
        // слушает ли другая сторона (Sync now требует слушателя на том конце)
        if(pair.errorCode==QStringLiteral("version-mismatch"))
            fail+=tr(" (theirs: ask peer for --sync-status)");
        else if(pair.errorCode==QStringLiteral("clock-skew"))
            fail+=tr(" (skew %1 ms — compare `date +%%s` on both sides)")
                .arg(hello.skewMs);
        else if(pair.errorCode==QStringLiteral("network"))
            fail+=tr(" (is sync server running on the other side? "
                      "Sync now needs a listener there: Sync menu checkbox "
                      "or --sync-server. For the reverse direction press "
                      "Sync now on that device instead)");

        return fail;
    }

    if(pair.pairedNow && !hello.warning.isEmpty())
        qDebug() << "SyncPair: server warning:" << hello.warning;

    // Пир ответил — запоминаем для окна Sync devices (с IP для Sync now)
    if(hello.ok && !hello.remoteDeviceId.isEmpty()
       && hello.remoteDeviceId!=mytetraConfig.get_syncDeviceId())
        mytetraConfig.touch_syncDevice(hello.remoteDeviceId,
                                       hello.remoteDeviceName,
                                       host, port);

    // Группа: пустая у нас — принимаем удаленную или детерминированную.
    // Чужая непустая при общем PIN — переезд между сетями, хаб прав:
    // берем его группу (зеркало логики SyncSession::run)
    if(!hello.remoteGroupId.isEmpty()
       && hello.remoteGroupId!=mytetraConfig.get_syncGroupId())
    {
        if(mytetraConfig.get_syncGroupId().isEmpty() || !pin.isEmpty())
            mytetraConfig.set_syncGroupId(hello.remoteGroupId);
    }
    else if(mytetraConfig.get_syncGroupId().isEmpty()
            && !hello.remoteDeviceId.isEmpty())
    {
        mytetraConfig.set_syncGroupId(
            SyncIdentity::pairGroupId(mytetraConfig.get_syncDeviceId(),
                                      hello.remoteDeviceId));
    }

    // Взаимность: сервер подтверждает пару (answer.paired) — фиксируем.
    // Transition редкий — логируется внутри noteMutual всегда
    if(hello.ok && hello.paired && !hello.remoteDeviceId.isEmpty()
       && hello.remoteDeviceId!=mytetraConfig.get_syncDeviceId())
        SyncHandshake::noteMutual(&mytetraConfig, hello.remoteDeviceId,
                                 "hello answer paired");

    // Пара создана/пересоздана этим запуском — фиксируем и выходим:
    // это была установка пары, данные следующим запуском.
    // (Секрет тут уже гарантированно есть: пустой раньше бы вышел
    // через "Not paired", adopt выше его выставил)
    if(mytetraConfig.get_syncSecretsRing().value(hello.remoteDeviceId)
           !=ringBefore.value(hello.remoteDeviceId)
       || mytetraConfig.get_syncGroupId()!=groupBefore)
    {
        if(errorCode!=nullptr)
            *errorCode=QStringLiteral("pair-established");

        const QString peerName=hello.remoteDeviceName.isEmpty()
            ? hello.remoteDeviceId : hello.remoteDeviceName;

        // Код sync/2: секрет уже подтверждён вторым hello (pending
        // повышен на сервере). Никакого Accept не нужно — только
        // следующий запуск за данными (гейт пары)
        if(codeMode)
        {
            qDebug() << "SyncPair: paired by code with" << peerName
                     << ", next run will transfer data";
            return tr("Paired with %1 by the one-time code. "
                      "Run sync again to transfer data.")
                .arg(peerName);
        }

        // Легаси-PIN: запрос в пару отдельным hello для Accept на сервере
        qDebug() << "SyncPair: sending pair request to"
                 << host << port
                 << "as" << mytetraConfig.get_syncDeviceName()
                 << "(" << mytetraConfig.get_syncDeviceId() << ")";

        const SyncHelloResult offer=client.hello(host, port, true);

        if(syncDebugEnabled())
            qDebug() << "SyncPair: offer answer ok=" << offer.ok
                     << "paired=" << offer.paired
                     << "pairPending=" << offer.pairPending
                     << (offer.warning.isEmpty() ? QString()
                                               : "warning="+offer.warning);

        if(offer.ok && offer.paired)
        {
            qDebug() << "SyncPair: server pre-accepted (standing PIN), "
                        "next run will transfer data";
            return tr("Pair established with %1, "
                      "run sync again to transfer data.")
                .arg(peerName);
        }

        qDebug() << "SyncPair: request sent, waiting for Accept on" << peerName;
        return tr("Pair request sent to %1, "
                  "waiting for Accept on that device. "
                  "Run sync again after accept.")
            .arg(peerName);
    }

    SyncSession session(treeScreen->knowTreeModel,
                        &mytetraConfig,
                        mytetraConfig.get_tetradir());

    // Стратегия первого синка: GUI — диалог, headless — --sync-strategy.
    // Тихий фон (silentDecisions) стратегию не выбирает: она обязана быть
    // улажена заранее (проверяет вызыватель), здесь страховка — Merge
    session.setStrategyResolver(
        [this, silentDecisions](int localItems, int remoteItems) -> SyncStrategy {
            if(silentDecisions!=nullptr)
                return SyncStrategy::Merge;

            if(isSyncHeadlessMode())
            if(isSyncHeadlessMode())
            {
                const QStringList args=QCoreApplication::arguments();
                const int i=args.indexOf(QStringLiteral("--sync-strategy"));

                if(i>=0 && i+1<args.size())
                {
                    const QString value=args.at(i+1);

                    if(value==QStringLiteral("pull"))
                        return SyncStrategy::PullFromHub;

                    if(value==QStringLiteral("push"))
                        return SyncStrategy::PushToHub;

                    if(value==QStringLiteral("merge-by-name"))
                        return SyncStrategy::MergeByName;
                }

                return SyncStrategy::Merge;
            }

            return askSyncStrategy(this, localItems, remoteItems);
        });

    // Предпросмотр синка — только GUI (категории с действиями + diff).
    // Headless: без диалогов, действует guard удалений ниже.
    // Тихий фон: без диалога, готовые тихие решения (только создания
    // в разрешённых направлениях, остальное — Skip до ручного синка)
    if(silentDecisions!=nullptr)
        session.setPreviewResolver(
            [silentDecisions, silentPush, silentPull](
                const SyncPreview::Preview &preview,
                SyncPreview::Decisions &decisions) -> bool {
                decisions=*silentDecisions;

                for(const SyncPreview::Item &item : preview.items)
                {
                    if(item.category!=SyncPreview::Category::Create)
                        continue;

                    if((item.fromLocal && !silentPush)
                       || (!item.fromLocal && !silentPull))
                        decisions.byId[item.id]=SyncPreview::Action::Skip;
                }

                return true;
            });
    else if(!isSyncHeadlessMode())
        session.setPreviewResolver(
            [this, &session, &host, port](
                const SyncPreview::Preview &preview,
                SyncPreview::Decisions &decisions) -> bool {
                return askSyncPreview(
                    preview, decisions, host, port,
                    [&session](const QMap<QString, SyncPreview::Action>
                               &actions) -> bool {
                        return session.syncPartialNow(actions);
                    });
            });

    // Опасный синк (удаления) — явный выбор: продолжить / отменить /
    // восстановиться с сервера. Безопасное едет молча.
    // Headless: диалогов нет, согласие — явный флаг --sync-allow-delete
    session.setDeleteGuard(
        [this](int remoteDeletes, int localDeletes)
            -> SyncSession::DeleteDecision {
            if(isSyncHeadlessMode())
            {
                const QStringList args=QCoreApplication::arguments();

                if(args.contains(QStringLiteral("--sync-allow-delete")))
                    return SyncSession::DeleteDecision::Proceed;

                qWarning() << "Sync: would delete" << remoteDeletes
                           << "item(s) on the other side and" << localDeletes
                           << "locally - refused without --sync-allow-delete";
                return SyncSession::DeleteDecision::Cancel;
            }

            QMessageBox box(this);
            box.setWindowTitle(tr("Sync"));
            box.setIcon(QMessageBox::Warning);
            box.setText(tr("This sync will DELETE data:\n"
                           "%1 item(s) on the other side, "
                           "%2 on this device.\n"
                           "Deleted items go to trash.\n\n"
                           "What to do?")
                        .arg(remoteDeletes).arg(localDeletes));

            QPushButton *continueButton=box.addButton(
                tr("Continue delete"), QMessageBox::YesRole);
            QPushButton *restoreButton=box.addButton(
                tr("Restore from the server instead "
                   "(my data goes to trash)"), QMessageBox::YesRole);
            QPushButton *cancelButton=box.addButton(
                tr("Cancel"), QMessageBox::RejectRole);
            box.setDefaultButton(cancelButton);
            box.exec();

            if(box.clickedButton()==continueButton)
                return SyncSession::DeleteDecision::Proceed;

            if(box.clickedButton()==restoreButton)
                return SyncSession::DeleteDecision::Restore;

            return SyncSession::DeleteDecision::Cancel;
        });

    // PIN пробрасываем и в сессию: ее починка пары — вторая линия обороны
    // (предсессионная выше обычно уже все решила).
    // Перед сессией сохраняем редактируемую запись: иначе текст не уедет
    // на хаб, а скачанная версия перетрет файл (нативный путь не делает
    // reloadSaveStage, в отличие от внешней команды).
    saveTextarea();

    syncDbg(QStringLiteral("gui: BEFORE session.run model=%1 scope=%2")
            .arg(syncPtr(treeScreen->knowTreeModel), scopeRootId));
    SyncReport report;

    // Фон/headless — без окна; интерактив — с прогрессом передачи
    if(isSyncHeadlessMode() || silentDecisions!=nullptr)
    {
        report=session.run(host, port, pin, scopeRootId);
    }
    else
    {
        // Окно прогресса: сессия блокирует GUI, без него окно «умирает»
        SyncProgressWindow progress(session, this);
        report=session.run(host, port, pin, scopeRootId);
    }
    syncDbg(QStringLiteral("gui: AFTER session.run ok=%1 err=%2 model=%3 root=%4")
            .arg(report.ok).arg(report.error)
            .arg(syncPtr(treeScreen->knowTreeModel))
            .arg(syncPtr(treeScreen->knowTreeModel->getRootItemPublic())));

    // Сессия уже сделала save+reload модели. Обновляем/закрываем
    // откреплённые окна, которые держали старые Record (иначе — мёртвые
    // указатели/устаревший текст), как это делает onSyncroCommandFinishWork.
    finishNativeSyncUi();

    // Последний результат по устройству — в окно устройств (и в фон:
    // таймер тоже проходит здесь). Пир известен из предсессионного hello.
    if(hello.ok && !hello.remoteDeviceId.isEmpty())
        mytetraConfig.set_syncDeviceLastResult(
            hello.remoteDeviceId, report.ok,
            report.ok
                ? tr("↑ notes %1, branches %2; ↓ notes %3, branches %4; deletes %5")
                      .arg(report.uploadedNotes).arg(report.uploadedBranches)
                      .arg(report.downloadedNotes).arg(report.downloadedBranches)
                      .arg(report.appliedDeletes)
                : report.error);

    // Headless-прогон: шаги сессии всегда в выводе, иначе зависший клиент
    // молчит и непонятно где встал. GUI показывает только summary
    const QString steps=session.log().join(QStringLiteral("\n"));

    if(!report.ok)
    {
        QString fail=tr("Sync failed: %1").arg(report.error);

        if(errorCode!=nullptr)
            *errorCode=report.errorCode;

        if(!steps.isEmpty())
            fail+=QStringLiteral("\n-- steps --\n")+steps;

        return fail;
    }

    // Срез по ветке пуст: предпросмотр не открывался (сессия вернулась
    // раньше), вместо сводки — короткий текст для information
    if(report.scopeEmpty)
    {
        ok=true;
        return tr("В этой ветке расхождений нет.");
    }

    QMap<QString, QString> logData;
    logData["upNotes"]=QString::number(report.uploadedNotes);
    logData["downNotes"]=QString::number(report.downloadedNotes);
    logData["upBranches"]=QString::number(report.uploadedBranches);
    logData["downBranches"]=QString::number(report.downloadedBranches);
    logData["appliedDeletes"]=QString::number(report.appliedDeletes);
    logData["conflicts"]=QString::number(report.conflicts.size());
    actionLogger.addAction("syncSession", logData);

    // Успех — короткая сводка. Шаги остаются в session.log()/консоли,
    // в окно их не тащим: сотни "pull branch ..." ничего не объясняют
    ok=true;
    return syncSuccessSummary(report, hello.remoteDeviceName);
}


void MainWindow::onSyncPairing(void)
{
    const QString unknownId=SyncHandshake::lastUnknownDeviceId();
    const QString unknownName=SyncHandshake::lastUnknownDeviceName();

    if(unknownId.isEmpty())
    {
        // Входящих нет — сводка состояния спаривания: кто я, с кем пара,
        // кто стучится. Короткие id (первые 8 символов), полные видно
        // в окне Sync devices
        QString info=tr("This device:\n%1\nFingerprint:\n%2\nGroup: %3")
            .arg(mytetraConfig.get_syncDeviceName(),
                 SyncIdentity::fingerprint(mytetraConfig.get_syncDeviceId()),
                 mytetraConfig.get_syncGroupId());

        const QMap<QString, QString> ring=mytetraConfig.get_syncSecretsRing();
        QStringList pairedLines;

        for(const AppConfig::SyncDeviceInfo &dev : mytetraConfig.get_syncDevices())
        {
            if(dev.id==mytetraConfig.get_syncDeviceId())
                continue;

            if(ring.contains(dev.id))
            {
                const QString label=dev.name.isEmpty() ? dev.id : dev.name;
                pairedLines << QStringLiteral("• %1 (%2…) — %3")
                    .arg(label, dev.id.left(8),
                         dev.lastMutual>0 ? tr("mutual")
                                          : tr("paired, waiting for accept"));
            }
            else if(dev.pairOffer)
            {
                const QString label=dev.name.isEmpty() ? dev.id : dev.name;
                pairedLines << QStringLiteral("• %1 (%2…) — %3")
                    .arg(label, dev.id.left(8), tr("pair requested"));
            }
        }

        if(pairedLines.isEmpty())
            info+=tr("\n\nPaired devices: none");
        else
            info+=tr("\n\nPaired devices (%1):\n%2")
                .arg(pairedLines.size()).arg(pairedLines.join(QStringLiteral("\n")));

        showSyncReport(this, tr("Sync pairing info"), info, false);
        return;
    }

    // Сервер: показать одноразовый код для входящего устройства.
    // PIN на сервере больше не вводится — клиент вводит код у себя
    QString unknownIp;

    for(const AppConfig::SyncDeviceInfo &info : mytetraConfig.get_syncDevices())
        if(info.id==unknownId)
        {
            unknownIp=info.lastIp;
            break;
        }

    QString qrHost=mytetraConfig.get_syncAdvertiseHost();

    if(qrHost.isEmpty())
        qrHost=SyncHandshake::lastLocalAddress();

    if(qrHost.isEmpty())
        qrHost=SyncTls::primaryAddress();

    if(qrHost.isEmpty())
        qrHost=QHostInfo::localHostName();

    QString qrFingerprint=mytetraConfig.get_syncTlsFingerprint();

    if(qrFingerprint.isEmpty()
       && !mytetraConfig.get_syncTlsCertPath().isEmpty())
        qrFingerprint=SyncTls::certificateFingerprint(
            mytetraConfig.get_syncTlsCertPath());

    const quint16 qrPort=syncHub!=nullptr ? syncHub->port() : syncDefaultPort;

    bool rejected=false;
    SyncPairDialogs::showCode(this, unknownId,
                              unknownName.isEmpty() ? unknownId : unknownName,
                              QDateTime::currentMSecsSinceEpoch(),
                              &rejected, unknownIp,
                              mytetraConfig.get_syncPairBindIp(),
                              qrHost, qrPort,
                              mytetraConfig.get_syncTlsEnabled(),
                              qrFingerprint,
                              mytetraConfig.get_syncDeviceName());

    if(rejected)
    {
        mytetraConfig.clear_syncDeviceOffer(unknownId);
        mytetraConfig.clear_syncDevicePendingSecret(unknownId);
    }
}


void MainWindow::onSyncDevices(void)
{
    SyncDevicesDialog dialog(treeScreen->knowTreeModel,
                             &mytetraConfig,
                             mytetraConfig.get_tetradir(),
                             syncHub!=nullptr ? syncHub->port()
                                              : syncDefaultPort,
                             this);
    dialog.exec();

    // Серверный чекбокс мог измениться только из своего пункта меню,
    // устройства — только учет. Синхронизировать нечего
}


// Диалог стратегии первого синка: данные есть с обеих сторон, пока
// ничего не слилось. Стирание — только в корзину

// Окно предпросмотра синка: категории с действиями, список элементов
// с точечными действиями, diff по требованию. false — отмена
bool MainWindow::askSyncPreview(const SyncPreview::Preview &preview,
                                SyncPreview::Decisions &decisions,
                                const QString &host, quint16 port,
                                const SyncSession::PartialSyncer &partial)
{
    // Свой текст — из модели; чужой — pull-note с хаба по требованию
    const auto ownText=[this](const SyncPreview::Item &item) -> QString {
        if(item.isBranch)
            return QString();

        const QJsonObject note=SyncNoteIo::serializeNote(
            treeScreen->knowTreeModel, mytetraConfig.get_tetradir(), item.id);

        return QString::fromUtf8(QByteArray::fromBase64(
            note.value(QStringLiteral("text")).toString().toLatin1()));
    };

    const auto otherText=[host, port](const SyncPreview::Item &item) -> QString {
        if(item.isBranch)
            return QString();

        SyncClient client(&mytetraConfig);
        const QJsonObject note=client.pullNote(host, port, item.id);

        return QString::fromUtf8(QByteArray::fromBase64(
            note.value(QStringLiteral("text")).toString().toLatin1()));
    };

    // Сохранённые «действия по умолчанию» (если есть) — стартовыми;
    // пункт «Сохранить как умолчание» в меню действий их обновляет.
    SyncPreview::Decisions initial=SyncPreview::Decisions::defaults();
    const QString saved=mytetraConfig.get_syncPreviewActions();

    if(!saved.isEmpty())
        initial=SyncPreview::defaultsFromString(saved);

    SyncPreviewDialog dialog(preview, initial, ownText, otherText, this);
    dialog.setPartialSyncer(partial);
    dialog.setSaveDefaults([this](const SyncPreview::Decisions &d) {
        mytetraConfig.set_syncPreviewActions(
            SyncPreview::serializeDefaults(d));
    });

    if(dialog.exec()!=QDialog::Accepted)
        return false;

    decisions=dialog.decisions();

    return true;
}


// Единое окно отчёта синка: текст можно выделить и скопировать
// (длинные списки ошибок/шагов бесполезны, если их нельзя переслать).
void MainWindow::showSyncReport(QWidget *parent, const QString &title,
                                const QString &text, bool isError)
{
    // Успех без протокольных шагов: это отладочный вывод, а не отчёт
    QString body=text;

    if(!isError)
    {
        const QString marker=QStringLiteral("\n-- steps --\n");
        const int at=body.indexOf(marker);

        if(at>=0)
            body=body.left(at);
    }

    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setIcon(isError ? QMessageBox::Warning : QMessageBox::Information);
    box.setText(body);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse
                                | Qt::TextSelectableByKeyboard);

    QPushButton *copy=box.addButton(tr("Copy"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);

    box.exec();

    if(box.clickedButton()==copy)
        QApplication::clipboard()->setText(body);
}


QString MainWindow::syncSuccessSummary(const SyncReport &report,
                                       const QString &peerName)
{
    QString text=peerName.isEmpty()
        ? tr("Synchronization complete.")
        : tr("Synchronization complete with %1.").arg(peerName);

    text+=QStringLiteral("\n\n")
        +tr("Received: %1 branches, %2 notes.\n"
            "Sent: %3 branches, %4 notes.\n"
            "Deleted: %5.")
             .arg(report.downloadedBranches).arg(report.downloadedNotes)
             .arg(report.uploadedBranches).arg(report.uploadedNotes)
             .arg(report.appliedDeletes);

    if(!report.conflicts.isEmpty())
        text+=tr("\n\nConflicts (kept this side, other side in backup):\n%1")
                 .arg(report.conflicts.join(QStringLiteral(", ")));

    if(!report.errors.isEmpty())
        text+=tr("\n\nSkipped items:\n%1")
                 .arg(report.errors.join(QStringLiteral("\n")));

    return text;
}


QString MainWindow::trimSyncReportForDialog(const QString &text)
{
    // Шаги — всё после маркера, ошибки — блок "Item errors" до него
    const QString marker=QStringLiteral("\n-- steps --\n");
    const int at=text.indexOf(marker);
    QString head=text, steps;

    if(at>=0)
    {
        head=text.left(at);
        steps=text.mid(at+marker.size());
    }

    // Ошибки — первые 50, дальше счётчик (их обычно единицы)
    {
        const QStringList lines=head.split(QLatin1Char('\n'));
        int errStart=-1;

        for(int i=0; i<lines.size(); ++i)
            if(lines.at(i).startsWith(QStringLiteral("Item errors")))
            {
                errStart=i+1;
                break;
            }

        if(errStart>=0)
        {
            const int errCount=lines.size()-errStart;

            if(errCount>50)
            {
                QStringList cut=lines.mid(0, errStart+50);
                cut << QStringLiteral("... and %1 more error(s), see full log in console "
                                      "with sync debugging enabled").arg(errCount-50);
                head=cut.join(QLatin1Char('\n'));
            }
        }
    }

    // Шаги: голова+хвост, середину в топку (там push note ×100500).
    // Полный текст остаётся в консоли при включённой отладке синка:
    // каждый шаг туда дублируется через syncDbg
    if(!steps.isEmpty())
    {
        const QStringList lines=steps.split(QLatin1Char('\n'));

        if(lines.size()>60)
        {
            QStringList cut=lines.mid(0, 15);
            cut << QStringLiteral("... [%1 lines omitted — full log in console "
                                  "with sync debugging enabled] ...").arg(lines.size()-55);
            cut+=lines.mid(lines.size()-40);
            steps=cut.join(QLatin1Char('\n'));
        }
    }

    return at>=0 ? head+marker+steps : head;
}


SyncStrategy MainWindow::askSyncStrategy(QWidget *parent,
                                         int localItems, int remoteItems)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(tr("Sync"));

    QVBoxLayout *layout=new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        tr("Both devices already have data.\n"
           "This device: %1 item(s), the other side: %2 item(s).\n\n"
           "How to sync?").arg(localItems).arg(remoteItems), &dialog));

    // Выбор стратегии — радиокнопки: кнопок слишком много,
    // текста в каждой длинный
    QRadioButton *merge=new QRadioButton(
        tr("Merge both trees"), &dialog);
    merge->setToolTip(tr("Both trees stay as they are. "
                         "If the same structure was recreated by hand, "
                         "duplicates are possible."));

    QRadioButton *mergeByName=new QRadioButton(
        tr("Merge same-named branches on the server"), &dialog);
    mergeByName->setToolTip(tr("Branches with equal names are merged into "
                               "the server's branches (server ids win), "
                               "same-named notes stay duplicated, "
                               "then my base becomes the merged result."));

    QRadioButton *pull=new QRadioButton(
        tr("Take from the server"), &dialog);
    pull->setToolTip(tr("My data goes to trash, "
                        "the server's base is downloaded in full."));

    QRadioButton *push=new QRadioButton(
        tr("Replace the server's with mine"), &dialog);
    push->setToolTip(tr("The server's data goes to trash, "
                        "my base is uploaded in full."));

    merge->setChecked(true);

    layout->addWidget(merge);
    layout->addWidget(mergeByName);
    layout->addWidget(pull);
    layout->addWidget(push);

    QDialogButtonBox *buttons=new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if(dialog.exec()!=QDialog::Accepted)
        return SyncStrategy::Cancel;

    if(mergeByName->isChecked())
        return SyncStrategy::MergeByName;

    if(pull->isChecked())
        return SyncStrategy::PullFromHub;

    if(push->isChecked())
        return SyncStrategy::PushToHub;

    return SyncStrategy::Merge;
}


// Попап входящего запроса на пару: устройство стучится с pairRequest.
// Принять — общий PIN (та же цепочка, что Accept в окне устройств),
// Отказать — снять запрос, Открыть окна — список устройств
// Запрет новых пар из меню: подтверждение и отмена незавершённых пар
void MainWindow::onSyncBlockPairingToggled(bool checked)
{
    if(checked)
    {
        if(QMessageBox::question(
               this, tr("Sync"),
               tr("Block new pairings?\n\n"
                  "Existing devices keep syncing. Any pairing in progress "
                  "will be cancelled (codes and pending secrets)."))
           !=QMessageBox::Yes)
        {
            const QSignalBlocker block(actionSyncMenuBlockPairing);
            actionSyncMenuBlockPairing->setChecked(false);
            return;
        }

        SyncPairing::clearAll();

        for(const AppConfig::SyncDeviceInfo &info : mytetraConfig.get_syncDevices())
            mytetraConfig.clear_syncDevicePendingSecret(info.id);
    }

    mytetraConfig.set_syncPairingBlocked(checked);
}


void MainWindow::onSyncPairRequest(const QString &deviceId, const QString &deviceName)
{
    // Headless: показывать некому, запрос и так виден в окне устройств
    if(isSyncHeadlessMode() || syncPairPopupActive)
        return;

    // Показываем отложенно: сигнал прилетает из обработчика hello сервера,
    // модальный диалог прямо там заблокировал бы отправку ответа клиенту
    // (тот висел бы до нажатия кнопки и упал бы по таймауту)
    QTimer::singleShot(0, this, [this, deviceId, deviceName]() {
        if(syncPairPopupActive)
            return;

        syncPairPopupActive=true;
        QString shownName=deviceName.isEmpty() ? deviceId : deviceName;
        QString peerIp;

        // IP помогает отличить «то самое» устройство в списке
        for(const AppConfig::SyncDeviceInfo &info : mytetraConfig.get_syncDevices())
            if(info.id==deviceId && !info.lastIp.isEmpty())
            {
                peerIp=info.lastIp;
                shownName+=QStringLiteral(" (%1)").arg(info.lastIp);
                break;
            }

        // Код показываем на сервере; клиент вводит его у себя.
        // Код живёт 5 минут и одноразовый — PIN на сервере больше не вводим
        QString qrHost=mytetraConfig.get_syncAdvertiseHost();

        if(qrHost.isEmpty())
            qrHost=SyncHandshake::lastLocalAddress();

        if(qrHost.isEmpty())
            qrHost=SyncTls::primaryAddress();

        if(qrHost.isEmpty())
            qrHost=QHostInfo::localHostName();

        QString qrFingerprint=mytetraConfig.get_syncTlsFingerprint();

        if(qrFingerprint.isEmpty()
           && !mytetraConfig.get_syncTlsCertPath().isEmpty())
            qrFingerprint=SyncTls::certificateFingerprint(
                mytetraConfig.get_syncTlsCertPath());

        const quint16 qrPort=syncHub!=nullptr ? syncHub->port()
                                              : syncDefaultPort;

        bool rejected=false;
        SyncPairDialogs::showCode(this, deviceId, shownName,
                                  QDateTime::currentMSecsSinceEpoch(),
                                  &rejected, peerIp,
                                  mytetraConfig.get_syncPairBindIp(),
                                  qrHost, qrPort,
                                  mytetraConfig.get_syncTlsEnabled(),
                                  qrFingerprint,
                                  mytetraConfig.get_syncDeviceName());

        if(rejected)
        {
            mytetraConfig.clear_syncDeviceOffer(deviceId);
            mytetraConfig.clear_syncDevicePendingSecret(deviceId);
            statusBar->showMessage(tr("Pair request from %1 rejected")
                                   .arg(shownName), 5000);
        }
        else
        {
            statusBar->showMessage(
                tr("Code shown for %1; enter it on that device")
                    .arg(shownName), 8000);
        }

        syncPairPopupActive=false;
    });
}

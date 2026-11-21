#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtXml>
#include <QtGui>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSystemTrayIcon>


class QAction;
class QWidget;
class QMenu;
class QCloseEvent;
class QSplitter;
class QStatusBar;

class TreeScreen;
class MetaEditor;
class RecordTableScreen;
class FindScreen;
class WindowSwitcher;
class CommandRunner;
class SyncHub;
#include "libraries/sync/SyncSession.h"
#include "libraries/sync/SyncPreview.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    virtual ~MainWindow();

    TreeScreen *treeScreen=nullptr;
    RecordTableScreen *recordTableScreen=nullptr;
    MetaEditor *editorScreen=nullptr;
    FindScreen *findScreenDisp=nullptr;
    QStatusBar *statusBar=nullptr;
    WindowSwitcher *windowSwitcher=nullptr;

    void restoreWindowGeometry(void);
    void restoreTreePosition(void);
    void restoreRecordTablePosition(void);
    void restoreEditorCursorPosition(void);
    void restoreEditorScrollBarPosition(void);
    void restoreFindOnBaseVisible(void);
    void restoreAllWindowState(void);

    void restoreDockableWindowsState(void);

    void setTreePosition(QStringList path);
    bool isTreePositionCrypt();

    void setRecordtablePositionById(QString id);

    //! Вспомогательный метод, который комплексно устанавливает
    //! путь в дереве и запись в таблице записей
    void setTreeAndRecordtablePositions(QStringList treePath, QString recordId);

    void synchronization(bool visible=true);

    // Кнопка синхронизации в нативном режиме: одна взаимная пара —
    // синк сразу, несколько — меню выбора устройства, ноль — окно
    // устройств (там спаривание). interactive=false (таймер) меню
    // и окна не показывает: неоднозначный выбор молча пропускается
    void runToolbarNativeSync(bool interactive);

    // Фоновый тик таймера в нативе: молча, только безопасное, по одной
    // паре за тик (round-robin), не чаще раза в 15 минут
    void backgroundSyncTick(void);

    void goWalkHistoryPrevious(void);
    void goWalkHistoryNext(void);

    void saveTextarea(void);

    // После нативного синка: обновить/закрыть откреплённые окна,
    // которые держали старые Record (сессия уже сделала reload модели)
    void finishNativeSyncUi(void);

    // Занятость нативного синка для фонового таймера (окно устройств
    // не проходит через runToolbarNativeSync и раньше не выставляло флаг)
    void setNativeSyncBusy(bool busy);

    void saveAllState(void);

    void reload(void);

signals:

    void globalPressKey(int key);
    void globalReleaseKey(int key);

    void doUpdateDetachedWindows();

public slots:
    void applicationExit(void);
    void applicationFastExit(void);
    void commitData(QSessionManager& manager);
    void messageHandler(QString message);

    void toolsFindInBase(void);

    void setupShortcuts(void);

private slots:

    void showWindow();

    bool fileSave(void);
    bool fileSaveAs(void);

    void fileDatabasesManagement(void);
    void fileExportBranch(void);
    void fileImportBranch(void);

    void filePrint(void);
    void filePrintPreview(void);
    void filePrintPdf(void);

    void toolsPreferences(void);
    void onActionLogClicked(void);

    void onSyncServerToggle(bool checked);
    void onSyncPairing(void);
    void onSyncDevices(void);

    // Попап входящего запроса на пару: «такой-то девайс хочет pair»
    // с вариантами Принять / Отказать / Окно устройств
    void onSyncPairRequest(const QString &deviceId, const QString &deviceName);


    // Headless sync-режим определяется флагами запуска, а не состоянием:
    // в нем нет дисплея, поэтому рестор и сейв UI-позиции пропускаются
    // (иначе клик по шифроветке = модальный пароль = вечный вис)
    static bool isSyncHeadlessMode(void);

public:

    // Headless-управление сервером (для --sync-server и тестов).
    // Порт по умолчанию 47777
    bool startSyncServer(quint16 port=47777, const QString &pairingPin=QString());
    void stopSyncServer(void);
    bool isSyncServerRunning(void) const;

    // Headless-синк: без диалогов. pin — для первого спаривания
    // (GUI-путь сверяет фингерпринт, здесь — осознанно нет).
    // scopeRootId — синк только поддерева ветки (пусто = весь синк).
    // Возвращает текст отчета, ok=false при ошибке.
    // errorCode — код handshake наружу (foreign-network для диалога),
    // опционален чтобы не трогать старые вызовы.
    // silentDecisions — тихий фон: вместо окна выбора применяются готовые
    // решения (только создания; остальное Skip до ручного синка).
    // silentPush/silentPull — разрешённые направления тихих созданий
    // (выключенное направление тоже уходит в Skip)
    QString runSyncToHeadless(const QString &host,
                              const QString &pin,
                              quint16 port,
                              bool &ok,
                              QString *errorCode=nullptr,
                              const QString &scopeRootId=QString(),
                              const SyncPreview::Decisions *silentDecisions=nullptr,
                              bool silentPush=true,
                              bool silentPull=true);

    // Диалог стратегии первого синка (данные есть с обеих сторон):
    // слить как есть / взять с сервера / заменить серверные / отмена.
    // Общий для runSyncToHeadless и окна Sync devices
    static SyncStrategy askSyncStrategy(QWidget *parent,
                                        int localItems, int remoteItems);

    // Длинный отчёт синка (100k+ строк шагов) в окно диалога не влезает:
    // шаги режем до головы+хвоста, ошибки — первые 50. Полный текст
    // остаётся в консоли при включённой отладке синка (там же все шаги
    // дублируются через syncDbg). Headless-путь печатает целиком
    static QString trimSyncReportForDialog(const QString &text);

    // Окно отчёта с выделяемым текстом и кнопкой «Копировать»
    static void showSyncReport(QWidget *parent, const QString &title,
                               const QString &text, bool isError);

    // Окно предпросмотра синка (категории с действиями + diff).
    // Заполняет decisions выбранными действиями, false — отмена.
    // partial — частичное «Применить» из окна (сессия режет план по id
    // тем же выполнением, окно при этом не закрывается)
    bool askSyncPreview(const SyncPreview::Preview &preview,
                        SyncPreview::Decisions &decisions,
                        const QString &host, quint16 port,
                        const SyncSession::PartialSyncer &partial);

    void onExpandEditArea(bool flag);

    void onClickHelpAboutMyTetra(void);
    void onClickHelpAboutQt(void);
    void onClickHelpTechnicalInfo(void);

    void onClickFocusTree(void);
    void onClickFocusNoteTable(void);
    void onClickFocusEditor(void);

    void runDirectPreferences(QAction *action);

    void onSyncroCommandFinishWork(void);

    void iconActivated(QSystemTrayIcon::ActivationReason reason);

    void onFocusChanged(QWidget *, QWidget *);

private:

    void setupUI(void);
    void setupSignals(void);
    void assembly(void);

    void initFileMenu(void);
    void initToolsMenu(void);
    void initSyncMenu(void);
    void initPreferencesMenu(QMenu *menu);
    void initHelpMenu(void);
    void initHiddenActions(void);

    void initRecordTableActions(void);

    void setupIconActions(void);
    void createTrayIcon(void);
    void setIcon(void);

    void saveWindowGeometry(void);
    void saveTreePosition(void);
    void saveRecordTablePosition(void);
    void saveEditorCursorPosition(void);
    void saveEditorScrollBarPosition(void);

    void reloadSaveStage(void);
    void reloadLoadStage(bool isLongTimeReload);


    QAction *actionFileMenuDatabasesManagement;
    QAction *actionFileMenuExportTreeItem;
    QAction *actionFileMenuImportTreeItem;
    QAction *actionFileMenuPrint;
    QAction *actionFileMenuPrintPreview;
    QAction *actionFileMenuExportPdf;
    QAction *actionFileMenuQuit;

    QAction *actionToolsMenuFindInBase;
    QAction *actionToolsMenuActionLog;
    QAction *actionToolsMenuPreferences; // Вызов окна настроек, используется в десктопе
    // Отдельное меню Sync (не Tools): подсистема синхронизации
    QAction *actionSyncMenuSyncServer=nullptr;
    QAction *actionSyncMenuSyncNow=nullptr;
    QAction *actionSyncMenuSyncPairing=nullptr;
    QAction *actionSyncMenuSyncDevices=nullptr;

    // Синхронизация: хаб живет пока включен сервер из меню Tools
    SyncHub *syncHub=nullptr;

    // PIN, с которым создан текущий хаб: при повторном startSyncServer
    // с другим PIN/портом его надо пересоздать, а не молча оставить старый
    QString m_syncHubPin;
    static const quint16 syncDefaultPort;

    // Попап pair-запроса уже показан (не плодим окна поверх друг друга)
    bool syncPairPopupActive=false;

    // Нативный синк уже идёт (кнопка модальна, а таймер — нет):
    // повторный вход пропускается, журнал один
    bool m_nativeSyncBusy=false;
    // Последний фоновый тик (мс): чаще раза в 15 минут не синкаем
    qint64 m_bgLastTickMs=0;
    // Кого синкать следующим фоновым тиком (round-robin по парам)
    int m_bgNextPeer=0;

    // Напрямую вызываемые настройки, используются в мобильном интерфейсе
    QAction *actionDirectPreferencesMain       =nullptr;
    QAction *actionDirectPreferencesAppearance =nullptr;
    QAction *actionDirectPreferencesCrypt      =nullptr;
    QAction *actionDirectPreferencesSyncro     =nullptr;
    QAction *actionDirectPreferencesRecordTable=nullptr;
    QAction *actionDirectPreferencesAttach     =nullptr;
    QAction *actionDirectPreferencesKeyboard   =nullptr;
    QAction *actionDirectPreferencesHistory    =nullptr;
    QAction *actionDirectPreferencesMisc       =nullptr;

    QAction *actionHelpMenuAboutMyTetra;
    QAction *actionHelpMenuAboutQt;
    QAction *actionHelpMenuTechnicalInfo;

    QAction *actionTrayRestore;
    QAction *actionTrayMaximize;
    QAction *actionTrayMinimize;
    QAction *actionTrayQuit;

    QAction *actionFocusTree;
    QAction *actionFocusNoteTable;
    QAction *actionFocusEditor;

    QSystemTrayIcon *trayIcon;
    QMenu           *trayIconMenu;

    QSplitter *vSplitter;
    QSplitter *hSplitter;
    QSplitter *findSplitter;

    CommandRunner *syncroCommandRun=nullptr;


protected:

    void closeEvent(QCloseEvent *event);

    bool eventFilter( QObject * o, QEvent * e ); // Отслеживание прочих событий

    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);

    void goWalkHistory(void);

    bool enableRealClose;
    int exitCounter=0;

};
#endif

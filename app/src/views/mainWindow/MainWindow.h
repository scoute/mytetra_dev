#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QtXml>
#include <QtGui>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSystemTrayIcon>

#include "libraries/Clipper.h"
#include "views/mcpHistory/McpHistoryDialog.h"


class QAction;
class QWidget;
class QMenu;
class QCloseEvent;
class QSplitter;
class QStatusBar;
class QLabel;
class QTimer;
class QTableWidget;
class QDockWidget;

class TreeScreen;
class MetaEditor;
class RecordTableScreen;
class FindScreen;
class HistoryPanel;
class BacklinksPanel;
class TagsPanel;
class WindowSwitcher;
class CommandRunner;

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

    // Статистика базы справа в статусбаре (постоянный виджет)
    QLabel *statsLabel=nullptr;

    // Позиция курсора заметки. Обычный (не постоянный) виджет статусбара:
    // важные showMessage-сообщения его перекрывают
    QLabel *cursorLabel=nullptr;

    // Полоса MCP-сессии сверху: крупно, немодально, без кнопки.
    // Висит всё время пока жив хоть один MCP-пир (Hello/Bye + TTL),
    // скрыть или подтвердить нельзя. Инлайн-стиль (не тема):
    // одинаково заметна в светлой и тёмной теме
    QWidget *mcpBannerBar=nullptr;
    QLabel *mcpBanner=nullptr;
    QTimer *mcpBannerTimer=nullptr;

    // Живые MCP-пиры: pid -> последнее Hello (мсек). Пусто = сессия кончилась
    QMap<QString, qint64> mcpPeers;

    // Умное автосохранение: single-shot цепочка, интервал перечитывается
    // из конфига при каждом срабатывании. Только при реальных правках:
    // без изменений ни файла ни коммита. Модальный диалог (превью
    // отката и прочие) сохранение на этот круг пропускает
    QTimer *autosaveTimer=nullptr;

    // Режимы пиров: pid -> флаг (--mcp-ro и т.п.). Показывается в баннере
    QMap<QString, QString> mcpPeerModes;

    // История действий сессии: только в памяти, на диск не пишется,
    // при дисконнекте чистится. Окно показывается на старт сессии
    QList<McpHistoryEntry> mcpHistory;
    McpHistoryDialog *mcpHistoryDialog=nullptr;
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

    void goWalkHistoryPrevious(void);
    void goWalkHistoryNext(void);

    void saveTextarea(void);

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

    void toolsFindInBaseWithText(const QString &text);

    void toolsImagesGallery(void);
    void toolsFilesGallery(void);

    // Показ всплывающего сообщения в системном трее.
    // Молча ничего не делает, если трей недоступен или скрыт
    void showTrayMessage(const QString &title, const QString &text);

    // Веб-клиппер: вставка из буфера в unsorted_notes сейчас
    void runClipperNow(void);

    // Доступ к клипперу для настроек (статус хоткея)
    Clipper *getClipper(void);

    void setupShortcuts(void);

    // Пересчитать и показать статистику базы в статусбаре.
    // Вызывается по сигналам модели
    void updateStats(void);

    // Подтверждение внешней MCP-правки: баннер и так висит всю
    // сессию, здесь только пересчёт видимости и блокировки
    void flashMcpWrite(void);

    // Текст баннера: сессия активна + режимы пиров
    QString mcpBannerText(void);

    // Пересчитать баннер и блокировку по пирам (с TTL-чисткой):
    // пиры есть — баннер виден и UI заблокирован, нет — всё снято
    void updateMcpBanner(void);

    // Срабатывание умного автосохранения
    void onAutosaveTimeout(void);

    // Перезапустить цепочку автосохранения с текущим конфигом.
    // Выключено или интервал <1: таймер стоит
    void restartAutosaveTimer(void);

    // Блокировка UI на время MCP-сессии: фан-аут read-only поверх,
    // своё досохраняется до блокировки. MCP-записи не трогает.
    // Идемпотентна: повторный вызов с тем же состоянием ничего не делает
    void setMcpUiLock(bool locked);

    // Накопить действие в историю сессии (с обрезкой) и обновить окно
    void addMcpHistoryEntry(const QString &kind,
                            const QString &tool,
                            const QString &summary);

    // Передать окну текущий список (с его фильтром)
    void updateMcpHistoryDialog(void);

    // Таблица разбивки статистики для проверки без модального окна
    QTableWidget *buildStatsTableWidget(QWidget *parent);

    // Диалог разбивки статистики по заметкам по клику на строку
    void showStatsDetails(void);

    // Перевести статичные тексты новым языком без перезапуска.
    // Тексты на шорткатах обновляются отдельно сигналом
    // updateWidgetShortcut, порядок вызывает applyLanguage
    void retranslateUi(void);

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

    // Клик по пункту меню Темы: переключить интерфейс на выбранную тему
    void onThemeMenuTriggered(QAction *action);

    // Меню Вид: дубли быстрых переключателей отображения
    void onViewHeaderToggled(bool checked);
    void onViewFavoritesToggled(bool checked);
    void onViewSecretColor(void);
    void onViewPaperColor(void);

    // Меню Вид: глобальный режим только чтения
    void onViewReadOnlyToggled(bool checked);
    void applyReadOnly(void);

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
    void initPreferencesMenu(QMenu *menu);
    void initHelpMenu(void);
    void initHiddenActions(void);

    // Меню быстрого переключения темы и пометка в нем текущей темы.
    // Пометка обновляется при каждом открытии меню, так как тему можно
    // сменить и из диалога настроек
    void initThemesMenu(void);
    void syncThemeMenu(void);

    // Меню Вид между Tools и Themes и пометка в нем текущих состояний.
    // Пометка обновляется при каждом открытии, так как все меняется
    // и из других мест (диалог настроек, контекстное меню)
    void initViewMenu(void);
    void syncViewMenu(void);

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
    QAction *actionToolsMenuImagesGallery;
    QAction *actionToolsMenuFilesGallery;
    QAction *actionToolsMenuActionLog;
    QAction *actionToolsMenuClipFromClipboard;
    QAction *actionToolsMenuPreferences; // Вызов окна настроек, используется в десктопе

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

    // Меню Темы в menubar: один клик вместо похода в настройки
    QMenu *themesMenu=nullptr;

    // Меню Вид в menubar между Tools и Themes: шапка, избранное,
    // цвет секрета, бумага
    QMenu *viewMenu=nullptr;
    QAction *viewHeaderAction=nullptr;
    QAction *viewFavoritesAction=nullptr;
    QAction *viewReadOnlyAction=nullptr;
    QAction *viewSecretColorAction=nullptr;
    QAction *viewPaperColorAction=nullptr;

    // Корневые меню: указатели нужны для перевода названий на лету
    QMenu *fileMenu=nullptr;
    QMenu *toolsMenu=nullptr;
    QMenu *helpMenu=nullptr;
    QMenu *preferencesSubMenu=nullptr;

    // Панель истории посещений. Переключается из меню Tools и горячей
    // клавишей, в закрытом виде место не занимает
    HistoryPanel *historyPanel=nullptr;
    QDockWidget *historyPanelDock=nullptr;

    // Панель входящих ссылок. Переключается из меню Tools,
    // в закрытом виде место не занимает
    BacklinksPanel *backlinksPanel=nullptr;
    QDockWidget *backlinksPanelDock=nullptr;

    // Панель меток. Переключается из меню Tools и горячей
    // клавишей, в закрытом виде место не занимает
    TagsPanel *tagsPanel=nullptr;
    QDockWidget *tagsPanelDock=nullptr;

    QSystemTrayIcon *trayIcon;
    QMenu           *trayIconMenu;

    // Веб-клиппер (глобальный хоткей -> unsorted_notes)
    Clipper clipper;

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

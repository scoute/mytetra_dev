#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QJsonObject>
#include <QByteArray>
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
class TrayManager;
class MenuBuilder;
class McpGuiBridge;

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


    // Умное автосохранение: single-shot цепочка, интервал перечитывается
    // из конфига при каждом срабатывании. Только при реальных правках:
    // без изменений ни файла ни коммита. Модальный диалог (превью
    // отката и прочие) сохранение на этот круг пропускает
    QTimer *autosaveTimer=nullptr;


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
    //! (в слотах чтобы клиппер мог прыгать к дублям через invokeMethod
    //! без линка MainWindow)
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

    // Синхронизация пометок меню Вид: слот чтобы дергать
    // из проб и внешних мест (aboutToShow зовет сам)
    void syncViewMenu(void);

    // Синхронный ответ на mcp-команду: фасад над McpGuiBridge
    // (дергает сокетный респондер из main). P3: тело уехало в мост
    QByteArray handleMcpMessage(const QString &message);

    // P3: слот старта сессии от моста: обновить диалог и показать

    void toolsFindInBase(void);

    void filePrintPreview(void);

    // Перестановка списка записей: справа от дерева либо под деревом
    // в левой колонке. Вызывается при старте и вживую из настроек
    void applyRecordTablePosition(void);

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

    // Пересчитать баннер и блокировку по пирам:
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

    // Передать окну текущий список (с его фильтром)
    void updateMcpHistoryDialog(void);

    // P3: первая Hello-сессия от моста: обновить диалог и показать.
    // Раньше было инлайн в mcpHello-ветке handleMcpCommand
    void onMcpSessionStarted(void);

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
    void fileImportTelegram(void);

    void filePrint(void);
    void filePrintPdf(void);

    void toolsPreferences(void);
    void onActionLogClicked(void);

    // Клик по пункту меню Темы: переключить интерфейс на выбранную тему
    void onThemeMenuTriggered(QAction *action);

    // Пункт «Настроить тему»: то же окно, что страница Themes в настройках
    void onThemeConfigure(void);

    // Меню Вид: дубли быстрых переключателей отображения
    void onViewHeaderToggled(bool checked);
    void onViewFavoritesToggled(bool checked);
    void onViewTreeFilterToggled(bool checked);
    void onViewToolsToggled(bool checked);
    void onViewHtmlSourceToggled(bool checked);
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

    // P3: клик по иконке обрабатывает TrayManager::onIconActivated

    void onFocusChanged(QWidget *, QWidget *);

private:

    void setupUI(void);
    void setupSignals(void);
    void assembly(void);

    void initHiddenActions(void);

    // Меню быстрого переключения темы и пометка в нем текущей темы.
    // Пометка обновляется при каждом открытии меню, так как тему можно
    // сменить и из диалога настроек

    // Меню Вид между Tools и Themes и пометка в нем текущих состояний.
    // Пометка обновляется при каждом открытии, так как все меняется
    // и из других мест (диалог настроек, контекстное меню).
    // Слот чтобы дергать из проб и внешних мест

    void initRecordTableActions(void);

    // P3: трей живет в TrayManager (DI через QWidget-хост).
    // Здесь только владение и два фасада ниже

    // Отложенное применение размеров левой колонки после её показа
    void applyRecordTableSizes(void);

    void saveWindowGeometry(void);
    void saveTreePosition(void);
    void saveRecordTablePosition(void);
    void saveEditorCursorPosition(void);
    void saveEditorScrollBarPosition(void);

    void reloadSaveStage(void);
    void reloadLoadStage(bool isLongTimeReload);



    // P3: действия трея переехали в TrayManager

    QAction *actionFocusTree;
    QAction *actionFocusNoteTable;
    QAction *actionFocusEditor;

    // P3: меню и их пункты переехали в MenuBuilder
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

    TrayManager *trayManager=nullptr;
    MenuBuilder *menuBuilder=nullptr;

    // P3: мост MCP-сервер -> живой GUI (модель, пиры, история, баннер).
    // MainWindow держит виджеты/диалог/таймер и рендерит по сигналам
    McpGuiBridge *mcpBridge=nullptr;

    // Веб-клиппер (глобальный хоткей -> unsorted_notes)
    Clipper clipper;

    QSplitter *vSplitter;
    QSplitter *hSplitter;
    QSplitter *findSplitter;

    // Левая колонка (дерево над списком записей), существует только
    // в режиме recordTableUnderTree; иначе список в правой колонке
    QSplitter *leftColumnSplitter=nullptr;

    // Счётчик повторов отложенного выставления размеров левой колонки
    int recordTableSizesRetry=0;

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

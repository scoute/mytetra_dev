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
#include "TrayManager.h"
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
#include "libraries/AppContext.h"
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

// Показ всплывающего сообщения в системном трее.
// Используется фоновыми задачами (клиппер), которым некого спросить,
// но надо сообщить пользователю о результате

// Показ всплывающего сообщения в системном трее.
// P3: фасад над TrayManager (вызывает и Clipper через invokeMethod)
void MainWindow::showTrayMessage(const QString &title, const QString &text)
{
    trayManager->showMessage(title, text);
}

void MainWindow::showWindow()
{
    activateWindow();
    showNormal();
    raise();

    // Восстанавливается поведение открепляемых окон
    EditorShowTextDispatcher::instance()->restoreBehavior();
}

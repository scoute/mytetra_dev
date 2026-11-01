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

#include "MainWindow.h"
#include "TrayManager.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"

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
    AppContext::instance().dispatcher()->restoreBehavior();
}

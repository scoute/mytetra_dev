// TrayManager стенд: DI против голого QWidget, без MainWindow.
// Офскрин: трея нет, проверяются wiring сигналов и отсутствие падений.
#include <QApplication>
#include <QWidget>
#include <QDebug>
#include <QAction>
#include <QList>

#include "views/mainWindow/TrayManager.h"

static int fails=0;
static void check(bool ok, const char *name)
{
    qDebug() << (ok ? "PASS" : "FAIL") << name;
    if(!ok)
        ++fails;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWidget host;
    host.setObjectName(QStringLiteral("host"));

    TrayManager tray(&host, &app);
    check(tray.parent()==&app, "TRAY-PARENT");

    // Хост как QWidget: DI без MainWindow
    tray.setupActions();
    QList<QAction*> acts=tray.findChildren<QAction*>();
    check(acts.size()==4, "TRAY-ACTIONS");

    tray.createTrayIcon();
    tray.setIcon();
    check(true, "TRAY-CREATE-NOCRASH");

    tray.retranslate();
    check(true, "TRAY-RETRANSLATE-NOCRASH");

    tray.show();
    check(tray.isVisible()==false, "TRAY-NOTVISIBLE-OFFSCREEN");

    tray.showMessage(QStringLiteral("t"), QStringLiteral("x"));
    check(true, "TRAY-MSG-NOCRASH");

    // Сигналы через действия (работают и без трея)
    bool restored=false;
    bool quit=false;
    QObject::connect(&tray, &TrayManager::restoreRequested, [&](){ restored=true; });
    QObject::connect(&tray, &TrayManager::quitRequested, [&](){ quit=true; });

    for(QAction *a : acts)
    {
        if(a->text().contains(QStringLiteral("Restore")) || a->text().contains(QStringLiteral("Показать")))
            a->trigger();
        if(a->text()==QStringLiteral("&Quit") || a->text()==QStringLiteral("Выход"))
            a->trigger();
    }
    check(restored, "TRAY-RESTORE-SIGNAL");
    check(quit, "TRAY-QUIT-SIGNAL");

    qDebug() << (fails==0 ? "TRAYPROBE-OK" : "TRAYPROBE-BAD");
    return fails;
}

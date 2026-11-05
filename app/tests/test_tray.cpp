// TrayManager на QTest (см. 524).
// Порт trayprobe: DI против голого QWidget, без MainWindow.
// Офскрин: трея нет, проверяются wiring сигналов и отсутствие падений.
#include <QtTest>
#include <QWidget>
#include <QAction>
#include <QList>

#include "views/mainWindow/TrayManager.h"

class TestTray : public QObject
{
    Q_OBJECT

private slots:

    void wiringNoCrash(void);
    void signalsViaActions(void);
};

void TestTray::wiringNoCrash(void)
{
    QWidget host;
    host.setObjectName(QStringLiteral("host"));

    TrayManager tray(&host, qApp);
    QCOMPARE(tray.parent(), qApp);

    // Хост как QWidget: DI без MainWindow
    tray.setupActions();
    QList<QAction*> acts=tray.findChildren<QAction*>();
    QCOMPARE(acts.size(), 4);

    tray.createTrayIcon();
    tray.setIcon();
    tray.retranslate();

    tray.show();
    QCOMPARE(tray.isVisible(), false);

    tray.showMessage(QStringLiteral("t"), QStringLiteral("x"));
}

void TestTray::signalsViaActions(void)
{
    QWidget host;
    TrayManager tray(&host, qApp);
    tray.setupActions();
    QList<QAction*> acts=tray.findChildren<QAction*>();

    // Сигналы через действия (работают и без трея)
    bool restored=false;
    bool quit=false;
    connect(&tray, &TrayManager::restoreRequested, [&](){ restored=true; });
    connect(&tray, &TrayManager::quitRequested, [&](){ quit=true; });

    for(QAction *a : acts)
    {
        if(a->text().contains(QStringLiteral("Restore")) || a->text().contains(QStringLiteral("Показать")))
            a->trigger();
        if(a->text()==QStringLiteral("&Quit") || a->text()==QStringLiteral("Выход"))
            a->trigger();
    }
    QVERIFY2(restored, "restore signal must fire");
    QVERIFY2(quit, "quit signal must fire");
}

QTEST_MAIN(TestTray)

#include "test_tray.moc"

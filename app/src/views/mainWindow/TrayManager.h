#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>

// Менеджер иконки системного трея. Первое классовое выделение из
// MainWindow-god (P3): вместо 6 мемберов и 6 методов в главном окне -
// отдельный QObject с DI. Хост передается как QWidget (не MainWindow),
// чтобы не тянуть циклодependence: менеджеру нужны только
// isVisible/isMinimized/hide/showMaximized главного окна.
// Действия трея:
// - Restore/Maximize/Minimize дергают хост напрямую или сигналом
// - Quit идет сигналом quitRequested в MainWindow::applicationExit
//   (там сейв и аккуратный выход, менеджер про это не знает)

class QMenu;
class QAction;

class TrayManager : public QObject
{
    Q_OBJECT

public:

    explicit TrayManager(QWidget *host, QObject *parent=nullptr);
    ~TrayManager(void) override;

    // Построить действия меню трея. Вызывает MainWindow один раз
    void setupActions(void);

    // Создать иконку и меню. Безопасно при отсутствии трея:
    // проверки QSystemTrayIcon::isSystemTrayAvailable внутри
    void createTrayIcon(void);

    // Повесить connect activated и поставить иконку oknу и трею
    void setIcon(void);

    // Показать иконку если трей доступен
    void show(void);

    // Видна ли иконка (для closeEvent: прятать окно вместо выхода)
    bool isVisible(void) const;

    // Всплывающее сообщение. Молча ничего не делает если трей
    // недоступен или иконка скрыта (как было в MainWindow)
    void showMessage(const QString &title, const QString &text);

    // Переперевести пункты меню (вызывается из retranslateUi)
    void retranslate(void);

signals:

    // Просьба показать окно (клик по иконке, Restore).
    // MainWindow связывает с showWindow (там еще restoreBehavior
    // открепляемых окон, менеджер про это не знает)
    void restoreRequested(void);

    // Просьба выйти (пункт Quit).
    // MainWindow связывает с applicationExit (там сейв, менеджер
    // про выход ничего не знает)
    void quitRequested(void);

public slots:

    // Клик по иконке: видимое сворачивает/прячет, невидимое
    // просит показать. Логика один в один из MainWindow
    void onIconActivated(QSystemTrayIcon::ActivationReason reason);

private:

    QWidget *m_host=nullptr;

    QSystemTrayIcon *m_trayIcon=nullptr;
    QMenu *m_trayMenu=nullptr;

    QAction *m_actionRestore=nullptr;
    QAction *m_actionMaximize=nullptr;
    QAction *m_actionMinimize=nullptr;
    QAction *m_actionQuit=nullptr;
};

#endif // TRAYMANAGER_H

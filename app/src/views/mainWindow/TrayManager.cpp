#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QIcon>
#include <QDebug>

#include "TrayManager.h"


TrayManager::TrayManager(QWidget *host, QObject *parent) : QObject(parent),
    m_host(host)
{

}


TrayManager::~TrayManager(void)
{

}


void TrayManager::setupActions(void)
{
    // Родитель действий - менеджер (переедет с ним, а не с окном)
    m_actionRestore=new QAction(tr("&Restore window"), this);
    connect(m_actionRestore, &QAction::triggered,
            this, &TrayManager::restoreRequested);

    m_actionMaximize=new QAction(tr("Ma&ximize window"), this);
    connect(m_actionMaximize, &QAction::triggered,
            m_host, &QWidget::showMaximized);

    m_actionMinimize=new QAction(tr("Mi&nimize window"), this);
    connect(m_actionMinimize, &QAction::triggered,
            m_host, &QWidget::hide);

    m_actionQuit=new QAction(tr("&Quit"), this);
    connect(m_actionQuit, &QAction::triggered,
            this, &TrayManager::quitRequested);
}


void TrayManager::createTrayIcon(void)
{
    m_trayMenu=new QMenu(m_host);
    m_trayMenu->addAction(m_actionRestore);
    m_trayMenu->addAction(m_actionMaximize);
    m_trayMenu->addAction(m_actionMinimize);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_actionQuit);

    m_trayIcon=new QSystemTrayIcon(m_host);
    m_trayIcon->setContextMenu(m_trayMenu);
}


void TrayManager::setIcon(void)
{
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &TrayManager::onIconActivated);

    QIcon icon=QIcon(":/resource/pic/logo.svg");
    m_trayIcon->setIcon(icon);
    m_host->setWindowIcon(icon);
}


void TrayManager::show(void)
{
    if(QSystemTrayIcon::isSystemTrayAvailable()==false)
        return;

    m_trayIcon->show();
}


bool TrayManager::isVisible(void) const
{
    if(m_trayIcon==nullptr)
        return false;

    return m_trayIcon->isVisible();
}


void TrayManager::showMessage(const QString &title, const QString &text)
{
    if(QSystemTrayIcon::isSystemTrayAvailable()==false)
        return;

    if(m_trayIcon==nullptr || m_trayIcon->isVisible()==false)
        return;

    m_trayIcon->showMessage(title, text, QSystemTrayIcon::Warning);
}


void TrayManager::retranslate(void)
{
    m_actionRestore->setText(tr("&Restore window"));
    m_actionMaximize->setText(tr("Ma&ximize window"));
    m_actionMinimize->setText(tr("Mi&nimize window"));
    m_actionQuit->setText(tr("&Quit"));
}


void TrayManager::onIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(QSystemTrayIcon::isSystemTrayAvailable()==false)
        return;

    qDebug() << "Click on tray icon";

    switch(reason)
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        // Если окно видно
        if(m_host->isVisible())
        {
            if(m_host->isMinimized())
            {
                qDebug() << "If visible and minimized";
                emit restoreRequested();
                return;
            }
            else
            {
                qDebug() << "Hide";
                m_host->hide();
                return;
            }
        }
        else
        {
            qDebug() << "If not visible";
            emit restoreRequested();
            return;
        }
    default:
        break;
    }
}

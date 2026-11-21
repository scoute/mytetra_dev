#include <QApplication>
#include <QEventLoop>
#include <QProgressDialog>

#include "SyncProgressWindow.h"
#include "libraries/sync/SyncSession.h"


SyncProgressWindow::SyncProgressWindow(SyncSession &session, QWidget *parent)
    : QObject(parent)
{
    m_dialog=new QProgressDialog(tr("Синхронизация…"), QString(), 0, 0, parent);
    m_dialog->setWindowTitle(tr("Sync"));
    m_dialog->setWindowModality(Qt::ApplicationModal);
    m_dialog->setCancelButton(nullptr);
    m_dialog->setMinimumDuration(0);
    m_dialog->setAutoClose(false);
    m_dialog->setAutoReset(false);
    m_dialog->setValue(0);
    m_dialog->show();

    // Дать окну отрисоваться до блокирующей сессии
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    m_throttle.start();

    m_session=&session;
    session.setProgressCallback(
        [this](const QString &phase, int done, int total) {
            onProgress(phase, done, total);
        });
}


SyncProgressWindow::~SyncProgressWindow()
{
    // Не оставляем висячий колбэк на разрушенное окно
    if(m_session!=nullptr)
    {
        m_session->setProgressCallback(nullptr);
        m_session=nullptr;
    }

    if(m_dialog!=nullptr)
    {
        m_dialog->close();
        delete m_dialog;
        m_dialog=nullptr;
    }
}


void SyncProgressWindow::onProgress(const QString &phase, int done, int total)
{
    if(m_dialog==nullptr)
        return;

    if(total>0)
    {
        m_dialog->setLabelText(tr("%1: %2 из %3").arg(phase).arg(done).arg(total));
        m_dialog->setRange(0, total);
        m_dialog->setValue(done);
    }
    else
    {
        m_dialog->setLabelText(phase);
        m_dialog->setRange(0, 0);
    }

    // Не чаще ~7 раз в секунду: сессия и так в GUI-потоке
    if(m_throttle.elapsed()>=150)
    {
        m_throttle.restart();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

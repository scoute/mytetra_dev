#ifndef SYNCPROGRESSWINDOW_H
#define SYNCPROGRESSWINDOW_H

#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QWidget;
class QProgressDialog;
class SyncSession;

// Окно прогресса нативного синка на десктопе. Раньше сессия шла в
// GUI-потоке вообще без обратной связи: на 100k окно просто замирало.
// Здесь — фаза + счётчики; repaint'ы ограничены по времени, ввод
// пользователя не обрабатывается (QEventLoop::ExcludeUserInputEvents).
class SyncProgressWindow : public QObject
{
    Q_OBJECT

public:
    SyncProgressWindow(SyncSession &session, QWidget *parent);
    ~SyncProgressWindow();

private:
    void onProgress(const QString &phase, int done, int total);

    QProgressDialog *m_dialog=nullptr;
    SyncSession *m_session=nullptr;
    QElapsedTimer m_throttle;
};

#endif // SYNCPROGRESSWINDOW_H

#ifndef SYNCHUB_H
#define SYNCHUB_H

#include <QObject>
#include <QString>

class SyncServer;
class KnowTreeModel;
class AppConfig;
class SyncSidecar;

// Хаб синхронизации: держит HTTP-сервер и отдает локальное состояние.
// Роль TCP-сервера == роль хаба пары в MVP (десктоп слушает всегда).
// Спок забирает манифест, считает план у себя и гоняет блобы;
// хаб параллельно считает план у себя, когда спок присылает свой
// манифест (обе стороны детерминированно согласны: правило одно).
class SyncHub : public QObject
{
    Q_OBJECT

public:

    explicit SyncHub(KnowTreeModel *model, AppConfig *config,
                     SyncSidecar &sidecar, const QString &tetradir,
                     QObject *parent=nullptr);
    ~SyncHub(void) override;

    // Слушать порт. false — порт занят
    bool start(quint16 port);
    void stop(void);
    bool isRunning(void) const;
    quint16 port(void) const;

    // Мы хаб этой пары (в MVP всегда true на слушающей стороне).
    // Влияет только на разрешение равных mtime при сверке
    void setHub(bool hub);
    bool isHub(void) const;

private:

    SyncServer *server=nullptr;
    KnowTreeModel *model=nullptr;
    AppConfig *config=nullptr;
    SyncSidecar *sidecar=nullptr;
    QString tetradir;
    bool hub=true;
};

#endif // SYNCHUB_H

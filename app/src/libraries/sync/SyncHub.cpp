#include "SyncHub.h"
#include "SyncServer.h"
#include "SyncHandshake.h"
#include "SyncManifest.h"
#include "SyncSidecar.h"


SyncHub::SyncHub(KnowTreeModel *model, AppConfig *config,
                 SyncSidecar &sidecar, const QString &tetradir,
                 QObject *parent)
    : QObject(parent),
      model(model),
      config(config),
      sidecar(&sidecar),
      tetradir(tetradir)
{
    server=new SyncServer(this);
    SyncHandshake::registerRoutes(server, config);

    // Манифест локальной базы для спока
    server->addRoute(QStringLiteral("GET /manifest"),
                     [this](const QJsonObject &) -> QJsonObject {
        const SyncManifest manifest=
            SyncManifestBuilder::build(this->model, *this->sidecar,
                                       this->tetradir);
        return manifest.toJson();
    });
}


SyncHub::~SyncHub(void)
{
    stop();
}


bool SyncHub::start(quint16 port)
{
    return server->start(port);
}


void SyncHub::stop(void)
{
    server->stop();
}


bool SyncHub::isRunning(void) const
{
    return server->isRunning();
}


quint16 SyncHub::port(void) const
{
    return server->port();
}


void SyncHub::setHub(bool hub)
{
    this->hub=hub;
}


bool SyncHub::isHub(void) const
{
    return hub;
}

#include "SyncHub.h"
#include "SyncServer.h"
#include "SyncHandshake.h"
#include "SyncManifest.h"
#include "SyncSidecar.h"
#include "SyncNoteIo.h"
#include "models/tree/KnowTreeModel.h"


SyncHub::SyncHub(KnowTreeModel *model, AppConfig *config,
                 SyncSidecar &sidecar, const QString &tetradir,
                 QObject *parent, const QString &pairingPin)
    : QObject(parent),
      model(model),
      config(config),
      sidecar(&sidecar),
      tetradir(tetradir)
{
    server=new SyncServer(this);
    SyncHandshake::registerRoutes(server, this->config, pairingPin);

    // Манифест локальной базы для спока
    server->addRoute(QStringLiteral("GET /manifest"),
                     [this](const QJsonObject &) -> QJsonObject {
        const SyncManifest manifest=
            SyncManifestBuilder::build(this->model, *this->sidecar,
                                       this->tetradir);
        return manifest.toJson();
    });

    // Заметка целиком: поля, блоб текста как лежит, аттачи байтами.
    // Шифрованные отдаются шифртекстом, ключ не нужен и не отдается
    server->addRoute(QStringLiteral("POST /pull-note"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return pullNote(request.value(QStringLiteral("id")).toString());
    });

    // Ветка целиком: поля + parentId
    server->addRoute(QStringLiteral("POST /pull-branch"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return pullBranch(request.value(QStringLiteral("id")).toString());
    });

    // Прием заметки целиком (создание или обновление, id сохраняются).
    // Ветка должна уже существовать: клиент шлет ветки раньше заметок
    server->addRoute(QStringLiteral("POST /push-note"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return applyNote(request);
    });
    // Прием ветки целиком (создание или обновление полей, id сохраняется)
    server->addRoute(QStringLiteral("POST /push-branch"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return applyBranch(request);
    });

    // Применить удаление по tombstone: ветка с поддеревом или заметка.
    // Время tombstone'а — удаленное, не локальное (иначе пинг-понг)
    server->addRoute(QStringLiteral("POST /push-delete"),
                     [this](const QJsonObject &request) -> QJsonObject {
        return applyDelete(request.value(QStringLiteral("id")).toString(),
                           request.value(QStringLiteral("deletedAt")).toVariant().toLongLong());
    });

    // Фиксация сессии: сейв базы и перезагрузка моделей,
    // чтобы виды показали примененное. Вызывает клиент в конце плана
    server->addRoute(QStringLiteral("POST /commit"),
                     [this](const QJsonObject &) -> QJsonObject {
        this->model->save();
        this->model->reload();

        QJsonObject answer;
        answer[QStringLiteral("ok")]=true;
        return answer;
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


static QByteArray readFileBytes(const QString &path, bool &ok)
{
    QFile file(path);
    ok=file.open(QIODevice::ReadOnly);

    if(!ok)
        return QByteArray();

    return file.readAll();
}


QJsonObject SyncHub::pullNote(const QString &id)
{
    return SyncNoteIo::serializeNote(model, tetradir, id);
}


QJsonObject SyncHub::pullBranch(const QString &id)
{
    return SyncNoteIo::serializeBranch(model, id);
}


QJsonObject SyncHub::applyNote(const QJsonObject &request)
{
    QJsonObject answer;
    const QString error=SyncNoteIo::applyNote(model, tetradir, request);

    if(!error.isEmpty())
        answer[QStringLiteral("error")]=error;
    else
        answer[QStringLiteral("ok")]=true;

    return answer;
}


QJsonObject SyncHub::applyBranch(const QJsonObject &request)
{
    QJsonObject answer;
    const QString error=SyncNoteIo::applyBranch(model, request);

    if(!error.isEmpty())
        answer[QStringLiteral("error")]=error;
    else
        answer[QStringLiteral("ok")]=true;

    return answer;
}


QJsonObject SyncHub::applyDelete(const QString &id, qint64 deletedAt)
{
    QJsonObject answer;
    const QString error=SyncNoteIo::applyDelete(model, id, deletedAt);

    if(!error.isEmpty())
        answer[QStringLiteral("error")]=error;
    else
        answer[QStringLiteral("ok")]=true;

    return answer;
}

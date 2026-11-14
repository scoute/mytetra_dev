#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include "SyncHub.h"
#include "SyncServer.h"
#include "SyncHandshake.h"
#include "SyncManifest.h"
#include "SyncSidecar.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/attachTable/AttachTableData.h"


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
    QJsonObject answer;

    if(model==nullptr || id.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad id");
        return answer;
    }

    Record *record=model->getRecord(id);

    if(record==nullptr)
    {
        answer[QStringLiteral("error")]=QStringLiteral("not found");
        return answer;
    }

    answer[QStringLiteral("id")]=id;
    answer[QStringLiteral("crypt")]=
        (record->getNaturalFieldSource(QStringLiteral("crypt"))==QStringLiteral("1"));

    static const char *fieldNames[] = {
        "name", "author", "url", "tags", "ctime",
        "block", "hasAttach", "attachCount"
    };

    QJsonObject fields;

    for(const char *field : fieldNames)
    {
        const QString name=QString::fromLatin1(field);
        fields.insert(name, record->getNaturalFieldSource(name));
    }

    answer[QStringLiteral("fields")]=fields;

    // Блоб текста как лежит на диске: открытый текст или шифртекст.
    // Шифртекст отдается вслепую, ключ не нужен
    const QString dir=record->getNaturalFieldSource(QStringLiteral("dir"));
    const QString file=record->getNaturalFieldSource(QStringLiteral("file"));
    const QString textPath=tetradir+QStringLiteral("/base/")
                           +dir+QStringLiteral("/")+file;

    bool ok=false;
    const QByteArray textBytes=readFileBytes(textPath, ok);

    if(!ok)
    {
        answer[QStringLiteral("error")]=QStringLiteral("no text");
        return answer;
    }

    answer[QStringLiteral("text")]=
        QString::fromLatin1(textBytes.toBase64());

    // Аттачи байтами. Имена файлов на принимающей стороне не используются
    // для путей (кладем в свой каталог записи), только для восстановления имен
    QJsonArray attaches;
    AttachTableData attachTable=record->getAttachTable();

    for(int i=0; i<attachTable.size(); ++i)
    {
        const QString absPath=attachTable.getAbsoluteInnerFileName(i);
        const QByteArray data=readFileBytes(absPath, ok);

        if(!ok)
        {
            answer[QStringLiteral("error")]=QStringLiteral("no attach file");
            return answer;
        }

        QJsonObject a;
        a[QStringLiteral("id")]=attachTable.getIdByRow(i);
        a[QStringLiteral("fileName")]=QFileInfo(absPath).fileName();
        a[QStringLiteral("data")]=QString::fromLatin1(data.toBase64());
        attaches.append(a);
    }

    answer[QStringLiteral("attaches")]=attaches;
    return answer;
}


QJsonObject SyncHub::pullBranch(const QString &id)
{
    QJsonObject answer;

    if(model==nullptr || id.isEmpty())
    {
        answer[QStringLiteral("error")]=QStringLiteral("bad id");
        return answer;
    }

    TreeItem *item=model->getItemById(id);

    if(item==nullptr)
    {
        answer[QStringLiteral("error")]=QStringLiteral("not found");
        return answer;
    }

    // Родитель корневого псевдо-элемента не синхронизируется:
    // верхнеуровневые ветки едут с пустым parent
    QString parentId;
    TreeItem *parentItem=item->parent();

    if(parentItem!=nullptr)
    {
        const QString parentFieldId=
            parentItem->getField(QStringLiteral("id"));

        if(parentFieldId!=QStringLiteral("0"))
            parentId=parentFieldId;
    }

    answer[QStringLiteral("id")]=id;
    answer[QStringLiteral("parent")]=parentId;

    QJsonObject fields;
    const QMap<QString, QString> allFields=item->getAllFieldsDirect();

    for(auto it=allFields.constBegin(); it!=allFields.constEnd(); ++it)
        fields.insert(it.key(), it.value());

    answer[QStringLiteral("fields")]=fields;
    return answer;
}

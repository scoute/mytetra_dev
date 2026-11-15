#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDebug>

#include "SyncSidecar.h"
#include "SyncDebug.h"


namespace
{
SyncSidecar *g_instance=nullptr;
}


SyncSidecar &SyncSidecar::instance(void)
{
    if(g_instance==nullptr)
        g_instance=new SyncSidecar();

    return *g_instance;
}


void SyncSidecar::init(const QString &xmlFileName)
{
    SyncSidecar &self=instance();

    if(self.load(xmlFileName))
        return;

    // Битый сайдкар — не молчим (раньше load() игнорировался, все touch*
    // становились no-op и журнал терялся молча). В журнале живут tombstone'ы:
    // без них удаления воскресают с пира. Файл убираем в карантин,
    // состояние начинаем с пустого и пишем заново при первом сейве
    const QString path=sidecarPathForXml(xmlFileName);
    const QString quarantine=path+QStringLiteral(".corrupt-")
        +QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss"));

    if(QFile::rename(path, quarantine))
        qWarning() << "SyncSidecar: sidecar was corrupted, moved to"
                   << quarantine;

    qWarning() << "SyncSidecar: sync journal (incl. tombstones) is lost, "
                  "deleted items may come back from peers until re-deleted";

    self.load(xmlFileName); // файла больше нет — пустое состояние, не ошибка
}


void SyncSidecar::shutdown(void)
{
    delete g_instance;
    g_instance=nullptr;
}


void SyncSidecar::touchNote(const QString &id, const QByteArray &contentBytes)
{
    if(g_instance==nullptr || !g_instance->isLoaded())
        return;

    const QString hash=QString::fromLatin1(
        QCryptographicHash::hash(contentBytes,
                                 QCryptographicHash::Sha256).toHex());

    g_instance->setEntry(id, QDateTime::currentMSecsSinceEpoch(),
                         hash, QStringLiteral("note"));
    g_instance->save();
}


void SyncSidecar::touchFields(const QString &id)
{
    if(g_instance==nullptr || !g_instance->isLoaded() || id.isEmpty())
        return;

    Entry old=g_instance->entry(id);

    g_instance->setEntry(id, QDateTime::currentMSecsSinceEpoch(),
                         old.hash, QStringLiteral("note"));
    g_instance->save();
}


void SyncSidecar::forgetNote(const QString &id)
{
    if(g_instance==nullptr || !g_instance->isLoaded() || id.isEmpty())
        return;

    g_instance->forget(id, QDateTime::currentMSecsSinceEpoch());
    g_instance->save();
}


void SyncSidecar::adopt(const QString &id, qint64 mtime,
                        const QString &hash, const QString &kind)
{
    if(g_instance==nullptr || !g_instance->isLoaded() || id.isEmpty())
        return;

    g_instance->setEntry(id, mtime, hash, kind);
    g_instance->save();
}


void SyncSidecar::adoptTombstone(const QString &id, qint64 deletedAt)
{
    if(g_instance==nullptr || !g_instance->isLoaded() || id.isEmpty())
        return;

    g_instance->forget(id, deletedAt);
    g_instance->save();
}


void SyncSidecar::reset(void)
{
    if(g_instance==nullptr)
        return;

    g_instance->m_entries.clear();
    g_instance->m_tombstones.clear();

    if(g_instance->isLoaded())
        g_instance->save();

    syncDbg("sidecar: journal reset");
}


void SyncSidecar::clearTombstones(void)
{
    if(g_instance==nullptr)
        return;

    const int count=g_instance->m_tombstones.size();
    g_instance->m_tombstones.clear();

    if(g_instance->isLoaded())
        g_instance->save();

    if(count>0)
        syncDbg("sidecar: "+QString::number(count)
                +" tombstone(s) cleared (new pairing epoch)");
}


void SyncSidecar::dropEntry(const QString &id)
{
    if(g_instance==nullptr || !g_instance->isLoaded() || id.isEmpty())
        return;

    g_instance->m_entries.remove(id);
    g_instance->save();
}


QString SyncSidecar::hashBranchFields(const QMap<QString, QString> &fields,
                                        const QString &parentId)
{
    QByteArray canonical;

    for(auto it=fields.constBegin(); it!=fields.constEnd(); ++it)
    {
        QString value=it.value();

        // Канонический crypt: "" вместо "0" старых баз (семантика одна).
        // Нормализация в одной точке — хеш, поэтому старые базы
        // сходятся без переписывания данных
        if(it.key()==QStringLiteral("crypt")
           && value==QStringLiteral("0"))
            value.clear();

        // Пустые значения не хешируем вообще: реальное приложение часть
        // атрибутов не пишет (нет ctime/icon = пустой ctime/icon),
        // а round-trip через JSON/apply их то роняет, то возвращает.
        // Без этого хеш вечно расходится на пустом месте, и каждая
        // такая ветка висит в конфликтах. Непустое <-> пустое —
        // по-прежнему разница (иконку сняли — видно)
        if(value.isEmpty())
            continue;

        canonical+=it.key().toUtf8();
        canonical+='=';
        canonical+=value.toUtf8();
        canonical+='\n';
    }

    canonical+="parent=";
    canonical+=parentId.toUtf8();
    canonical+='\n';

    return QString::fromLatin1(
        QCryptographicHash::hash(canonical,
                                 QCryptographicHash::Sha256).toHex());
}


void SyncSidecar::touchBranch(const QString &id,
                              const QMap<QString, QString> &fields,
                              const QString &parentId)
{
    if(g_instance==nullptr || !g_instance->isLoaded() || id.isEmpty())
        return;

    g_instance->setEntry(id, QDateTime::currentMSecsSinceEpoch(),
                         hashBranchFields(fields, parentId),
                         QStringLiteral("branch"));
    g_instance->save();
}


void SyncSidecar::forgetBranch(const QString &id)
{
    if(g_instance==nullptr || !g_instance->isLoaded() || id.isEmpty())
        return;

    g_instance->forget(id, QDateTime::currentMSecsSinceEpoch());
    g_instance->save();
}


QString SyncSidecar::sidecarPathForXml(const QString &xmlFileName)
{
    // Тот же каталог, фиксированное имя. Подкаталоги баз не поддерживаются:
    // mytetra.xml всегда один на базу
    return QFileInfo(xmlFileName).absolutePath()+
           QStringLiteral("/mytetra.sync");
}


bool SyncSidecar::load(const QString &xmlFileName)
{
    m_xmlFileName=xmlFileName;
    m_sidecarPath=sidecarPathForXml(xmlFileName);
    m_entries.clear();
    m_tombstones.clear();
    m_loaded=false;

    QFile file(m_sidecarPath);

    if(!file.exists())
    {
        // Сайдкара нет — первое знакомство движка с базой.
        // Не ошибка: состояние строится с нуля при сейвах
        m_loaded=true;
        return true;
    }

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError parseError;
    const QJsonDocument doc=QJsonDocument::fromJson(file.readAll(),
                                                    &parseError);
    file.close();

    if(parseError.error!=QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject root=doc.object();

    if(root.value(QStringLiteral("version")).toInt(0)!=1)
        return false;

    const QJsonObject entries=root.value(QStringLiteral("entries")).toObject();

    for(auto it=entries.constBegin(); it!=entries.constEnd(); ++it)
    {
        const QJsonObject e=it.value().toObject();
        Entry entry;
        entry.mtime=e.value(QStringLiteral("mtime")).toVariant().toLongLong();
        entry.hash=e.value(QStringLiteral("hash")).toString();
        entry.kind=e.value(QStringLiteral("kind")).toString();
        m_entries.insert(it.key(), entry);
    }

    const QJsonObject tombs=root.value(QStringLiteral("tombstones")).toObject();

    for(auto it=tombs.constBegin(); it!=tombs.constEnd(); ++it)
        m_tombstones.insert(it.key(), it.value().toVariant().toLongLong());

    m_loaded=true;
    syncDbg("sidecar loaded: "+QString::number(m_entries.size())
            +" entries, "+QString::number(m_tombstones.size())+" tombstones");
    return true;
}


bool SyncSidecar::save(void) const
{
    if(!m_loaded || m_sidecarPath.isEmpty())
        return false;

    QJsonObject entries;

    for(auto it=m_entries.constBegin(); it!=m_entries.constEnd(); ++it)
    {
        QJsonObject e;
        e[QStringLiteral("mtime")]=it.value().mtime;
        e[QStringLiteral("hash")]=it.value().hash;
        e[QStringLiteral("kind")]=it.value().kind;
        entries.insert(it.key(), e);
    }

    QJsonObject tombs;

    for(auto it=m_tombstones.constBegin(); it!=m_tombstones.constEnd(); ++it)
        tombs.insert(it.key(), it.value());

    QJsonObject root;
    root[QStringLiteral("version")]=1;
    root[QStringLiteral("entries")]=entries;
    root[QStringLiteral("tombstones")]=tombs;

    // Атомарно (QSaveFile): обрыв посреди записи раньше оставлял
    // обрезанный JSON, load() его не брал и журнал терялся целиком
    QSaveFile file(m_sidecarPath);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "SyncSidecar: cannot write" << m_sidecarPath;
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));

    if(!file.commit())
    {
        qWarning() << "SyncSidecar: commit failed for" << m_sidecarPath;
        syncDbg("sidecar: SAVE FAILED "+m_sidecarPath);
        return false;
    }

    return true;
}


bool SyncSidecar::isLoaded(void) const
{
    return m_loaded;
}


QString SyncSidecar::xmlFileName(void) const
{
    return m_xmlFileName;
}


void SyncSidecar::setEntry(const QString &id, qint64 mtime,
                           const QString &hash, const QString &kind)
{
    if(id.isEmpty())
        return;

    Entry e;
    e.mtime=mtime;
    e.hash=hash;
    e.kind=kind;
    m_entries.insert(id, e);
    m_tombstones.remove(id);
}


bool SyncSidecar::hasEntry(const QString &id) const
{
    return m_entries.contains(id);
}


SyncSidecar::Entry SyncSidecar::entry(const QString &id) const
{
    return m_entries.value(id);
}


QMap<QString, SyncSidecar::Entry> SyncSidecar::allEntries(void) const
{
    return m_entries;
}


void SyncSidecar::forget(const QString &id, qint64 deletedAt)
{
    m_entries.remove(id);

    if(!id.isEmpty())
        m_tombstones.insert(id, deletedAt);
}


QMap<QString, qint64> SyncSidecar::tombstones(void) const
{
    return m_tombstones;
}


void SyncSidecar::clearTombstone(const QString &id)
{
    m_tombstones.remove(id);
}

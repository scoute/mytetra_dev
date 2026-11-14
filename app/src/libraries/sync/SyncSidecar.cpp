#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

#include "SyncSidecar.h"


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
    instance().load(xmlFileName);
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


QString SyncSidecar::sidecarPathForXml(const QString &xmlFileName)
{
    // Тот же каталог, фиксированное имя. Подкаталоги баз не поддерживаются:
    // mytetra.xml всегда один на базу
    return QFileInfo(xmlFileName).absolutePath()+
           QStringLiteral("/mytetras.sync");
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

    QFile file(m_sidecarPath);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
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

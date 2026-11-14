#ifndef SYNCSIDECAR_H
#define SYNCSIDECAR_H

#include <QMap>
#include <QString>

// Сайдкар синхронизации (mytetras.sync рядом с mytetras.xml, JSON).
// Свое состояние движка, стоковый xintrea файл не знает и игнорирует.
// entries: id -> {mtime, hash, kind}. mtime — мс последнего локального
// сейва (для шифрованных — время изменения blob-файла, внутрь не лезем).
// hash — SHA256 контента (текст как лежит / канонические поля ветки).
// tombstones: id -> время удаления. Чистятся только после подтверждения
// всех спаренных устройств (watermark'и хранит хаб, см. SyncHub позже).
class SyncSidecar
{
public:

    struct Entry
    {
        qint64 mtime=0;
        QString hash;
        QString kind; // "note" или "branch"
    };

    // Путь сайдкара для данного mytetra.xml (тот же каталог, имя mytetras.sync)
    static QString sidecarPathForXml(const QString &xmlFileName);

    // Загрузить с диска (нет файла — пустое состояние, не ошибка).
    // false — файл битый: состояние сбрасывается в пустое, вызывающий
    // решает (карантин/клонирование), молча не продолжаем
    bool load(const QString &xmlFileName);
    bool save(void) const;

    // Состояние
    bool isLoaded(void) const;
    QString xmlFileName(void) const;

    // Записи
    void setEntry(const QString &id, qint64 mtime, const QString &hash,
                  const QString &kind);
    bool hasEntry(const QString &id) const;
    Entry entry(const QString &id) const;
    QMap<QString, Entry> allEntries(void) const;

    // Удаление: запись уходит, появляется tombstone. Безвозвратного
    // стирания из журнала здесь нет — только через подтверждение спариваний
    void forget(const QString &id, qint64 deletedAt);
    QMap<QString, qint64> tombstones(void) const;
    void clearTombstone(const QString &id);

    // Единственный экземпляр на процесс (база одна активна).
    // init вызывается при открытии базы, дальше touch* пишут сами
    static SyncSidecar &instance(void);
    static void init(const QString &xmlFileName);
    static void shutdown(void);

    // Отметить сейв заметки: mtime=сейчас, hash контента, kind="note".
    // Пишет сайдкар на диск сразу (файл крошечный). Нет init — no-op
    static void touchNote(const QString &id, const QByteArray &contentBytes);

    // Отметить изменение без нового контента (поля, перемещение):
    // mtime=сейчас, hash прежний. Пишет на диск сразу
    static void touchFields(const QString &id);

    // Отметить удаление: запись уходит, появляется tombstone.
    // Пишет на диск сразу
    static void forgetNote(const QString &id);

    // Хеш канонической сериализации полей ветки (QMap уже отсортирован)
    static QString hashBranchFields(const QMap<QString, QString> &fields);

    // Отметить изменение ветки: mtime=сейчас, hash полей. Пишет сразу
    static void touchBranch(const QString &id,
                            const QMap<QString, QString> &fields);

    // Забыть ветку целиком (tombstone). Пишет сразу
    static void forgetBranch(const QString &id);

private:

    QString m_xmlFileName;
    QString m_sidecarPath;
    bool m_loaded=false;
    QMap<QString, Entry> m_entries;
    QMap<QString, qint64> m_tombstones;
};

#endif // SYNCSIDECAR_H

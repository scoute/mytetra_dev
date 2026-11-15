#ifndef SYNCSIDECAR_H
#define SYNCSIDECAR_H

#include <QMap>
#include <QString>

// Сайдкар синхронизации (mytetra.sync рядом с mytetra.xml, JSON).
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

    // Путь сайдкара для данного mytetra.xml (тот же каталог, имя mytetra.sync)
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

    // Принять чужое состояние как свое (при apply пуша): mtime/hash
    // ставятся присланные, а не "сейчас". Пишет сразу
    static void adopt(const QString &id, qint64 mtime,
                      const QString &hash, const QString &kind);

    // Принять чужой tombstone: запись стирается, время — удаленное.
    // Пишет сразу
    static void adoptTombstone(const QString &id, qint64 deletedAt);

    // Полный сброс журнала: entries и tombstones стираются целиком.
    // Нужен стратегиям "начать заново" (локальное состояние уходит
    // в корзину и пересоздается синком с чистого листа)
    static void reset(void);

    // Сброс только tombstone'ов: новая пара — новая эпоха, старые
    // удаления не должны поехать новому пиру и уничтожить его данные
    static void clearTombstones(void);

    // Молча выкинуть entry без tombstone (мусор удаленного поддерева:
    // его покрывает tombstone ветки). Пишет сразу
    static void dropEntry(const QString &id);

    // Хеш канонической сериализации полей ветки (QMap уже отсортирован)
    // плюс parentId: переезд ветки меняет хеш и потому синхронизируется.
    // parentId в формате провода ("" для корневого уровня, не "0")
    static QString hashBranchFields(const QMap<QString, QString> &fields,
                                    const QString &parentId=QString());

    // Отметить изменение ветки: mtime=сейчас, hash полей+родителя. Пишет сразу
    static void touchBranch(const QString &id,
                            const QMap<QString, QString> &fields,
                            const QString &parentId=QString());

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

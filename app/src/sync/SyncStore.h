#ifndef _SYNCSTORE_H_
#define _SYNCSTORE_H_

#include <QString>
#include <QMap>

class KnowTreeModel;
class TreeItem;

// Sidecar-состояние синхронизации. Формат mytetra.xml заморожен,
// поэтому ревизии и хеши хранятся отдельно в syncstate.ini.
// Семантика: rev монотонно растет при каждом изменении серверного
// состояния (push клиента или замеченные локальные правки).
// Для каждой сущности помнится хеш и rev последнего изменения.
// Прототип рассчитан на одного клиента (один телефон).

class SyncStore
{
public:

    SyncStore(void);
    ~SyncStore(void);

    // Загрузка и сохранение sidecar-файла из рабочей директории.
    // Файла может не быть при первом запуске, это нормально
    void load(void);
    void save(void) const;

    // Текущая ревизия серверного состояния
    unsigned int rev(void) const;

    // Сканирование модели без изменения sidecar: в changedBranches и
    // changedRecords складываются id и текущие хеши сущностей, чьи хеши
    // отличаются от запомненных (локальные правки, новые сущности).
    // Удаленные локально сущности сразу забываются, удаления
    // в прототипе не синхронизируются
    void scan(KnowTreeModel *model,
              QMap<QString, QString> &changedBranches,
              QMap<QString, QString> &changedRecords);

    // Хеш ветки по ее полям
    static QString branchHash(TreeItem *item);

    // Хеш записи: поля таблицы, байты text.html, имена и байты блобов.
    // dirName это поле dir записи (подкаталог в base/)
    static QString recordHash(const QMap<QString, QString> &fields,
                              const QString &dirName);

    // Пометка сущности как измененной в текущей ревизии.
    // Вызывается после применения push
    void touchBranch(const QString &id, const QString &hash);
    void touchRecord(const QString &id, const QString &hash);

    // Запомненный хеш и rev сущности, пусто если сущность неизвестна
    QString storedBranchHash(const QString &id) const;
    QString storedRecordHash(const QString &id) const;
    unsigned int storedBranchRev(const QString &id) const;
    unsigned int storedRecordRev(const QString &id) const;

    // Удаление сущности из sidecar (ветка или запись удалены локально).
    // Удаления в прототипе не синхронизируются, только забываются
    void forgetBranch(const QString &id);
    void forgetRecord(const QString &id);

private:

    // Имя sidecar-файла в рабочей директории
    static QString stateFileName(void);

    // Обход дерева с вызовом callback для каждой ветки.
    // Без functional, в стиле проекта: два отдельных метода
    void collectBranchHashes(KnowTreeModel *model,
                             QMap<QString, QString> &hashes) const;
    void collectBranchHashesRecurse(TreeItem *item,
                                    QMap<QString, QString> &hashes) const;

    unsigned int m_rev;

    QMap<QString, QString> m_branchHash;
    QMap<QString, unsigned int> m_branchRev;

    QMap<QString, QString> m_recordHash;
    QMap<QString, unsigned int> m_recordRev;
};

#endif // _SYNCSTORE_H_

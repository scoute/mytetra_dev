#ifndef _SYNCCORE_H_
#define _SYNCCORE_H_

#include <QMap>
#include <QString>
#include <QByteArray>

#include "sync/SyncStore.h"

class KnowTreeModel;
class TreeItem;
class QJsonArray;

// Ядро синхронизации, общее для всех транспортов (HTTP-сервер,
// будущий локальный MCP). Транспорты это тонкие адаптеры: парсят
// свой протокол и зовут эти методы.
// Правила ядра:
// - модель передается явно, никакого поиска окон по именам,
//   чтобы ядро работало и без GUI;
// - никакой графики: ни диалогов, ни message box, только коды
//   и тексты ошибок в ответах;
// - все изменения состояния идут через SyncStore, чтобы правки
//   из любого транспорта были видны остальным.

class SyncCore
{
public:

    SyncCore(void);
    ~SyncCore(void);

    // Модель, с которой работает ядро. Должна быть задана до вызовов
    void setModel(KnowTreeModel *model);

    // Доступ к sidecar ревизий и хешей
    SyncStore &store(void);

    // Версия протокола синхронизации
    static int protoVersion(void);

    // Полный снимок дерева: ветки и записи с родителями и хешами.
    // Локальные правки подтверждаются в sidecar
    QByteArray snapshot(void);

    // Инкрементальные изменения против последнего подтвержденного
    // состояния. Параметр since принимается, но в прототипе diff
    // всегда против него (одного клиента достаточно)
    QByteArray pull(void);

    // Полная запись: поля, тело, список блобов.
    // Пустой массив если запись не найдена
    QByteArray record(const QString &recordId);

    // Байты блоба, код ответа кладется в responseCode.
    // Имя санитизируется, путь строго внутри каталога записи
    QByteArray blob(const QString &recordId,
                    const QString &fileName,
                    int &responseCode);

    // Применение изменений клиента: ветки и записи из JSON push.
    // Примененные id и проблемы складываются в ответ
    QByteArray push(const QByteArray &body);

    // Полный снимок дерева: ветки и записи с родителями и хешами
    void collectSnapshot(QMap<QString, QMap<QString, QString> > &branches,
                         QMap<QString, QMap<QString, QString> > &records);

    // Применение push: ветки и записи. Примененные id складываются
    // в appliedBranches и appliedRecords, проблемы в conflicts
    void applyPush(unsigned int baseRev,
                   const QJsonArray &branches,
                   const QJsonArray &records,
                   QJsonArray &appliedBranches,
                   QJsonArray &appliedRecords,
                   QJsonArray &conflicts);

private:

    // Поиск записи по id. Ветка и позиция в ее таблице кладутся
    // в branchItem и pos. Только чтение модели
    static bool findRecord(KnowTreeModel *model,
                           const QString &recordId,
                           TreeItem *&branchItem,
                           int &pos);

    KnowTreeModel *m_model;

    // Sidecar ревизий и хешей, общий для всех транспортов
    SyncStore m_store;
};

#endif // _SYNCCORE_H_

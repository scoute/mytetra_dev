#ifndef SYNCPREVIEW_H
#define SYNCPREVIEW_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "SyncManifest.h"

// Предпросмотр синка: классификация плана по категориям и действия
// над категориями/элементами. Принцип: безопасное (создание) едет
// молча, остальное — с явного решения пользователя.
//
// Категории (разделение удалений по направлению — решение дизайнa):
//   Create      — добавлено (новые записи/ветки)
//   Rename      — переименовано/метаданные (содержимое то же)
//   Edit        — изменено содержимое (текст, аттачи, картинки)
//   MyDelete    — мои удаления (едут на сервер)
//   TheirDelete — ихние удаления (приедут ко мне)
//
// Действия (универсальные для всех категорий):
//   Apply     — применить молча
//   Confirm   — показать и подтвердить (diff по требованию)
//   TakeServer— взять с сервера: мое в корзину/воскресить
//   ForceMine — настоять на моем: мое побеждает, ихнее в корзину
//   Skip      — пропустить в этот раз (расхождение остается)
namespace SyncPreview
{

enum class Category
{
    Create=0,
    Rename,
    Edit,
    MyDelete,
    TheirDelete,
};

enum class Action
{
    Apply=0,
    Confirm,
    TakeServer,
    ForceMine,
    Skip,
};

// Элемент предпросмотра: одна запись/ветка в одной категории
struct Item
{
    QString id;
    Category category=Category::Create;
    bool isBranch=false;
    bool fromLocal=false;  // true — наше изменение (едет наружу),
                           // false — ихнее (придет к нам)
    QString name;          // имя для списка (из нашего манифеста)
    QString branchId;      // для записей: ветка (для diff-запроса)
    QString parentId;      // для веток: родитель; для записей = branchId
    QString parentName;    // имя родителя (для дерева по веткам)
};

// Разложенный план: элементы по категориям (порядок стабильный)
struct Preview
{
    QList<Item> items;

    int count(Category category) const;
    bool isEmpty(void) const;
    bool hasDestructive(void) const;  // есть удаления
    bool needsAttention(void) const;  // есть не-создания (Confirm-кандидаты)
};

// Классифицировать план по категориям.
// Правила: есть только там -> Create; хеши равны, fhash/поля разные ->
// Rename; хеш контента разный -> Edit; pushTombs -> MyDelete;
// deleteLocal -> TheirDelete
Preview classify(const SyncPlan &plan,
                 const SyncManifest &local,
                 const SyncManifest &remote);

// Действия по категориям + точечные переопределения по id
struct Decisions
{
    QMap<Category, Action> category;
    QMap<QString, Action> byId;

    Action forItem(const Item &item) const;

    // Дефолтная матрица (см. спеку): Create=Apply, Rename=Confirm,
    // Edit=Confirm, MyDelete=TakeServer, TheirDelete=Confirm
    static Decisions defaults(void);
};

// Применить решения к плану: Skip/TakeServer/ForceMine переписывают
// списки плана (см. комментарии в .cpp — там же тонкости воскрешения)
SyncPlan applyDecisions(const SyncPlan &plan,
                        const Preview &preview,
                        const Decisions &decisions,
                        const SyncManifest &local,
                        const SyncManifest &remote);

// Имя действия для UI
QString actionName(Action action);
QString categoryName(Category category);

} // namespace SyncPreview

#endif // SYNCPREVIEW_H

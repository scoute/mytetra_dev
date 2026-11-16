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
    QString parentId;      // родитель (последний из pathIds)
    QString parentName;    // имя родителя (для панели ветки)
    /** id предков от корня до родителя — порядок и глубина дерева. */
    QStringList pathIds;
    /** Имена предков от корня до родителя — контекст дерева. */
    QStringList path;
    /** Число вложений с нашей стороны (для "почти пустой"). */
    int ownAttaches=0;
    /** Число вложений с ихней стороны. */
    int otherAttaches=0;
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

// Умолчания категорий в строку конфига (5 чисел через запятую)
// и обратно. Используется галочкой "Запомнить" в окне предпросмотра
QString serializeDefaults(const Decisions &decisions);
Decisions defaultsFromString(const QString &data);

// Угроза бития: почти-пустая версия затирает содержательную.
// Асимметрично: обе почти-пустые или обе содержательные — обычный поток
enum class Danger { None, MyEmpty, TheirEmpty };

/** Порог "почти пустой": видимый текст короче (и нет вложений). */
constexpr int NEAR_EMPTY_CHARS=10;

/** Видимый текст заметки: без HTML-тегов и сущностей. */
int visibleTextLength(const QString &text);

/** Почти пустая: мало текста И нет вложений (вложения — тоже содержимое). */
bool isNearEmpty(const QString &text, int attaches);

/**
 * Опасность для Edit-правки: только когда почти-пустое затирает
 * содержательное. Создание ничего не затирает, удаления уже защищены
 * дефолтами (мои — воскресить, ихние — подтвердить).
 */
Danger dangerFor(const Item &item, bool ownEmpty, bool otherEmpty);

// Имя действия для UI
QString actionName(Action action);
QString categoryName(Category category);

} // namespace SyncPreview

#endif // SYNCPREVIEW_H

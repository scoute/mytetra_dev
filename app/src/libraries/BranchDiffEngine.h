#ifndef _BRANCHDIFFENGINE_H_
#define _BRANCHDIFFENGINE_H_

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

// Двигатель семантической разности публикаций ветки.
//
// Работает с двумя «фрагментами» (парой состояний branch.xml публикации):
//   baseline (базовая точка, с которой подписчик уже знаком) и
//   head (актуальная версия в shared/).
// Сравнение идёт по идентификаторам записей/веток и по sha256 содержимого
// файлов записей — никакого формата mytetra.xml это не меняет.
//
// Разбиение по этапам:
//   1. parseFragment(branch.xml) -> FragmentData;
//   2. fillTextShas(fragment, contentProvider) — вычисление хешей текста
//      записей (контент берётся из shared/ либо из git: baseline:records/...);
//   3. buildChangeList(baseline, head) -> список изменений для UI и импорта.

// Данные одной записи во фрагменте
struct DiffRecordData
{
    QString id;                     // уникальный id записи (атрибут id <record>)
    QString name;
    QString author;
    QString url;
    QString tags;
    QString dir;                    // каталог записи в records/
    QString file;                   // файл записи (обычно text.html)
    QByteArray textSha;             // sha256 содержимого <dir>/<file> (заполняет fillTextShas)
    QStringList attachNames;        // внутренние (внутрисистемные) имена файлов <files>/<file>
};

// Данные одной ветки (узла <node>) во фрагменте
struct DiffBranchData
{
    QString id;
    QString name;
    QString parentId;               // id родительской ветки (пусто у корня)
    QVector<QString> recordIds;     // записи ветки в порядке ветки
    QVector<QString> childIds;      // вложенные ветки в порядке ветки
};

// Полный срез фрагмента (вся информация из одного branch.xml)
struct FragmentData
{
    QHash<QString, DiffRecordData> records;     // recordId -> запись
    QHash<QString, DiffBranchData> branches;    // branchId -> ветка
    QString rootId;                             // id корневой ветки
};

// Одно изменение между baseline и head
struct DiffChange
{
    enum Type
    {
        RecordAdd,      // новая запись у владельца
        RecordUpdate,   // изменены поля/текст/вложения записи
        RecordDelete,   // запись удалена владельцем (опасно)
        BranchRename,   // изменено имя ветки (branch.props)
        BranchMove,     // ветка перенесена в другую ветку
        BranchAdd,      // новая ветка у владельца
        BranchDelete    // ветка удалена владельцем (опасно)
    };

    Type type=RecordAdd;
    QString recordId;   // id записи (для Record*)
    QString branchId;   // id ветки (для Branch*; для Record* — ветка-родитель)
    QString title;      // имя записи/ветки для отображения

    // RecordUpdate: старое/новое натуральных полей (name/author/url/tags)
    QList<QPair<QString, QPair<QString, QString>>> fields;

    // RecordUpdate: признаки изменения текста и вложений
    bool textChanged=false;
    QStringList attachAdded;
    QStringList attachRemoved;

    // BranchMove: id старого и нового родителя
    QString oldParentId;
    QString newParentId;

    // Признак «опасного» изменения (любое удаление) — требует явного согласия
    bool isDangerous(void) const
    {
        return (type==RecordDelete || type==BranchDelete);
    }

    // Тип изменения в виде текстовой метки
    QString typeName(void) const;
};

class BranchDiffEngine
{
public:
    // sha256 данных в шестнадцатеричном виде
    static QByteArray sha256(const QByteArray &data);

    // Парсинг содержимого branch.xml в структуру фрагмента
    static FragmentData parseFragment(const QString &branchXmlContent);

    // Вычисление textSha записей фрагмента.
    // contentProvider: возвращает содержимое файла записи
    //   (пустой массив — файл недоступен/не найден).
    static void fillTextShas(FragmentData &fragment,
                             const std::function<QByteArray(const DiffRecordData &)> &contentProvider);

    // Построение списка изменений между двумя фрагментами.
    // buildChangeList сама не считает textSha — вызывающий обязан
    // вызывать fillTextShas для обоих фрагментов заранее.
    static QList<DiffChange> buildChangeList(const FragmentData &baseline,
                                             const FragmentData &head);
};

#endif // _BRANCHDIFFENGINE_H_
#ifndef TREENODE_H
#define TREENODE_H

#include <QModelIndex>
#include <QObject>
#include <QMap>
#include <QSet>
#include <QList>
#include <QDomElement>
#include <QDateTime>
#include <QRegularExpression>

#include "TreeModel.h"

// KnowTreeModel - класс с удобными методами для управления данными дерева
// Внимание! Данный класс наследуется от класса TreeModel


class ClipboardBranch;
class Record;
class QXmlStreamWriter;

// Легкий снимок для выбора заметки: имя, ветка, метки.
// Один проход по дереву, только поля из памяти, без чтения файлов.
// locked = ветка под закрытым шифром (в выбор не попадает).
// Нижние регистры предподсчитаны чтобы фильтр не lower()ил на кейстрок
struct NoteIndexEntry
{
  QString id;
  QString name;
  QString branch;
  QString branchPath;
  QString tags;
  QString nameLower;
  QString branchLower;
  QString tagsLower;
  bool locked=false;
};

// Одна строка детализации статистики: откуда что посчиталось
struct BaseStatRow
{
  QString branchPath; // Имена веток от корня через " / "
  QString noteName;
  QString recordId;
  int files=0;
  int images=0;
  int links=0;
  int tags=0;
};

class KnowTreeModel : public TreeModel
{
    Q_OBJECT

public:
    KnowTreeModel(QObject *parent = nullptr); // KnowTreeModel(const QStringList &headers, QDomDocument domModel, QObject *parent = 0);
    ~KnowTreeModel();

    //! Загрузка данных из файла формата mytetra.xml
    //! Полное имя файла запоминается
    void initFromXML(QString fileName);

    //! Перечитывание данных из файла формата mytetra.xml
    //! Используется имя файла, запомненное в initFromXML()
    void reload(void);

    //! Очистка модели
    void clear(void);

    QDomElement exportFullModelDataToDom(TreeItem *root);
    void exportFullModelDataToStreamWriter(QXmlStreamWriter *xmlWriter, TreeItem *root);

    //! Сохранения данных дерева в XML-файл
    //! Используется имя файла, запомненное в initFromXML()
    void save(void);

    //! Добавление новой подветки к указанной ветке
    void addNewChildBranch(const QModelIndex &index, QMap<QString, QString> branchFields);

    //! Добавление новой ветки после указанной ветки
    void addNewSiblingBranch(const QModelIndex &index, QMap<QString, QString> branchFields);

    //! Добавление новой подветки к Item элементу
    void addNewBranch(TreeItem *parent, QMap<QString, QString> branchFields);


    //! Перемещение ветки вверх
    QModelIndex moveUpBranch(const QModelIndex &index);

    //! Перемещение ветки вниз
    QModelIndex moveDownBranch(const QModelIndex &index);

    //! Перемещение ветки к другой ветке с сохранением идентификатора.
    //! asChild=true - сделать подветкой targetId, иначе - соседней после нее.
    //! Файлы записей не трогаются, т.к. идентификаторы не меняются.
    //! Возвращает false если ветка не найдена или цель внутри источника
    bool moveBranch(const QString &sourceId, const QString &targetId, bool asChild);

    //! Перемещение ветки к новому родителю (смена родителя поддерева).
    //! Ветка встает в конец детей нового родителя. Уже на месте считается
    //! успехом без действий. Тонкая обертка над moveBranch.
    bool moveBranchToParent(TreeItem *item, TreeItem *newParent);


    //! Получение индекса подчиненного элемента с указанным номером
    QModelIndex indexChildren(const QModelIndex &parent, int n) const;
    
    QModelIndex getIndexByItem(TreeItem *item);

    //! Получение ветки с указанным ID (поиск ветки)
    TreeItem *getItemById(const QString &id);

    //! Возвращает общее количество записей, хранимых в дереве
    int getAllRecordCount(void);

    //! Возвращает общее количество веток в дереве (без невидимого корня)
    int getBranchCount(void) const;

    //! Общая статистика базы: ветки, записи, приаттаченные файлы,
    //! файлы картинок на диске в каталогах записей, distinct-ссылки
    //! на всю базу (одна ссылка в десяти заметках - одна ссылка; Qt
    //! дробит одну ссылку на несколько <a href>, голый подсчет врет)
    //! и distinct-метки. Тексты читаются тихо (нет файла = пусто,
    //! закрытый шифр = пусто), модель не меняется.
    //! Считается один раз, дальше кеш: точечные обновления строк
    //! (refreshRecordStats) и сброс флага на структурных правках
    void getBaseStats(int &branches, int &records, int &files, int &images, int &links, int &tags);

    //! Построчная детализация статистики для диалога по клику:
    //! каждая запись со своим путем, счетчиками и идентификатором
    //! (двойной клик прыгает к заметке). Тот же тихий проход что статистика
    QList<BaseStatRow> getBaseStatRows(void);

    //! Сбросить кеш статистики: следующий запрос посчитает все заново.
    //! Вызывается на структурных правках (ветки, bulk-операции),
    //! построчные правки идут через refresh/remove без сброса
    void invalidateStats(void);

    //! Пересчитать одну запись (текст, метки, аттачи): distinct-наборы
    //! правятся через счетчики ссылок, полного прохода нет.
    //! Нет записи - строка тихо выкидывается. При грязном кеше
    //! сначала полный пересчет
    void refreshRecordStats(const QString &recordId);

    //! Выкинуть одну запись из кеша (удаление). При грязном кеше
    //! ничего не делает: пересчет все равно впереди
    void removeRecordStats(const QString &recordId);
    
    //! Возвращает количество записей в ветке и всех подветках
    int getRecordCountForItem(const TreeItem *item);

    QString pasteNewChildBranch(const QModelIndex &index, ClipboardBranch *subbranch);
    QString pasteNewSiblingBranch(const QModelIndex &index, ClipboardBranch *subbranch);

    //! Данные для вида. Переопределен чтобы красить вырезанную ветку серым
    QVariant data(const QModelIndex &index, int role) const;

    //! ID ветки в состоянии "вырезана" (серая, ждет вставки-перемещения).
    //! Пустая строка означает отсутствие вырезания. Состояние только
    //! в памяти, в mytetra.xml не сохраняется и после перезагрузки сброшено
    QString cutBranchId(void) const;
    void setCutBranchId(const QString &id);
    void clearCutBranchId(void);

    void reEncrypt(QString previousPassword, QString currentPassword);

    bool isContainsCryptBranches(void);
    bool isItemContainsCryptBranches(TreeItem *item);

    bool isItemContainsBlockRecords(TreeItem *item);

    QStringList getRecordPath(const QString &recordId);

    Record *getRecord(const QString &recordId);

    //! Проверка наличия идентификатора ветки во всем дереве
    bool isItemIdExists(QString findId) const;

    //! Проверка наличия идентификатора записи во всем дереве
    bool isRecordIdExists(QString findId) const;

    //! Получение списка всех идентификаторов записей в ветке и ее подветках
    QSharedPointer< QSet<QString> > getRecordsIdList(TreeItem *item) const;

    //! Получение списка всех идентификаторов записей в базе
    QSharedPointer< QSet<QString> > getAllRecordsIdList() const;

    //! Плоский снимок всех записей для быстрого фильтра (выбор
    //! заметки,go to). Один проход, без чтения файлов и повторных
    //! поисков по дереву: тормозной вариант "список id + getRecord
    //! + getRecordPath на каждую запись на каждый кейстрок" вешал
    //! приложение на тысячах заметок
    QList<NoteIndexEntry> getNoteIndex(void);

    void deleteItemsByModelIndexList(QModelIndexList &selectItems);

    //! Выгрузка ветки и всех подветок в директорию в виде отдельной базы MyTetra (экспорт)
    bool exportBranchToDirectory(TreeItem *startItem, QString exportDir);

    //! Загрузка отдельной базы MyTetra в виде подветки (импорт)
    QString importBranchFromDirectory(TreeItem *startItem, QString importDir);

    QDateTime getLastSaveDateTime();
    QDateTime getLastLoadDateTime();

    QString getXmlFileName() const;


signals:

    void doCloseDetachedWindowByIdSet( QSet<QString> ids );

private:

    QString m_xmlFileName;

    // ID вырезанной ветки, см. cutBranchId(). Не поле данных, не сохраняется
    QString cutBranchIdValue;

    QDateTime m_lastSaveDateTime;
    QDateTime m_lastLoadDateTime;

    void init(QDomDocument *domModel);

    //! Функция заполнения дерева из DOM-документа
    void setupModelData(QDomDocument *dommodel, TreeItem *parent);

    //! Преобразование DOM в Item модель. Функция рекурсивного обхода элементов DOM-документа
    void parseNodeElement(QDomElement n, TreeItem *parent);

    //! Преобразование Item в DOM модель
    void parseTreeToDom(QDomDocument *doc, QDomElement *xmlData, TreeItem *currItem);
    void parseTreeToStreamWriter( QXmlStreamWriter *xmlWriter, TreeItem *currItem);

    //! Стандартный DOM-документ для сохранения дерева знаний
    QDomDocument createStandartDocument();

    //! Стандартный root-элемент с информацией о версии формата данных
    QDomElement createStandartRootElement(QDomDocument &doc);

    //! Расшифровка зашифрованных атрибутов документа
    //! Возвращают false, если копирование связанных данных не удалось
    bool exportRelatedDataAndDecryptIfNeed(QDomDocument &doc, QString exportDir);
    bool exportRelatedDataAndDecryptIfNeedRecurse(QDomElement &element, QString exportDir);
    
    //! Перемещение ветки вверх или вниз
    QModelIndex moveUpDownBranch(const QModelIndex &index,int direction);

    int getAllRecordCountRecurse(const TreeItem *item, int mode) const;

    int getBranchCountRecurse(const TreeItem *item) const;

    // Кеш статистики: строки по id в порядке дерева, наборы ссылок
    // и меток построчно (для refcount distinct-итогов), сами итоги
    // как счетчики (сколько записей ссылается/метит)
    bool m_statsDirty=true;
    QMap<QString, BaseStatRow> m_statsRows;
    QStringList m_statsOrder;
    QMap<QString, QSet<QString>> m_statsRowLinks;
    QMap<QString, QSet<QString>> m_statsRowTags;
    QMap<QString, int> m_linkRefs;
    QMap<QString, int> m_tagRefs;

    void ensureStats(void);

    // Посчитать строку одной записи. Ссылки и метки отдаются наружу
    // наборами для refcount, в строку идут только счетчики
    BaseStatRow computeStatsRow(Record *record, TreeItem *branchItem,
                                QSet<QString> &noteLinks, QSet<QString> &noteTags,
                                const QRegularExpression &hrefPattern);

    void refStatsSets(const QSet<QString> &links, const QSet<QString> &tags);
    void unrefStatsRow(const QString &recordId);

    QString pasteSubbranchRecurse(TreeItem *item,
                                  QString startBranchId,
                                  ClipboardBranch *subbranch);

    TreeItem *getItemByIdRecurse(TreeItem *item, const QString &id, int mode);

    bool isContainsCryptBranchesRecurse(TreeItem *item, int mode);

    bool isContainsBlockRecordsRecurse(TreeItem *item, int mode);

    //! Добавление подветки из буфера обмена относительно указанного элемента
    QString pasteSubbranch(TreeItem *item, ClipboardBranch *subbranch);

    QStringList getRecordPathRecurse(TreeItem *item,
                                     QStringList &currentPath,
                                     const QString &recordId,
                                     int mode);

    bool isItemIdExistsRecurse(TreeItem *item, QString findId, int mode) const;

    bool isRecordIdExistsRecurse(TreeItem *item, QString findId, int mode) const;

    void getAllRecordsIdListRecurse(TreeItem *item, QSet<QString> *ids) const;

    //! Удаление одной ветки и её подветок
    void deleteOneBranch(QModelIndex index);

    bool isRecordDirExists(QString findDir) const;
    bool isRecordDirExistsRecurse(TreeItem *item, QString findDir, int mode) const;

    bool checkFormat(QDomElement elementFormat);

    bool updateSubVersionFrom1To2(void);


    //! Нахождение совпадающих ID в импортируемых данных и в основной базе для веток
    //! Данный метод используется при импорте
    QMap<QString, QString> getAttributeTranslateTable(QDomDocument &doc, QString elementName, QString attributeName);

    //! Копирование директорий импортируемой ветки в основную базу
    //! Данный метод используется при импорте
    bool copyImportRecordDirectories( QDomDocument &doc,
                                      QString importDir,
                                      QMap<QString, QString> idRecordTranslate,
                                      QMap<QString, QString> dirRecordTranslate );

    //! Преобразование DOM-документа согласно таблицам трансляции
    //! Данный метод используется при импорте
    void translateImportDomData(QDomDocument &doc, QString elementName, QString elementAttribute, QMap<QString, QString> translateTable );
};

#endif // TREENODE_H

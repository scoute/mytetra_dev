#ifndef _TAGSPANEL_H_
#define _TAGSPANEL_H_

#include <QWidget>
#include <QMap>
#include <QTableWidget>

// Панель списка тегов базы со счетчиками использования.
// Переключается из меню Tools и горячей клавишей, место не занимает.
// Двойной клик по тегу запускает глобальный поиск как клик по тегу в заметке

class QLineEdit;
class QTableWidgetItem;
class QShowEvent;
class TreeItem;


// Таблица тегов с ограниченным sizeHint по ширине: тег с очень длинным
// именем не должен раздувать док и сужать основное окно
class TagsTable : public QTableWidget
{
    Q_OBJECT

public:

    explicit TagsTable(QWidget *parent=nullptr);

    // Потолок ширины sizeHint. Вызывать после пересборки содержимого
    void setMaxContentWidth(int width);

    virtual QSize sizeHint(void) const override;

private:

    int maxContentWidth;
};


// Элемент колонки количества в панели меток: сортировка числовая
class CountTableWidgetItem : public QTableWidgetItem
{
public:

    explicit CountTableWidgetItem(int count);

    bool operator<(const QTableWidgetItem &other) const override;

private:

    int countValue;
};

class TagsPanel : public QWidget
{
    Q_OBJECT

public:

    TagsPanel(QWidget *parent=nullptr);
    virtual ~TagsPanel(void);

    // Собрать словарь тег->количество по ветке и подветкам.
    // Регистр сводится: пишется первое встречное написание, счет суммируется.
    // Зашифрованные ветки без пароля пропускаются как в поиске
    static void collectTagCounts(const TreeItem *curritem,
                                 QMap<QString, int> &counts,
                                 QMap<QString, QString> &display);

    // Собрать записи с тегом: пары ветка и строка таблицы.
    // Сравнение без учета регистра. Зашифрованные ветки пропускаются
    static void collectTagTargets(TreeItem *curritem,
                                  const QString &tagLower,
                                  QList< QPair<TreeItem *, int> > &targets);

    // Заменить тег в списке целиком без учета регистра.
    // Возвращает новый список, в changed было ли изменение
    static QStringList replaceTagInList(const QStringList &tags,
                                        const QString &oldLower,
                                        const QString &newSpelling,
                                        bool &changed);

    // Убрать тег из списка целиком без учета регистра.
    // Возвращает новый список, в changed было ли изменение
    static QStringList removeTagFromList(const QStringList &tags,
                                         const QString &oldLower,
                                         bool &changed);

protected:

    // Словарь пересобирается при каждом показе: теги могли измениться
    void showEvent(QShowEvent *event) override;

private slots:

    void onFilterChanged(const QString &text);
    void onTagClicked(int row, int column);
    void onTagsContextMenu(const QPoint &pos);
    void onRenameTag(void);
    void onDeleteTag(void);

    // Пользователь подвигал границу колонки: ширина запоминается
    // чтобы пересборка ее не сбрасывала
    void onSectionResized(int logicalIndex, int oldSize, int newSize);

public slots:

    // Пересборка таблицы по всему дереву. Публичный для обновления
    // извне: смена хранилища, сохранение метаданных через сигнал
    void refreshTags(void);

private:

    // Собрать записи с тегом через дерево. Пустой список значит тег уже исчез
    void collectTagTargetsFromTree(const QString &tagLower,
                                   QList< QPair<TreeItem *, int> > &targets);

    // Есть ли в базе тег кроме переименовываемого. Нужно для блокировки слияния
    bool tagExistsInBase(const QString &tagLower, const QString &excludeLower);

    // Восстановить выделение метки после пересборки таблицы
    void restoreTagSelection(int selectedRow, const QString &selectedTag);

    // Сохранить базу, обновить панель и строку меток открытой заметки.
    // Пустое newSpelling значит удаление
    void saveBaseAndRefresh(const QString &oldLower, const QString &newSpelling);

    void setupUi(void);
    void assembly(void);
    void setupSignals(void);

    // Строка фильтра и таблица тег-количество
    QLineEdit *filterEdit;
    TagsTable *tagsTable;

    // Подписка на сохранение метаданных дерева делается один раз
    // и лениво: в конструкторе treeScreen может еще не существовать
    bool treeMetadataConnected;

    // Ширина колонки тегов, заданная пользователем вручную.
    // Отрицательная значит автоширина по содержимому с потолком
    int tagColumnWidth;

    // Свои программные ресайзы не запоминать как пользовательские
    bool resizingProgrammatically;
};

#endif /* _TAGSPANEL_H_ */

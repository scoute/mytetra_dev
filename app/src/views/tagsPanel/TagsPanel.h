#ifndef _TAGSPANEL_H_
#define _TAGSPANEL_H_

#include <QWidget>
#include <QMap>

// Панель списка тегов базы со счетчиками использования.
// Переключается из меню Tools и горячей клавишей, место не занимает.
// Клик по тегу запускает глобальный поиск как клик по тегу в заметке

class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class QShowEvent;
class TreeItem;

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

public slots:

    // Пересборка таблицы по всему дереву. Публичный для обновления
    // извне: смена хранилища, сохранение метаданных через сигнал
    void refreshTags(void);

protected:

    // Словарь пересобирается при каждом показе: теги могли измениться
    void showEvent(QShowEvent *event) override;

private slots:

    void onFilterChanged(const QString &text);
    void onTagClicked(int row, int column);
    void onTagsContextMenu(const QPoint &pos);
    void onRenameTag(void);
    void onDeleteTag(void);

private:

    // Собрать записи с тегом через дерево. Пустой список значит тег уже исчез
    void collectTagTargetsFromTree(const QString &tagLower,
                                   QList< QPair<TreeItem *, int> > &targets);

    // Есть ли в базе тег кроме переименовываемого. Нужно для блокировки слияния
    bool tagExistsInBase(const QString &tagLower, const QString &excludeLower);

    // Сохранить базу, обновить панель и строку меток открытой заметки.
    // Пустое newSpelling значит удаление
    void saveBaseAndRefresh(const QString &oldLower, const QString &newSpelling);

    void setupUi(void);
    void assembly(void);
    void setupSignals(void);

    // Строка фильтра и таблица тег-количество
    QLineEdit *filterEdit;
    QTableWidget *tagsTable;

    // Подписка на сохранение метаданных дерева делается один раз
    // и лениво: в конструкторе treeScreen может еще не существовать
    bool treeMetadataConnected;
};

#endif /* _TAGSPANEL_H_ */

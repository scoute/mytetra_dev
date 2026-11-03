#ifndef HTMLTAGTREE_H
#define HTMLTAGTREE_H

#include <QTreeWidget>

class HtmlSourceEdit;

// Дерево HTML-тегов для диалога исходника: структура документа слева,
// клик прыгает к тегу в редакторе, движение курсора подсвечивает узел.
// Парсер свой, легковесный (тот же регекс что поиск пары): комментарии
// пропускаются, пустые элементы - листья. Самодостаточен: AppContext
// не трогает, покрывается headless-пробой
class HtmlTagTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit HtmlTagTree(QWidget *parent=nullptr);
    virtual ~HtmlTagTree();

    void setEditor(HtmlSourceEdit *editor);

    // Перестроить дерево по текущему документу редактора
    void rebuild(void);

    // Узел, содержащий позицию pos (самый глубокий). Null если нет
    QTreeWidgetItem *itemForPosition(int pos) const;

public slots:
    // Подсветить узел под курсором редактора (без захвата фокуса)
    void syncToCursor(void);

    // Прыгнуть редактором к тегу узла
    void jumpToItem(QTreeWidgetItem *item);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    HtmlSourceEdit *m_editor=nullptr;
    bool m_sync=false;

    // Позиции комментариев <!-- -->: совпадения внутри них пропускаются
    static QList<QPair<int, int>> commentRanges(const QString &text);

    // Короткая подпись тега: имя + #id + .class (обрезано)
    static QString tagLabel(const QString &tagText);

    // Лимит узлов: гигантский HTML не должен вешать диалог
    static const int maxNodes=5000;
};

#endif // HTMLTAGTREE_H

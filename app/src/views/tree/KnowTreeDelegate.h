#ifndef __KNOWTREEDELEGATE_H__
#define __KNOWTREEDELEGATE_H__

#include <QStyledItemDelegate>
#include <QString>


// Делегат дерева веток. Штатная отрисовка сохраняется полностью,
// добавляются только оверлеи: вырезанная ветка и совпадение фильтра.
//
// Подложки рисуются именно здесь, а не через роль BackgroundRole модели:
// штатный делегат заливает выделенную курсором строку цветом selection
// поверх любых ролей модели, и серая вырезанная ветка под курсором была
// бы не видна. Полупрозрачный оверлей виден и под выделением, и без него
class KnowTreeDelegate : public QStyledItemDelegate
{
 Q_OBJECT

public:
 explicit KnowTreeDelegate(QObject *parent=nullptr);
 virtual ~KnowTreeDelegate();

 // Подстрока фильтра для подсветки в именах веток.
 // Пустая — обычная отрисовка без накладных расходов
 void setFilterText(const QString &text);

 void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:

 // Текст с подсвеченными совпадениями (все вхождения).
 // Пустой результат — совпадение съедено троеточием, рисовать штатно
 QString highlightedHtml(const QString &text, const QFont &font, int width) const;

 QString m_filterText;
};

#endif // __KNOWTREEDELEGATE_H__

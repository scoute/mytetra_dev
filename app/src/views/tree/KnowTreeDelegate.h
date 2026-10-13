#ifndef __KNOWTREEDELEGATE_H__
#define __KNOWTREEDELEGATE_H__

#include <QStyledItemDelegate>


// Делегат дерева веток. Штатная отрисовка сохраняется полностью,
// добавляется только подсветка вырезанной ветки.
//
// Подложка рисуется именно здесь, а не через роль BackgroundRole модели:
// штатный делегат заливает выделенную курсором строку цветом selection
// поверх любых ролей модели, и серая вырезанная ветка под курсором была
// бы не видна. Полупрозрачный оверлей виден и под выделением, и без него
class KnowTreeDelegate : public QStyledItemDelegate
{
 Q_OBJECT

public:
 explicit KnowTreeDelegate(QObject *parent=nullptr);
 virtual ~KnowTreeDelegate();

 void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // __KNOWTREEDELEGATE_H__

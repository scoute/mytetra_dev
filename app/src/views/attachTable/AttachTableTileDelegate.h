#ifndef ATTACHTABLETILEDELEGATE_H
#define ATTACHTABLETILEDELEGATE_H

#include <QStyledItemDelegate>

// Делегат плитки прикрепленных файлов: миниатюра сверху, имя снизу.
// Картинки — уменьшенная копия, текстовые файлы — страница с первыми
// строками (как в галерее), остальное — иконка типа из модели

class AttachTableTileDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit AttachTableTileDelegate(QObject *parent=nullptr);
    virtual ~AttachTableTileDelegate();

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    static const int tileSize=128;
    static const int thumbSize=96;
};

#endif // ATTACHTABLETILEDELEGATE_H

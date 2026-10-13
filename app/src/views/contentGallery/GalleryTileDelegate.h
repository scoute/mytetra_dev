#ifndef _GALLERYTILEDELEGATE_H_
#define _GALLERYTILEDELEGATE_H_

#include <QStyledItemDelegate>
#include <QColor>

// Делегат плиток галереи: ячейка строгий квадрат, картинка вписана
// внутрь, подпись в одну строку. Размер сообщает фиксированный,
// поэтому сетка вида всегда совпадает с покраской (стиль окружения
// геометрию больше не раздувает). Отрисовка жестко клипуется
// прямоугольником пункта: вылезти на соседа невозможно

class GalleryTileDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:

    explicit GalleryTileDelegate(QObject *parent=nullptr);
    virtual ~GalleryTileDelegate(void);

    void setTile(int tile);

    // Цвет подложки ячейки под текущую тему. Им же заливаются поля
    // вокруг вписанной картинки при загрузке иконок
    static QColor cellColor(void);

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:

    QColor frameColor(void) const;
    QColor selectedCellColor(void) const;

    // Сторона квадрата картинки. Сетка = tile+8, подпись +24 снизу
    int tileSize;
};

#endif /* _GALLERYTILEDELEGATE_H_ */

#include <QPainter>
#include <QFontMetrics>

#include "GalleryTileDelegate.h"

#include "models/appConfig/AppConfig.h"


extern AppConfig mytetraConfig;


GalleryTileDelegate::GalleryTileDelegate(QObject *parent) : QStyledItemDelegate(parent),
    tileSize(160)
{

}


GalleryTileDelegate::~GalleryTileDelegate(void)
{

}


void GalleryTileDelegate::setTile(int tile)
{
    tileSize=tile;
}


QColor GalleryTileDelegate::cellColor(void)
{
    if(mytetraConfig.getInterfaceTheme()==QStringLiteral("dark"))
        return QColor(QStringLiteral("#232c36"));

    return QColor(QStringLiteral("#eceff1"));
}


QColor GalleryTileDelegate::frameColor(void) const
{
    if(mytetraConfig.getInterfaceTheme()==QStringLiteral("dark"))
        return QColor(QStringLiteral("#4a545f"));

    return QColor(QStringLiteral("#b9bfc7"));
}


QColor GalleryTileDelegate::selectedCellColor(void) const
{
    if(mytetraConfig.getInterfaceTheme()==QStringLiteral("dark"))
        return QColor(QStringLiteral("#2b3f52"));

    return QColor(QStringLiteral("#d9e9f8"));
}


QSize GalleryTileDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    // Фиксированный размер: сетка вида выставляется ровно таким же,
    // совпадение гарантировано конструкцией а не подбором
    return QSize(tileSize+8, tileSize+8+24);
}


void GalleryTileDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    painter->save();
    painter->setClipRect(option.rect);

    const bool selected=(option.state & QStyle::State_Selected)!=0;
    const bool hovered=!selected && ((option.state & QStyle::State_MouseOver)!=0);

    // Подложка ячейки
    painter->setPen(Qt::NoPen);
    painter->setBrush(selected ? selectedCellColor() : cellColor());
    painter->drawRoundedRect(option.rect.adjusted(0, 0, -1, -1), 6, 6);

    // Картинка вписана в квадрат сверху по центру. Иконки грузятся
    // уже вписанными в tile x tile, тут только центрирование.
    // Иконка берется из индекса напрямую: option.icon заполняет
    // initStyleOption, а paint у нас свой
    const QIcon itemIcon=qvariant_cast<QIcon>(index.data(Qt::DecorationRole));

    if(!itemIcon.isNull())
    {
        const QPixmap pixmap=itemIcon.pixmap(tileSize, tileSize);

        if(!pixmap.isNull())
        {
            const int imageX=option.rect.x()+(option.rect.width()-pixmap.width())/2;
            const int imageY=option.rect.y()+4;
            painter->drawPixmap(imageX, imageY, pixmap);
        }
    }

    // Подпись в одну строку по низу, лишнее многоточится
    const QString text=index.data(Qt::DisplayRole).toString();

    if(!text.isEmpty())
    {
        const int textHeight=24;
        const QRect textRect(option.rect.x()+2,
                             option.rect.bottom()-textHeight+1,
                             option.rect.width()-4,
                             textHeight-2);

        painter->setPen(selected ? option.palette.highlightedText().color()
                                 : option.palette.text().color());

        const QString elidedText=option.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedText);
    }

    // Рамка поверх: синяя у выбранной и наведенной, обычная иначе
    QColor frame=frameColor();

    if(selected || hovered)
        frame=QColor(QStringLiteral("#1A72BB"));

    painter->setPen(frame);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(option.rect.adjusted(0, 0, -1, -1), 6, 6);

    painter->restore();
}

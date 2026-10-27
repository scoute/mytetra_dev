#include <QPainter>
#include <QImageReader>
#include <QApplication>

#include "AttachTableTileDelegate.h"
#include "models/attachTable/AttachTableModel.h"
#include "models/attachTable/Attach.h"
#include "views/contentGallery/ContentGallery.h"


AttachTableTileDelegate::AttachTableTileDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}


AttachTableTileDelegate::~AttachTableTileDelegate()
{
}


void AttachTableTileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    painter->save();

    // Фон выделения и наведения — штатный, чтобы плитка жила
    // в любой теме как остальные вьюхи
    if(option.state & QStyle::State_Selected)
        painter->fillRect(option.rect, option.palette.highlight());
    else if(option.state & QStyle::State_MouseOver)
        painter->fillRect(option.rect, option.palette.brush(QPalette::Highlight).color().lighter(180));

    if(!index.isValid())
    {
        painter->restore();
        return;
    }

    const AttachTableModel *attachModel=qobject_cast<const AttachTableModel *>(index.model());

    QPixmap thumb;
    QString fileName=index.data(Qt::DisplayRole).toString();

    if(attachModel!=nullptr)
    {
        const Attach attach=attachModel->getAttach(index);
        fileName=attach.getField("fileName");

        if(attach.getField("type")==QStringLiteral("file"))
        {
            const QString path=attach.getAbsoluteInnerFileName();

            QImageReader reader(path);

            if(reader.canRead())
            {
                const QSize realSize=reader.size().isValid() ? reader.size() : QSize(thumbSize, thumbSize);
                const QSize scaled=realSize.scaled(thumbSize, thumbSize, Qt::KeepAspectRatio);
                reader.setScaledSize(scaled);

                const QImage image=reader.read();

                if(!image.isNull())
                    thumb=QPixmap::fromImage(image);
            }

            if(thumb.isNull())
            {
                const QPixmap textPage=ContentGallery::renderTextPreview(path, thumbSize);

                if(!textPage.isNull())
                    thumb=textPage;
            }
        }
    }

    // Миниатюра по центру верхней части
    const int thumbX=option.rect.left()+(option.rect.width()-thumbSize)/2;
    const int thumbY=option.rect.top()+4;

    if(!thumb.isNull())
        painter->drawPixmap(thumbX, thumbY+(thumbSize-thumb.height())/2, thumb);
    else
    {
        const QIcon icon=index.data(Qt::DecorationRole).value<QIcon>();

        if(!icon.isNull())
        {
            const QPixmap iconPixmap=icon.pixmap(48, 48);
            painter->drawPixmap(thumbX+(thumbSize-iconPixmap.width())/2,
                                thumbY+(thumbSize-iconPixmap.height())/2,
                                iconPixmap);
        }
    }

    // Имя файла внизу с троеточием
    QFontMetrics metrics(option.font);
    const QString elided=metrics.elidedText(fileName, Qt::ElideMiddle, option.rect.width()-8);

    painter->setPen(option.state & QStyle::State_Selected ?
                    option.palette.highlightedText().color() :
                    option.palette.text().color());
    painter->drawText(option.rect.left()+4, thumbY+thumbSize+4,
                      option.rect.width()-8, metrics.height(),
                      Qt::AlignHCenter, elided);

    painter->restore();
}


QSize AttachTableTileDelegate::sizeHint(const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)

    return QSize(tileSize, tileSize+24);
}

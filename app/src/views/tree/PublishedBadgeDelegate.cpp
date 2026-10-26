#include <QIcon>
#include <QPainter>

#include "PublishedBadgeDelegate.h"

#include "models/tree/TreeItem.h"

PublishedBadgeDelegate::PublishedBadgeDelegate(QObject *parent) : KnowTreeDelegate(parent)
{
    badgeIcon=new QIcon(":/resource/pic/branch_published.svg");
}


PublishedBadgeDelegate::~PublishedBadgeDelegate()
{
    delete badgeIcon;
}


void PublishedBadgeDelegate::setPublishedKeys(const QSet<QString> &keys)
{
    publishedKeys=keys;
}


void PublishedBadgeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    // Сначала штатная отрисовка и подсветка вырезанной ветки из базы,
    // затем поверх бейдж опубликованной
    KnowTreeDelegate::paint(painter, option, index);

    if(publishedKeys.isEmpty())
        return;

    // Определение идентификатора ветки по внутреннему указателю модели
    TreeItem *item=static_cast<TreeItem*>(index.internalPointer());
    if(!item)
        return;

    if(!publishedKeys.contains(item->getField("id")))
        return;

    const int badgeSize=14;
    QPixmap pixmap=badgeIcon->pixmap(badgeSize, badgeSize);
    if(pixmap.isNull())
        return;

    int x=option.rect.right()-badgeSize-2;
    int y=option.rect.top()+(option.rect.height()-badgeSize)/2;

    painter->drawPixmap(x, y, pixmap);
}
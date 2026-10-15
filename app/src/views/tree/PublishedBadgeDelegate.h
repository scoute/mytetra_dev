#ifndef _PUBLISHEDBADGEDELEGATE_H_
#define _PUBLISHEDBADGEDELEGATE_H_

#include "views/tree/KnowTreeDelegate.h"
#include <QSet>
#include <QString>

class QIcon;

// Делегат дерева разделов, рисующий бейдж «рука» на ветках,
// опубликованных в общий каталог (значок по образцу Windows SMB).
// Наследуется от KnowTreeDelegate: поверх его подсветки вырезанной
// ветки дорисовывается бейдж, обе фичи живут в одном делегате.
//
// Набор ключей опубликованных веток задается извне
// (BranchPublisher::listPublishedBranchKeys) и может обновляться
// без пересоздания делегата

class PublishedBadgeDelegate : public KnowTreeDelegate
{
    Q_OBJECT

public:
    explicit PublishedBadgeDelegate(QObject *parent=nullptr);
    ~PublishedBadgeDelegate();

    // Обновление набора ключей опубликованных веток
    void setPublishedKeys(const QSet<QString> &keys);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    QSet<QString> publishedKeys;
    QIcon *badgeIcon;
};

#endif // _PUBLISHEDBADGEDELEGATE_H_
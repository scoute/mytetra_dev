#include <QPainter>

#include "KnowTreeDelegate.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"


KnowTreeDelegate::KnowTreeDelegate(QObject *parent) : QStyledItemDelegate(parent)
{

}


KnowTreeDelegate::~KnowTreeDelegate()
{

}


void KnowTreeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
 // Стандартная отрисовка строки со всеми иконками и выделениями
 QStyledItemDelegate::paint(painter, option, index);

 // Вырезанная ветка подсвечивается желтой подложкой поверх штатной
 // отрисовки. Полупрозрачность сохраняет читаемость текста и оставляет
 // видимым выделение курсором, лишь подкрашивая его
 const KnowTreeModel *treeModel=qobject_cast<const KnowTreeModel *>(index.model());
 if(treeModel==nullptr)
   return;

 QString cutId=treeModel->cutBranchId();
 if(cutId.isEmpty())
   return;

 TreeItem *item=treeModel->getItem(index);
 if(item==nullptr || item->getField("id")!=cutId)
   return;

 painter->save();
 painter->fillRect(option.rect, QColor(255, 235, 130, 110));
 painter->restore();
}

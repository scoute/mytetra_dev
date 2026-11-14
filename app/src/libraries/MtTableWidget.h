#ifndef __MTTABLEWIDGET_H__
#define __MTTABLEWIDGET_H__

#include <QTableWidget>

class QWidget;
class QStyledItemDelegate;

// Этот класс не используется, скорее всего будет удален

// Класс, исправляющий QTableWidget, чтобы правильно применялись QSS-стили

class MtTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    MtTableWidget(QWidget *parent = nullptr);
    virtual ~MtTableWidget();

private:
    QStyledItemDelegate* itemDelegate;

};

#endif // __MTTABLEWIDGET_H__


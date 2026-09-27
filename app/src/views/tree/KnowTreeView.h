#ifndef __KNOWTREEVIEW_H__
#define __KNOWTREEVIEW_H__

#include <QObject>
#include <QTreeView>
#include <QPoint>
#include <QModelIndex>


class QWidget;
class QMouseEvent;
class QDragEnterEvent;
class QDropEvent;
class QTapAndHoldGesture;
class QEvent;
class QGestureEvent;

class KnowTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit KnowTreeView(QWidget *parent = nullptr);
    virtual ~KnowTreeView();

signals:
    void tapAndHoldGestureFinished(const QPoint &);
    void dropEventHandleCatch();

public slots:

protected:

    bool event(QEvent *event);
    bool gestureEvent(QGestureEvent *event);
    void tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture);

    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);

    template <class X> bool isDragableData(X *event);

    // Реакция на кнопки мышки для начала перетаскивания ветки
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    // Начало перетаскивания ветки
    void customStartDrag(void);

    // Завершение перетаскивания ветки: ветка становится подветкой цели
    void dropBranch(QDropEvent *event);

    // Точка нажатия и ветка-кандидат для начала перетаскивания
    QPoint startDragPos;
    QModelIndex startDragIndex;

    // Флаг активного перетаскивания ветки
    bool isDragHappeningNow;

};

#endif // __KNOWTREEVIEW_H__


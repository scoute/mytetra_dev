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
class QPixmap;
class QIcon;
class QFont;

class KnowTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit KnowTreeView(QWidget *parent = nullptr);
    virtual ~KnowTreeView();

    // Картинка перетаскивания: иконка и имя ветки едут за курсором.
    // Плюсик не ставится осознанно: операция перемещение, а не копия
    static QPixmap makeBranchDragPixmap(const QIcon &icon,
                                        const QString &branchName,
                                        const QFont &font);

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


#ifndef ATTACHTABLETILEVIEW_H
#define ATTACHTABLETILEVIEW_H

#include <QListView>
#include <QObject>

// Плитка прикрепленных файлов (альтернатива таблице).
// Та же модель и общий selection model с таблицей (ставит контроллер).
// Двойной клик открывает файл, перетаскивание добавляет файлы

class AttachTableController;
class QMenu;
class QGestureEvent;
class QTapAndHoldGesture;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class AttachTableTileView : public QListView
{
  Q_OBJECT

public:
  AttachTableTileView(QWidget *parent=nullptr);
  virtual ~AttachTableTileView();

  void init(void);

  void setController(AttachTableController *pController);

protected slots:
  void onCustomContextMenuRequested(const QPoint &pos);

protected:
  void setupSignals(void);
  void assemblyContextMenu(void);

  bool event(QEvent *event);
  bool gestureEvent(QGestureEvent *event);
  void tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture);

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

signals:
  void tapAndHoldGestureFinished(const QPoint &);

private:
  QMenu *contextMenu=nullptr;
  AttachTableController *controller=nullptr;
};

#endif // ATTACHTABLETILEVIEW_H

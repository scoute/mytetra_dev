#ifndef __ATTACHTABLEVIEW_H__
#define __ATTACHTABLEVIEW_H__

#include <QTableView>
#include <QObject>

// Отображение таблицы с файлами (только таблица)


class AttachTableController;
class QWidget;
class QMenu;
class QGestureEvent;
class QTapAndHoldGesture;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class AttachTableView : public QTableView
{
  Q_OBJECT

public:
  AttachTableView(QWidget *parent=nullptr);
  virtual ~AttachTableView();

  void init(void);

  void setController(AttachTableController *pController);

  int getFirstSelectionPos(void);

  // HTML миниатюры картинки-вложения для тултипа (пусто если не картинка).
  // Публичный чтобы вызывать из проб и внешних мест
  QString attachImageTooltip(const QModelIndex &index) const;


signals:

  void tapAndHoldGestureFinished(const QPoint &);


protected slots:

  void onCustomContextMenuRequested(const QPoint &pos);


protected:

  void setupSignals(void);
  void assemblyContextMenu(void);

  bool event(QEvent *event);
  virtual void resizeEvent(QResizeEvent *event);
  virtual void paintEvent(QPaintEvent *event);
  bool gestureEvent(QGestureEvent *event);
  void tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture);

  // Drag'n'drop файлов: вход/движение подсвечивают прием,
  // бросок уходит в контроллер списком локальных путей
  virtual void dragEnterEvent(QDragEnterEvent *event);
  virtual void dragMoveEvent(QDragMoveEvent *event);
  virtual void dropEvent(QDropEvent *event);

  AttachTableController *controller;

  QMenu *contextMenu;
};

#endif // __ATTACHTABLEVIEW_H__

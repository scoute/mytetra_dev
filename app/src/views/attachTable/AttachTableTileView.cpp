#include <QMenu>
#include <QDebug>
#include <QMimeData>
#include <QUrl>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QGestureEvent>
#include <QTapAndHoldGesture>

#include "AttachTableTileView.h"
#include "AttachTableScreen.h"
#include "AttachTableTileDelegate.h"
#include "controllers/attachTable/AttachTableController.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/GestureHelper.h"


extern GlobalParameters globalParameters;


AttachTableTileView::AttachTableTileView(QWidget *parent) : QListView(parent)
{
  // Плитка: значки с переносом строк, сетка под делегат
  this->setViewMode(QListView::IconMode);
  this->setMovement(QListView::Static);
  this->setResizeMode(QListView::Adjust);
  this->setWrapping(true);
  this->setGridSize(QSize(AttachTableTileDelegate::tileSize+8,
                          AttachTableTileDelegate::tileSize+32));
  this->setSpacing(4);

  this->setSelectionMode(QAbstractItemView::ExtendedSelection);
  this->setSelectionBehavior(QAbstractItemView::SelectItems);

  this->setItemDelegate(new AttachTableTileDelegate(this));

  // Настройка области виджета для кинетической прокрутки
  GestureHelper::setKineticScrollArea( qobject_cast<QAbstractItemView*>(this) );

  // Разрешение принимать жест QTapAndHoldGesture
  grabGesture(Qt::TapAndHoldGesture);

  // Прием файлов перетаскиванием: dropEvent отдает их контроллеру
  // тем же движком что file picker
  setAcceptDrops(true);
}


AttachTableTileView::~AttachTableTileView()
{

}


void AttachTableTileView::init(void)
{
  setupSignals();

  assemblyContextMenu();
  setContextMenuPolicy(Qt::CustomContextMenu);
}


void AttachTableTileView::setController(AttachTableController *pController)
{
  controller=pController;
}


void AttachTableTileView::setupSignals(void)
{
  // Соединение сигнал-слот чтобы показать контекстное меню по правому клику
  connect(this, &AttachTableTileView::customContextMenuRequested,
          this, &AttachTableTileView::onCustomContextMenuRequested);

  // Соединение сигнал-слот чтобы показать контекстное меню по долгому нажатию
  connect(this,       &AttachTableTileView::tapAndHoldGestureFinished,
          this,       &AttachTableTileView::onCustomContextMenuRequested);

  // Двойной клик открывает файл на просмотр/редактирование
  connect(this,       &AttachTableTileView::doubleClicked,
          controller, &AttachTableController::onOpenAttach);
}


void AttachTableTileView::assemblyContextMenu(void)
{
  // Конструирование меню
  contextMenu=new QMenu(this);

  AttachTableScreen *screenPointer=qobject_cast<AttachTableScreen *>(controller->parent());

  contextMenu->addAction(screenPointer->actionAddAttach);
  contextMenu->addAction(screenPointer->actionAddAttachFromUrl);
  contextMenu->addAction(screenPointer->actionEditFileName);
  contextMenu->addAction(screenPointer->actionDeleteAttach);
  contextMenu->addAction(screenPointer->actionSaveAsAttach);
  contextMenu->addSeparator();
  contextMenu->addAction(screenPointer->actionOpenAttach);
  contextMenu->addAction(screenPointer->actionShowAttachInfo);
}


void AttachTableTileView::onCustomContextMenuRequested(const QPoint &pos)
{
  // Включение отображения меню на экране
  contextMenu->exec( viewport()->mapToGlobal(pos) );
}


// Обработчик событий, нужен только для QTapAndHoldGesture (долгое нажатие)
bool AttachTableTileView::event(QEvent *event)
{
  if (event->type() == QEvent::Gesture)
  {
    qDebug() << "In gesture event(): " << event << " Event type: " << event->type();
    return gestureEvent(static_cast<QGestureEvent*>(event));
  }

  return QListView::event(event);
}


// Обработчик жестов
// Вызывается из обработчика жестов event()
bool AttachTableTileView::gestureEvent(QGestureEvent *event)
{
  qDebug() << "In gesture event(): " << event << " Event type: " << event->type();

  if (QGesture *gesture = event->gesture(Qt::TapAndHoldGesture))
    tapAndHoldGestureTriggered(static_cast<QTapAndHoldGesture *>(gesture));

  return true;
}


// Обработчик жеста TapAndHoldGesture
// Вызывается из обработчика жестов gestureEvent()
void AttachTableTileView::tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture)
{
  qDebug() << "In tapAndHoldGestureTriggered()" << gesture;

  if(gesture->state()==Qt::GestureFinished)
    if(globalParameters.getTargetOs()=="android")
      emit tapAndHoldGestureFinished( mapFromGlobal(gesture->position().toPoint()) );
}


// Принимаются только перетаскивания с локальными файлами:
// такой mime дают файловые менеджеры
void AttachTableTileView::dragEnterEvent(QDragEnterEvent *event)
{
  if(event->mimeData()!=nullptr && event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    event->ignore();
}


void AttachTableTileView::dragMoveEvent(QDragMoveEvent *event)
{
  if(event->mimeData()!=nullptr && event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    event->ignore();
}


// Бросок файлов: локальные пути уходят в контроллер,
// нелокальные ссылки молча пропускаются
void AttachTableTileView::dropEvent(QDropEvent *event)
{
  if(event->mimeData()==nullptr || !event->mimeData()->hasUrls())
  {
    event->ignore();
    return;
  }

  QStringList files;

  foreach(const QUrl &url, event->mimeData()->urls())
    if(url.isLocalFile() && !url.toLocalFile().isEmpty())
      files.append(url.toLocalFile());

  if(files.isEmpty())
  {
    event->ignore();
    return;
  }

  event->acceptProposedAction();

  if(controller!=nullptr)
    controller->addDroppedFiles(files);
}

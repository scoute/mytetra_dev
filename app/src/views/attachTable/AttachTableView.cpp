#include <QMenu>
#include <QDebug>
#include <QHeaderView>
#include <QPainter>
#include <QWidget>
#include <QMimeData>
#include <QUrl>
#include <QToolTip>
#include <QHelpEvent>
#include <QImageReader>
#include <QImage>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QGestureEvent>
#include <QTapAndHoldGesture>
#include <QApplication>

#include "AttachTableView.h"
#include "AttachTableScreen.h"
#include "controllers/attachTable/AttachTableController.h"
#include "models/attachTable/AttachTableModel.h"
#include "models/attachTable/Attach.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/GestureHelper.h"


extern GlobalParameters globalParameters;


AttachTableView::AttachTableView(QWidget *parent) : QTableView(parent)
{
  // Имя файла тянется на свободное место, остальные колонки —
  // фиксированные регулируемые вручную. Раньше первый столбец
  // принудительно занимал 80% при любом ресайзе и выдавливал
  // остальные за правый край, а ручная регулировка тут же сбрасывалась.
  // Режимы ставятся в setModel(): на пустом хедере без модели
  // setSectionResizeMode роняет Qt
  this->setSelectionBehavior(QAbstractItemView::SelectRows); // Выделяется вся строка

  // Настройка области виджета для кинетической прокрутки
  GestureHelper::setKineticScrollArea( qobject_cast<QAbstractItemView*>(this) );

  // Разрешение принимать жест QTapAndHoldGesture
  grabGesture(Qt::TapAndHoldGesture);

  // Прием файлов перетаскиванием: dropEvent отдает их контроллеру
  // тем же движком что file picker
  setAcceptDrops(true);
}


AttachTableView::~AttachTableView()
{

}


void AttachTableView::init(void)
{
  setupSignals();

  assemblyContextMenu();
  setContextMenuPolicy(Qt::CustomContextMenu);
}


void AttachTableView::setupSignals(void)
{
  // Сигнал чтобы показать контекстное меню по правому клику на списке записей
  connect(this, &AttachTableView::customContextMenuRequested,
          this, &AttachTableView::onCustomContextMenuRequested);

  // Соединение сигнал-слот чтобы показать контекстное меню по долгому нажатию
  connect(this, &AttachTableView::tapAndHoldGestureFinished,
          this, &AttachTableView::onCustomContextMenuRequested);

  // Сигнал чтобы открыть на просмотр/редактирование файл по двойному клику
  connect(this,       &AttachTableView::doubleClicked,
          controller, &AttachTableController::onOpenAttach);
}


void AttachTableView::setController(AttachTableController *pController)
{
  controller=pController;
}


void AttachTableView::assemblyContextMenu(void)
{
  // Конструирование меню
  contextMenu=new QMenu(this);

  // find_object() не работает, потому что при инициализации еще не вызван метод assembly() у attachTableScreen, и этому объекту не задан родитель layout
  // AttachTableScreen *screenPointer=find_object<AttachTableScreen>("attachTableScreen");
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


// Обработчик событий, нужен только для QTapAndHoldGesture (долгое нажатие)
bool AttachTableView::event(QEvent *event)
{
  if (event->type() == QEvent::Gesture)
  {
    qDebug() << "In gesture event(): " << event << " Event type: " << event->type();
    return gestureEvent(static_cast<QGestureEvent*>(event));
  }

  // Миниатюра картинки-вложения при наведении мыши
  if(event->type()==QEvent::ToolTip)
  {
    QHelpEvent *helpEvent=static_cast<QHelpEvent *>(event);
    const QString html=attachImageTooltip(indexAt(helpEvent->pos()));

    if(html.isEmpty())
      QToolTip::hideText();
    else
      QToolTip::showText(helpEvent->globalPos(), html, this);

    return true;
  }

  return QTableView::event(event);
}


// HTML миниатюры картинки-вложения для тултипа: пусто если строка
// не картинка. Размер читается из заголовка файла, вписывается
// в квадрат maxSide с сохранением пропорций
QString AttachTableView::attachImageTooltip(const QModelIndex &index) const
{
  if(!index.isValid())
    return QString();

  AttachTableModel *attachModel=qobject_cast<AttachTableModel *>(model());

  if(attachModel==nullptr)
    return QString();

  const Attach attach=attachModel->getAttach(index);

  if(attach.getField("type")!=QStringLiteral("file"))
    return QString();

  const QString absoluteFileName=attach.getAbsoluteInnerFileName();

  QImageReader reader(absoluteFileName);

  if(!reader.canRead())
    return QString();

  QSize realSize=reader.size();

  if(!realSize.isValid())
  {
    // Формат не отдал размер из заголовка — грузим целиком
    const QImage full(absoluteFileName);

    if(full.isNull())
      return QString();

    realSize=full.size();
  }

  const int maxSide=256;
  QSize showSize=realSize;

  if(qMax(realSize.width(), realSize.height())>maxSide)
    showSize=realSize.scaled(maxSide, maxSide, Qt::KeepAspectRatio);

  return QStringLiteral("<img src=\"")+
         QUrl::fromLocalFile(absoluteFileName).toString()+
         QStringLiteral("\" width=\"")+QString::number(showSize.width())+
         QStringLiteral("\" height=\"")+QString::number(showSize.height())+
         QStringLiteral("\">");
}


void AttachTableView::resizeEvent(QResizeEvent *event)
{
  // Ширины колонок больше не трогаются: имя тянется режимом Stretch,
  // остальные регулируются вручную и переживают ресайзы
  QTableView::resizeEvent(event);
}


void AttachTableView::paintEvent(QPaintEvent *event)
{
  QTableView::paintEvent(event);

  // Если список приаттаченных файлов пуст
  if(model()!=NULL)
    if(model()->rowCount()==0)
    {
      QPainter painter(viewport());
      painter.setPen( QApplication::palette().color(QPalette::ToolTipText) );
      painter.setFont(QFont("Arial", 14));
      painter.drawText(rect(), Qt::AlignCenter, tr("No attach files"));
    }
}


// Обработчик жестов
// Вызывается из обработчика событий event()
bool AttachTableView::gestureEvent(QGestureEvent *event)
{
  qDebug() << "In gestureEvent()" << event;

  if (QGesture *gesture = event->gesture(Qt::TapAndHoldGesture))
    tapAndHoldGestureTriggered(static_cast<QTapAndHoldGesture *>(gesture));

  return true;
}


// Обработчик жеста TapAndHoldGesture
// Вызывается из обработчика жестов gestureEvent()
void AttachTableView::tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture)
{
  qDebug() << "In tapAndHoldGestureTriggered()" << gesture;

  if(gesture->state()==Qt::GestureFinished)
    if(globalParameters.getTargetOs()=="android")
      emit tapAndHoldGestureFinished( mapFromGlobal(gesture->position().toPoint()) );
}


// Открытие контекстного меню в таблице приаттаченных файлов
void AttachTableView::setModel(QAbstractItemModel *model)
{
  QTableView::setModel(model);

  // Имя файла тянется на свободное место, остальные колонки —
  // фиксированные регулируемые вручную
  this->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
  this->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Interactive );
  this->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Interactive );
  this->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::Interactive );
}


void AttachTableView::onCustomContextMenuRequested(const QPoint &pos)
{
  // Включение отображения меню на экране
  contextMenu->exec( viewport()->mapToGlobal(pos) );
}


// Принимаются только перетаскивания с локальными файлами:
// такой mime дают файловые менеджеры
void AttachTableView::dragEnterEvent(QDragEnterEvent *event)
{
  if(event->mimeData()!=nullptr && event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    event->ignore();
}


void AttachTableView::dragMoveEvent(QDragMoveEvent *event)
{
  if(event->mimeData()!=nullptr && event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    event->ignore();
}


// Бросок файлов: локальные пути уходят в контроллер,
// нелокальные ссылки молча пропускаются
void AttachTableView::dropEvent(QDropEvent *event)
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


// Получение номера первого выделенного элемента
int AttachTableView::getFirstSelectionPos(void)
{
 // Получение списка выделенных Item-элементов
 QModelIndexList selectItems=selectionModel()->selectedIndexes();

 if(selectItems.isEmpty())
  return -1; // Если ничего не выделено
 else
  return (selectItems.at(0)).row(); // Индекс первого выделенного элемента
}

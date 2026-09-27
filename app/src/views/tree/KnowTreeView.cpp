#include <QWidget>
#include <QDebug>
#include <QMimeData>
#include <QMessageBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QTapAndHoldGesture>
#include <QGestureEvent>

#include "KnowTreeView.h"
#include "KnowTreeDelegate.h"
#include "TreeScreen.h"
#include "views/mainWindow/MainWindow.h"
#include "libraries/ClipboardRecords.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/GestureHelper.h"
#include "models/tree/KnowTreeModel.h"
#include "models/recordTable/RecordTableData.h"
#include "models/tree/TreeItem.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/recordTable/RecordTableView.h"
#include "views/record/MetaEditor.h"
#include "controllers/recordTable/RecordTableController.h"


extern GlobalParameters globalParameters;


KnowTreeView::KnowTreeView(QWidget *parent) : QTreeView(parent)
{
    // Разрешение принимать Drop-события
    setAcceptDrops(true);
    setDropIndicatorShown(true);

    // Делегат подсвечивает вырезанную ветку желтой подложкой.
    // Видом владеет, удалится автоматически
    setItemDelegate(new KnowTreeDelegate(this));

    // Разрешение принимать жест QTapAndHoldGesture
    grabGesture(Qt::TapAndHoldGesture);

    // Настройка области виджета для кинетической прокрутки
    GestureHelper::setKineticScrollArea( qobject_cast<QAbstractItemView*>(this) );

    isDragHappeningNow=false;
}


KnowTreeView::~KnowTreeView()
{

}


// Обработчик событий, нужен только для QTapAndHoldGesture (долгое нажатие)
bool KnowTreeView::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
     {
        qDebug() << "In gesture event(): " << event << " Event type: " << event->type();
        return gestureEvent(static_cast<QGestureEvent*>(event));
     }

    return QTreeView::event(event);
}


// Обработчик жестов
// Вызывается из обработчика событий
bool KnowTreeView::gestureEvent(QGestureEvent *event)
{
    qDebug() << "In gestureEvent()" << event;

    if (QGesture *gesture = event->gesture(Qt::TapAndHoldGesture))
    {
        tapAndHoldGestureTriggered(static_cast<QTapAndHoldGesture *>(gesture));
    }

    return true;
}


// Обработчик жеста TapAndHoldGesture
// Вызывается из обработчика жестов
void KnowTreeView::tapAndHoldGestureTriggered(QTapAndHoldGesture *gesture)
{
    qDebug() << "In tapAndHoldGestureTriggered()" << gesture;

    if(gesture->state()==Qt::GestureFinished)
    {
        if(globalParameters.getTargetOs()=="android")
        {
            emit tapAndHoldGestureFinished( mapFromGlobal(gesture->position().toPoint()) );
        }
    }
}


// Начало перетаскивания, вызывается ОДИН РАЗ
// когда курсор с перетаскиваемым объектом ВПЕРВЫЕ входит в виджет
void KnowTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if( isDragableData(event) )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}


// Движение мышкой при перетаскивании, вызывается МНОГОКРАТНО
// при каждом перемещении курсора ВНУТРИ виджета
void KnowTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    if( isDragableData(event) )
    {
        event->acceptProposedAction();

        // Выясняется элемент дерева, над которым находится курсор
        QModelIndex index=indexAt(event->pos());

        // Указатель на родительский элемент, чтобы далее получить модель данных
        TreeScreen *parentPointer=qobject_cast<TreeScreen *>( parent() );

        // В модели данных отмечается элемент дерева, над которым находится курсор
        parentPointer->knowTreeModel->setData(index, QVariant(true), Qt::UserRole);
    }
    else
    {
        event->ignore();
    }
}


template <class X>
bool KnowTreeView::isDragableData(X *event)
{
    // Проверяется, содержит ли объект переноса данные нужного формата
    const QMimeData *mimeData=event->mimeData();
    if(mimeData==nullptr)
    {
        return false;
    }

    // Перенос записей из таблицы записей
    if(mimeData->hasFormat(FixedParameters::appTextId+"/records"))
    {
        QObject *sourceObject=qobject_cast<QObject *>( event->source() );

        if(sourceObject!=nullptr && sourceObject->objectName()=="recordTableView")
        {
            return true;
        }

        return false;
    }

    // Перенос ветки внутри этого же дерева
    if(mimeData->hasFormat(FixedParameters::appTextId+"/branchmove"))
    {
        return event->source()==this;
    }

    return false;
}


void KnowTreeView::dropEvent(QDropEvent *event)
{
    qDebug() << "dropEvent() - Start";

    emit dropEventHandleCatch();

    // Перетаскивание ветки обрабатывается отдельно от переноса записей:
    // в объекте переноса лежит только идентификатор ветки
    if(event->mimeData()!=nullptr &&
       event->mimeData()->hasFormat(FixedParameters::appTextId+"/branchmove") &&
       event->source()==this)
    {
        qDebug() << "Try move branch by drag and drop";

        dropBranch(event);

        return;
    }

    if( isDragableData(event) )
    {
        qDebug() << "Try move record by drag and drop";

        // Извлечение объекта
        const ClipboardRecords *clipboardRecords;
        clipboardRecords=qobject_cast<const ClipboardRecords *>(event->mimeData());

        // Печать в консоль содержимого перетаскиваемого объекта (для отладки)
        clipboardRecords->print();

        // Выясняется элемент дерева, над которым был сделан Drop
        QModelIndex index=indexAt(event->pos());

        // Если отпускание мышки произошло не на ветке дерева,
        // а на свободном пустом месте в области виджета дерева
        if(!index.isValid())
        {
            return;
        }

        // Указатель на родительский виджет
        TreeScreen *parentPointer=qobject_cast<TreeScreen *>( parent() );

        // Выясняется ссылка на элемент дерева (на ветку), над которым был совершен Drop
        TreeItem *treeItemDrop=parentPointer->knowTreeModel->getItem(index);

        // Выясняется ссылка на таблицу данных ветки, над которой совершен Drop
        RecordTableData *recordTableData=treeItemDrop->recordtableGetTableData();

        // Исходная ветка в момент Drop (откуда переностся запись) - это
        // выделенная курсором ветка
        QModelIndex indexFrom = find_object<TreeScreen>("treeScreen")->getCurrentItemIndex();

        // Выясняется ссылка на элемент дерева (на ветку), откуда переностся запись
        TreeItem *treeItemDrag=parentPointer->knowTreeModel->getItem(indexFrom);

        // Если перенос происходит в ту же самую ветку
        if(indexFrom==index)
        {
            return;
        }

        // Если перенос происходит из не зашифрованной ветки в зашифрованную,
        // а пароль не установлен
        if(treeItemDrag->getField("crypt")!="1" &&
                treeItemDrop->getField("crypt")=="1" &&
                globalParameters.getCryptKey().length()==0)
        {
            // Выводится уведомление что невозможен перенос без пароля
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("Warning!"));
            msgBox.setText( tr("Unable to move the item to an encrypted item. You have to enter the password for this action.") );
            msgBox.setIcon(QMessageBox::Information);
            msgBox.exec();

            return;
        }


        // Перенос записей, хранящихся в MimeData
        // В настоящий момент в MimeData попадает только одна запись,
        // но в дальнейшем планируется переносить несколько записей
        // и здесь код подготовлен для переноса нескольких записей
        RecordTableController *recordTableController=find_object<RecordTableController>("recordTableController");  // Указатель на контроллер таблицы конечных записей
        for(int i=0; i<clipboardRecords->getCount(); i++)
        {
            // Полные данные записи
            Record record=clipboardRecords->getRecord(i);

            qDebug() << " Before delete, cursor at row: " << recordTableController->getView()->currentIndex().row();

            // Удаление записи из исходной ветки, удаление должно быть вначале,
            // чтобы сохранился ID записи.
            // В этот момент вид таблицы конечных записей показывает таблицу,
            // из которой совершается Drag.
            // TreeItem *treeItemFrom=parentPointer->knowTreeModel->getItem(indexFrom);
            recordTableController->removeRowById( record.getField("id") );

            qDebug() << " After delete, cursor at row: " << recordTableController->getView()->currentIndex().row();

            // Если после удаления перемещаемой записи в таблице остались
            // еще какие-то записи
            if(recordTableController->getRowCount()>0)
            {
                // Происходит виртуальный клик по записи, на которой
                // стоит курсор после удаления переносимой записи.
                // Это нужно чтобы обновился текст записи, так как курсор
                // в таблице записей после удаления остается на месте
                // и никаких событий изменения selection model не генерируются,
                // соответственно автоматического обновления не происходит,
                // и нужно делать виртуальный клик
                recordTableController->clickToRecord( recordTableController->getView()->currentIndex() );
            }
            else
            {
                // Иначе таблица конечных записей после удаления перемещенной записи
                // стала пустой. Нужно очистить поле редактирования чтобы
                // не видно было текста последней удаленной записи
                find_object<MetaEditor>("editorScreen")->clearAll();
            }

            find_object<RecordTableScreen>("recordTableScreen")->toolsUpdate();

            // Добавление записи в базу
            recordTableData->insertNewRecord(GlobalParameters::AddNewRecordBehavior::ADD_TO_END,
                                             0,
                                             record);

            // Сохранение дерева веток
            find_object<TreeScreen>("treeScreen")->saveKnowTree();
        }

        // Обновление исходной ветки чтобы было видно что записей убавилось
        parentPointer->updateBranchOnScreen(indexFrom);

        // Обновлении конечной ветки чтобы было видно что записей прибавилось
        parentPointer->updateBranchOnScreen(index);

        // В модели данных дерева обнуляется элемент,
        // который подсвечивался при Drag And Drop
        parentPointer->knowTreeModel->setData(QModelIndex(), QVariant(false), Qt::UserRole);
    }
}


// Реакция на нажатие кнопок мышки
// Индекс запоминается по координатам, так как событие приходит
// до выделения ветки под курсором
void KnowTreeView::mousePressEvent(QMouseEvent *event)
{
    if(event->buttons()==Qt::LeftButton)
    {
        startDragPos=event->pos();

        if(indexAt(event->pos()).isValid())
        {
            startDragIndex=indexAt(event->pos());
        }
        else
        {
            startDragIndex=QModelIndex();
        }
    }

    // При клике перетаскивание еще не начинается,
    // и флаг от предыдущего перетаскивания надо очистить
    isDragHappeningNow=false;

    QTreeView::mousePressEvent(event);
}


// Реакция на движение мышкой
void KnowTreeView::mouseMoveEvent(QMouseEvent *event)
{
    // Если при движении нажата левая кнопка мышки
    // и выбрана ровно одна ветка
    if((event->buttons() & Qt::LeftButton) &&
       selectionModel()->selectedIndexes().size()==1 &&
       startDragIndex.isValid())
    {
        // Выясняется расстояние от места начала нажатия
        int distance=(event->pos() - startDragPos).manhattanLength();

        if(distance >= QApplication::startDragDistance())
        {
            customStartDrag(); // Начинается перетаскивание
        }
    }

    // При зажатых кнопках нельзя пробрасывать вызов родительского метода,
    // чтобы Qt не менял выделение в процессе перетаскивания
    if((event->buttons() & Qt::LeftButton) || (event->buttons() & Qt::RightButton))
    {
        return;
    }

    QTreeView::mouseMoveEvent(event);
}


// Реакция на отпускание кнопки мышки
void KnowTreeView::mouseReleaseEvent(QMouseEvent *event)
{
    QTreeView::mouseReleaseEvent(event);

    isDragHappeningNow=false;

    // Индекс перетаскиваемой ветки очищается в любом случае
    startDragIndex=QModelIndex();
}


// Начало переноса ветки
void KnowTreeView::customStartDrag(void)
{
    if(!startDragIndex.isValid())
    {
        return;
    }

    // Модель дерева выставлена виду в TreeScreen, отдельный поход
    // к родителю не нужен
    KnowTreeModel *treeModel=qobject_cast<KnowTreeModel *>( model() );
    if(treeModel==nullptr)
    {
        return;
    }

    TreeItem *dragItem=treeModel->getItem(startDragIndex);
    if(dragItem==nullptr)
    {
        return;
    }

    isDragHappeningNow=true; // Выставляется флаг что началось перетаскивание

    qDebug() << "Start branch drag:" << dragItem->getField("name");

    // В объект переноса кладется только идентификатор ветки.
    // Перемещение выполняет принимающая сторона через moveBranch,
    // поэтому копировать поддерево не нужно
    QDrag *drag=new QDrag(this);
    QMimeData *mimeData=new QMimeData();
    mimeData->setData(FixedParameters::appTextId+"/branchmove",
                      dragItem->getField("id").toUtf8());
    drag->setMimeData(mimeData);

    // Запуск операции перетаскивания объекта
    unsigned int result=drag->exec(Qt::MoveAction);

    // Если перетаскивание завершено вне дерева (или отменено),
    // в модели данных обнуляется оформление элемента,
    // который подсвечивался при Drag And Drop
    if(result==0)
    {
        // todo: Совершенно непонятно, где удалять объект drag.
        // Если удалять в этом месте, имеем сегфолт
        // delete drag;

        treeModel->setData(QModelIndex(), QVariant(false), Qt::UserRole);
    }

    isDragHappeningNow=false;
    startDragIndex=QModelIndex();
}


// Завершение перетаскивания ветки: ветка становится подветкой цели.
// Проверки совпадения и зацикливания внутри moveBranch
void KnowTreeView::dropBranch(QDropEvent *event)
{
    KnowTreeModel *treeModel=qobject_cast<KnowTreeModel *>( model() );
    if(treeModel==nullptr)
    {
        return;
    }

    // Идентификатор перетаскиваемой ветки из объекта переноса
    QString dragId=QString::fromUtf8(event->mimeData()->data(FixedParameters::appTextId+"/branchmove"));

    // Выясняется ветка, над которой был сделан Drop
    QModelIndex index=indexAt(event->pos());

    // Если отпускание мышки произошло не на ветке дерева,
    // а на свободном пустом месте, ничего не делается
    if(!index.isValid())
    {
        return;
    }

    // Выясняется ветка, над которой совершен Drop
    TreeItem *treeItemDrop=treeModel->getItem(index);
    if(treeItemDrop==nullptr)
    {
        return;
    }

    QString dropId=treeItemDrop->getField("id");

    // Перенос ветки на саму себя бессмысленен
    if(dragId==dropId)
    {
        return;
    }

    // Перемещение с сохранением идентификатора.
    // Отказ (цель внутри переносимой ветки) молча игнорируется:
    // дерево просто остается как было
    bool moveok=treeModel->moveBranch(dragId, dropId, true);
    if(!moveok)
    {
        return;
    }

    // Установка курсора на перемещенную ветку.
    // В тестах главного окна нет, тогда только двигается модель
    MainWindow *mainWindow=find_object<MainWindow>("mainwindow");
    TreeItem *movedItem=treeModel->getItemById(dragId);
    if(mainWindow!=nullptr && movedItem!=nullptr)
    {
        QStringList movedPath=movedItem->getPath();
        mainWindow->setTreePosition(movedPath);
    }

    // Сохранение дерева веток
    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen!=nullptr)
    {
        treeScreen->saveKnowTree();
    }

    // В модели данных дерева обнуляется элемент,
    // который подсвечивался при Drag And Drop
    treeModel->setData(QModelIndex(), QVariant(false), Qt::UserRole);
}


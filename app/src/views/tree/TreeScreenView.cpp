#include <QAbstractItemView>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QString>
#include <QMap>
#include <QAction>
#include <QItemSelectionModel>
#include <QVBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QLineEdit>
#include <QInputDialog>

#include "TreeScreen.h"
#include "KnowTreeView.h"

#include "views/recordTable/RecordTableScreen.h"
#include "models/appConfig/AppConfig.h"
#include "views/mainWindow/MainWindow.h"
#include "models/tree/TreeItem.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/GlobalParameters.h"
#include "libraries/crypt/Password.h"
#include "libraries/WindowSwitcher.h"
#include "controllers/recordTable/RecordTableController.h"
#include "libraries/helpers/ObjectHelper.h"

#include "models/appConfig/AppConfigUi.h"


// Обновление на экране ветки и подветок
void TreeScreen::updateBranchOnScreen(const QModelIndex &index)
{
 // Для корневой ветки дается команда чтобы модель сообщила о своем изменении
 knowTreeModel->emitSignalDataChanged(index);

 // По модельному индексу выясняется указатель на ветку
 TreeItem *item=knowTreeModel->getItem(index);

 // Перебираются подветки
 QList<QStringList> updatePathts=item->getAllChildrenPath();
 foreach(QStringList currentPath, updatePathts)
 {
  TreeItem *currentItem=knowTreeModel->getItem(currentPath);

  QModelIndex currentIndex=knowTreeModel->getIndexByItem(currentItem);

  // Для подветки дается команда чтобы модель сообщила о своем изменении
  knowTreeModel->emitSignalDataChanged(currentIndex);
 }
}

// Вспомогательный слот, позволяющий ввести пароль на ветку в случае,
// если в базе только одна корневая ветка и она зашифрована
/*


void TreeScreen::checkIfOneRootCryptItem(const QModelIndex &index)
{
    // Если пароль доступа к зашифрованным данным не вводился в этой сессии
    if(AppContext::instance().globalParameters()->getCryptKey().length()==0) {

        // Указатель на текущую выбранную ветку дерева
        TreeItem *item = knowTreeModel->getItem(index);

        // Корневой элемент дерева (это "технический элемент", он не виден на экране в виде ветки)
        const TreeItem *rootItem = knowTreeModel->getRootItem();

        // Если у корневого элемента дерева только один подчиненный элемент
        if ( rootItem->childCount() == 1 )
        {
            // Значит в дереве имеется только один элемент первого уровня
            // (у него могут быть подчиненные элементы, но это в данном случае не имеет значения)
            TreeItem *firstAndSingleItem = rootItem->child(0);

            // Сравнение указателя на единственный элемент первого уровня
            // с указателем элемента дерева, по которому был произведен клик
            if ( item == firstAndSingleItem )
            {
                // Проверяется, происходит ли клик по зашифрованной ветке
                if(item->getField("crypt")=="1")
                {
                    // Вызывается стандартный клик по ветке, он запустит процедуру ввода пароля
                    onKnowtreeClicked(index);
                }
            }
        }
    }
}
*/

// Действия при клике на ветку дерева через selection-модель


// Действия при клике на ветку дерева через selection-модель
void TreeScreen::onKnowtreeClicked(const QModelIndex &index)
{
    if ( !index.isValid() ) // Если клик был на пустом месте
    {
        return;
    }

    // Обновление выделения курсором и прокрутка к выбранному элементу
    this->knowTreeView->setCurrentIndex(index);
    this->knowTreeView->scrollTo(index, QAbstractItemView::EnsureVisible);

    // Выполнение остального кода произойдет на следующем цикле событий, когда вид обновится
    QTimer::singleShot(0, this, [this, index]() {
        processKnowtreeClicked(index); // Основная обработка
    });
}

// Реальные действия при клике на ветку дерева


// Реальные действия при клике на ветку дерева
void TreeScreen::processKnowtreeClicked(const QModelIndex &index)
{
    if ( !index.isValid() ) // Если клик был на пустом месте
    {
        return;
    }

    // Данный слот может повторно вызываться, когда его работа еще не завершена.
    // Например, в момент завершения синхронизации ветки могут добавиться или удалиться,
    // может сработать восстановление положения курсора после синхронизации.
    // Повторный вызов в пределах одного цикла событий возможен, так как
    // в данном слоте может быть вызван диалог запроса пароля, а диалог
    // может ожидать ввода пользователя неограниченное время.
    // Переменная isKnowtreeClickedWork блокирует работу слота, если он
    // повторно вызван когда он еще не закончил работу

    /*
    if(this->isKnowtreeClickedWork)
    {
        return;
    }
    else
    {
        this->isKnowtreeClickedWork=true; // Следить чтобы перед каждым return данный флаг сбрасывался
    }
    */

    // Запрещается обработка одного и того же индекса в пределах одного цикла обработки событий Qt
    // Это нужно чтобы повторно не обрабатывать индекс, если одновременно при одном клике вызвались
    // и QItemSelectionModel::currentRowChanged и KnowTreeView::pressed
    static QModelIndex lastHandledIndex;
    if ( lastHandledIndex.isValid() && lastHandledIndex == index )
    {
        // this->isKnowtreeClickedWork=false;

        return;
    }
    else
    {
        lastHandledIndex = index;
    }

    // QModelIndex index = nodetreeview->selectionModel()->currentIndex();

    // Сохраняется текст в окне редактирования в соответсвующий файл
    find_object<MainWindow>("mainwindow")->saveTextarea();

    // Указатель на текущую выбранную ветку дерева
    TreeItem *item = knowTreeModel->getItem(index);

    // Все инструменты по работе с _записями_ выключаются
    find_object<RecordTableScreen>("recordTableScreen")->disableAllActions();

    // Проверяется, происходит ли клик по ветке, которая зашифрованна
    // и открывающий ее правильный пароль еще не вводился в этой сессии,
    // то есть ветку невозможно просмотреть
    bool isAccessItem=true;
    if(item->getField("crypt")=="1" and
       AppContext::instance().globalParameters()->getCryptKey().length()==0)
    {
        isAccessItem=false;
    }

    // Инструменты работы с веткой включаются или выключаются
    QMapIterator<QString, QAction *> i(actionList);
    while (i.hasNext()) {
        i.next();
        i.value()->setEnabled( isAccessItem );
    }

    // Если ветка недоступна из-за того что она зашифрована и пароль еще не вводился
    if ( !isAccessItem )
    {
        // Устанавливаются пустые данные для отображения таблицы конечных записей,
        // чтобы в области списка записей было пусто,
        // и пользователю в момент работы окна ввода пароля
        // не показывались записи ветки, на которой до этого момента стоял курсор
        find_object<RecordTableController>("recordTableController")->setTableData(nullptr);

        // Запрашивается пароль
        Password password(m_dispatcher);
        if ( password.retrievePassword() ) // Если пароль введен верно
        {
            // Инструменты работы с веткой включаются
            QMapIterator<QString, QAction *> i(actionList);
            while (i.hasNext()) {
                i.next();
                i.value()->setEnabled( true );
            }

            isAccessItem = true;
        }
    }

    if ( isAccessItem )
    {
        // Получаем указатель на данные таблицы конечных записей
        RecordTableData *rtdata=item->recordtableGetTableData();

        // Устанавливаем данные таблицы конечных записей
        find_object<RecordTableController>("recordTableController")->setTableData(rtdata);

        // Устанавливается текстовый путь в таблице конечных записей для мобильного варианта интерфейса
        if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
        {
            QStringList path=item->getPathAsName();

            // Убирается пустой элемент, если он есть (это может быть корень, у него нет названия)
            int emptyStringIndex=path.indexOf("");
            path.removeAt(emptyStringIndex);

            find_object<RecordTableScreen>("recordTableScreen")->setTreePath( path.join(" > ") );
        }

        // Ширина колонки дерева устанавливается так чтоб всегда вмещались данные
        knowTreeView->resizeColumnToContents(0);

        // Переключаются окна (используется для мобильного интерфейса)
        AppContext::instance().globalParameters()->getWindowSwitcher()->switchFromTreeToRecordtable();
    }

    // this->isKnowtreeClickedWork=false;

    // Установка обнуления lastHandledIndex в конце цикла обработки событий
    QTimer::singleShot(0, this, []{
        lastHandledIndex = QModelIndex();

        // Дополнительно обязательно сбрасывается isKnowtreeClickedWork
        // this->isKnowtreeClickedWork=false;
    });

}

/*
void treescreen::on_knowTreeView_repaint(void)
{
 // Попытка расширить нулевую колонку с деревом, если содержимое слишком широкое
 knowTreeView->resizeColumnToContents(0);
}
*/


void TreeScreen::updateSelectedBranch(void)
{
 // Получение списка выделенных Item-элементов
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

 // Обновление на экране
 for (int i = 0; i < selectitems.size(); ++i) 
  knowTreeView->update(selectitems.at(i));
}


QItemSelectionModel * TreeScreen::getSelectionModel(void)
{
 return knowTreeView->selectionModel();
}


void TreeScreen::setCursorToIndex(QModelIndex index)
{
  // qDebug() << "Set cursor to tree item: " << index.data(Qt::DisplayRole);

  // Если индекс невалидный
  if(!index.isValid())
  {
    qDebug() << "Try set cursor to bad index. Set cursor disabled.";
    return;
  }

  // Если попытка установить курсор на корень (а корень в MyTetra не отображается)
  // Это условие некорректно.
  // В древовидной Qt-модели узлы, находящиеся на верхнем уровне, считаются корневыми.
  // У них нет общего корня. Модель хранит просто список узлов.
  // Но надо разобраться дальше. Ведь по-хорошему должен быть корневой узел, в котором перечислены узлы верхнего уровня.
  /*
  if(!index.parent().isValid())
  {
    qDebug() << "Try set cursor to ROOT index. Disabled.";
    return;
  }
  */

  // Курсор устанавливается на нужный элемент дерева
  // В desktop-варианте на сигнал currentRowChanged() будет вызван слот on_knowtree_clicked()
  knowTreeView->selectionModel()->setCurrentIndex(index,QItemSelectionModel::ClearAndSelect);

  // В мобильной версии реакции на выбор ветки нет (не обрабатывается сигнал смены строки в модели выбора)
  // Поэтому по ветке должен быть сделан виртуальный клик, чтобы заполнилась таблица конечных записей
  // Метод clicked() публичный начиная с Qt5 (мобильный интерфейс возможен только в Qt5)
#if QT_VERSION >= 0x050000 && QT_VERSION < 0x060000
  if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
    emit knowTreeView->clicked(index);
#endif
}


void TreeScreen::setCursorToId(QString nodeId)
{
  TreeItem *item=knowTreeModel->getItemById( nodeId );

  QModelIndex index=knowTreeModel->getIndexByItem( item );

  setCursorToIndex(index);
}

// Получение номера первого выделенного элемента


// Получение номера первого выделенного элемента
int TreeScreen::getFirstSelectedItemIndex(void)
{
 // Получение списка выделенных Item-элементов
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

 if(selectitems.isEmpty())
  return -1; // Если ничего не выделено
 else
  return (selectitems.at(0)).row(); // Индекс первого выделенного элемента
}

// Получение индекса текущего элемента на котором стоит курсор


// Получение индекса текущего элемента на котором стоит курсор
QModelIndex TreeScreen::getCurrentItemIndex(void)
{
 return knowTreeView->selectionModel()->currentIndex();
}

// Метод, следящий, не обнулилось ли дерево
// Если дерево стало пустым, данный метод добавит одну ветку


// Метод, следящий, не обнулилось ли дерево
// Если дерево стало пустым, данный метод добавит одну ветку
void TreeScreen::treeEmptyControl(void)
{
 qDebug() << "treescreen::tree_empty_control() : Tree item count " << knowTreeModel->rowCount();

 if(knowTreeModel->rowCount()==0)
  {
   qDebug() << "treescreen::tree_empty_control() : Tree empty, create blank item";

   insBranchProcess(QModelIndex(), tr("Rename me"), false);
  }
}

// Глобальный режим только чтения: мутирующие действия гаснут,
// копирование ветки, сворачивание и поиск разрешены


// Глобальный режим только чтения: мутирующие действия гаснут,
// копирование ветки, сворачивание и поиск разрешены
void TreeScreen::setReadOnly(bool state)
{
 const QStringList mutatingActions=QStringList()
  << "moveUpBranch" << "moveDownBranch"
  << "insSubbranch" << "insBranch"
  << "editBranch" << "delBranch"
  << "cutBranch" << "pasteBranch" << "pasteSubbranch"
  << "encryptBranch" << "decryptBranch"
  << "setIcon";

 for(const QString &name : mutatingActions)
  if(actionList.contains(name))
   actionList[name]->setEnabled(!state);

 // Перетаскивание веток запрещено, раскрытие и выбор работают
 if(knowTreeView!=nullptr)
  {
   knowTreeView->setDragEnabled(!state);
   knowTreeView->setAcceptDrops(!state);
  }
}


// Метод, следящий, не стало ли дерево содержать только незашифрованные записи
// Если в дереве нет шифрования, задается вопрос, нужно ли сбросить пароль
void TreeScreen::treeCryptControl(void)
{
 // Если в дереве нет шифрования
 if(knowTreeModel->isContainsCryptBranches()==false)
  {
   // Запускается диалог сброса пароля шифрования
   Password pwd(m_dispatcher);
   pwd.resetPassword();
  }
}

// Сохранение дерева веток на диск


// Сохранение дерева веток на диск
void TreeScreen::saveKnowTree(void)
{
  knowTreeModel->save();

  updateLastKnowTreeData( QFileInfo(), false );

  // Сохранено дерево, значит изменились метаданные: заголовки веток,
  // имена, авторы, теги и url заметок, состав веток. Подписчики
  // (панель истории) обновляются по этому сигналу
  emit treeMetadataSaved();
}

// Перечитывание дерева веток с диска
// Метод возвращает true, если обнаружено что данные были изменены и перечитаны


// Перечитывание дерева веток с диска
// Метод возвращает true, если обнаружено что данные были изменены и перечитаны
bool TreeScreen::reloadKnowTree(void)
{
  // Если по каким-то причинам нет данных о файле сохраненного дерева
  if(lastKnowTreeModifyDateTime.isValid()==false || lastKnowTreeSize==0)
  {
    knowTreeModel->reload();
    updateLastKnowTreeData( QFileInfo(), false );
    qDebug() << "Reload XML data if last data not found";
    return true;
  }

  // Если сохраненные ранее данные отличаются от текущих
  QFileInfo fileInfo( knowTreeModel->getXmlFileName() );
  if(fileInfo.lastModified()!=lastKnowTreeModifyDateTime || fileInfo.size()!=lastKnowTreeSize)
  {
    knowTreeModel->reload();
    updateLastKnowTreeData( fileInfo, true );
    qDebug() << "Reload XML data";
    return true;
  }

  qDebug() << "Skip reload XML data";
  return false;
}


void TreeScreen::updateLastKnowTreeData(QFileInfo fileInfo, bool isFileInfoReal)
{
  if(isFileInfoReal==false)
    fileInfo=QFileInfo( knowTreeModel->getXmlFileName() );

  lastKnowTreeModifyDateTime=fileInfo.lastModified();
  lastKnowTreeSize=fileInfo.size();
}

// Установка фокуса на базовый виджет (на список веток дерева)


// Установка фокуса на базовый виджет (на список веток дерева)
void TreeScreen::setFocusToBaseWidget()
{
    knowTreeView->setFocus();
}

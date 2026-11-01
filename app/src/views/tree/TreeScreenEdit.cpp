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
#include "KnowTreeDelegate.h"

#include "models/recordTable/RecordTableData.h"
#include "views/recordTable/RecordTableScreen.h"
#include "models/appConfig/AppConfig.h"
#include "views/mainWindow/MainWindow.h"
#include "views/toolbarSettings/MainToolbarSettingsScreen.h"
#include "models/tree/TreeItem.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/ClipboardBranch.h"
#include "views/record/MetaEditor.h"
#include "views/favoritesPanel/FavoritesPanel.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/crypt/Password.h"
#include "libraries/WindowSwitcher.h"
#include "libraries/helpers/DiskHelper.h"
#include "controllers/recordTable/RecordTableController.h"
#include "libraries/IconSelectDialog.h"
#include "libraries/ShortcutManager.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/ActionHelper.h"
#include "libraries/helpers/MessageHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "libraries/InternalClipboard.h"

#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/appConfig/AppConfigStorage.h"


void TreeScreen::expandAllSubbranch(void)
{
 // Получение индексов выделенных строк
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

 for(int i = 0; i < selectitems.size(); ++i) 
  expandOrCollapseRecurse(selectitems.at(i), true);
}


void TreeScreen::collapseAllSubbranch(void)
{
 // Получение индексов выделенных строк
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

 for(int i = 0; i < selectitems.size(); ++i) 
  expandOrCollapseRecurse(selectitems.at(i), false);
}


void TreeScreen::expandOrCollapseRecurse(QModelIndex modelIndex, bool mode)
{
    knowTreeView->setExpanded(modelIndex, mode);

    // Перебор дочерних (child) элементов
    int i=0;
    while( (modelIndex.model()->index(i,0,modelIndex)).isValid() )
    {
        expandOrCollapseRecurse(modelIndex.model()->index(i,0,modelIndex), mode);
        i++;
    }

}


void TreeScreen::moveUpBranch(void)
{
 moveUpDownBranch(1);
}


void TreeScreen::moveDownBranch(void)
{
 moveUpDownBranch(-1);
}


void TreeScreen::moveUpDownBranch(int direction)
{
 // Если ветку нельзя перемещать
 if(!moveCheckEnable()) return;
 
 // Получение индекса выделенной строки
 QModelIndex index=getCurrentItemIndex();
 
 // Ветка перемещается
 QModelIndex index_after_move;
 if(direction==1) index_after_move=knowTreeModel->moveUpBranch(index);
 else             index_after_move=knowTreeModel->moveDownBranch(index);
  
 // Установка курсора на позицию, куда была перенесена ветка
 if(index_after_move.isValid())
  {
   knowTreeView->selectionModel()->setCurrentIndex(index_after_move,QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
   knowTreeView->selectionModel()->select(index_after_move,QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
  } 
 
 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

  // Порядок веток поменялся: пути в кеше статистики в пересчет
  knowTreeModel->invalidateStats();
}


bool TreeScreen::moveCheckEnable(void)
{
 // Получение списка индексов QModelIndex выделенных элементов
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();
 
 // Если выбрано более одной ветки
 if(selectitems.size()>1)
  {
   QMessageBox messageBox(this);
   messageBox.setWindowTitle(tr("Unavailable action"));
   messageBox.setText(tr("You've selected ")+QString::number(selectitems.size())+tr(" items.\nPlease select single item for moving."));
   messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
   messageBox.exec();
   return false; 
  }
 else
  return true;
}


void TreeScreen::insBranch(void)
{
 qDebug() << "In ins_branch()";

 insBranchSmart(true);
}


void TreeScreen::insSubbranch(void)
{
 qDebug() << "In ins_subbranch()";

 insBranchSmart(false);
}

// Вспомогательная функция, используется при добавлении ветки
// Если is_branch=true, то добавляется ветка на тот же уровень
// Если is_branch=false, то добавляется подветка


// Вспомогательная функция, используется при добавлении ветки
// Если is_branch=true, то добавляется ветка на тот же уровень
// Если is_branch=false, то добавляется подветка
void TreeScreen::insBranchSmart(bool is_branch)
{
 // Получение списка индексов QModelIndex выделенных элементов
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

 // Если выбрано более одной ветки
 if(selectitems.size()>1)
  {
   QMessageBox messageBox(this);
   messageBox.setWindowTitle(tr("Unavailable action"));
   messageBox.setText(tr("You've selected ")+QString::number(selectitems.size())+tr(" items.\nPlease select single item for enabling insert operation."));
   messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
   messageBox.exec();
   return;
  }

 // Создается окно ввода данных
 bool ok;
 QString title, text;
 if(is_branch) {
  title=tr("Create new item");
  text=tr("Item name:");
 }
 else {
  title=tr("Create new sub item");
  text=tr("Sub item name:");
 }
 QString name = QInputDialog::getText(this, title, text, QLineEdit::Normal, "", &ok);
 if (!( ok && !name.isEmpty() )) return; // Если была нажата отмена

  // Получение индекса выделенной строки
 QModelIndex index=getCurrentItemIndex();

 // Введенные данные добавляются
 insBranchProcess(index, name, is_branch);
}


void TreeScreen::insBranchProcess(QModelIndex index, QString name, bool is_branch)
{
 // Получение ссылки на узел, который соответствует выделенной строке
 TreeItem *item=knowTreeModel->getItem(index);

 find_object<MainWindow>("mainwindow")->setDisabled(true);

 // Получение уникального идентификатора
 QString id=getUniqueId();

 // Инфополя создаваемой ветки
 QMap<QString, QString> branchFields;
 branchFields["id"]=id;
 branchFields["name"]=name;

 // Вставка данных и установка курсора

 // Одноранговая ветка
 if(is_branch)
  {
   // Вставка новых данных в модель дерева записей
   knowTreeModel->addNewSiblingBranch(index, branchFields);

   // Установка курсора на только что созданную позицию

   // Чтобы вычислить позицию, надо синхронно получить parent элемента,
   // на уровне которого должен был создасться новый элемент.
   // Parent надо получить и в виде объекта QModelIndex, и в виде объекта Item
   // Затем у объекта Item выяснить количество элементов, и установить
   // засветку через метод index() относительно parent в виде QModelIndex
   QModelIndex setto=knowTreeModel->index(item->parent()->childCount()-1,0,index.parent());
   knowTreeView->selectionModel()->setCurrentIndex(setto, QItemSelectionModel::ClearAndSelect);
  }
 else
  {
   // Подветка

   // Вставка новых данных в модель дерева записей
   knowTreeModel->addNewChildBranch(index, branchFields);

   // Установка курсора на только что созданную позицию
   QModelIndex setto=knowTreeModel->indexChildren(index,item->childCount()-1);
   knowTreeView->selectionModel()->setCurrentIndex(setto, QItemSelectionModel::ClearAndSelect);

   // А можно было установить курсор на нужную позицию и так
   // knowTreeView->selectionModel()->setCurrentIndex(kntrmodel->index(item->childCount()-1,0,index),
   //                                             QItemSelectionModel::ClearAndSelect);
  }

 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

 find_object<MainWindow>("mainwindow")->setEnabled(true);
}


void TreeScreen::editBranch(void)
{
 qDebug() << "In edit_branch()";

  // Получение списка индексов QModelIndex выделенных элементов
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();
 
 // Если выбрано более одной ветки
 if(selectitems.size()>1)
  {
   QMessageBox messageBox(this);
   messageBox.setWindowTitle(tr("Unavailable action"));
   messageBox.setText(tr("You've selected ")+QString::number(selectitems.size())+tr(" items.\nPlease select single item for enabling edit operation."));
   messageBox.addButton(tr("OK"), QMessageBox::AcceptRole);
   messageBox.exec();
   return; 
  }
 
 // Получение индекса выделенной строки
 QModelIndex index=getCurrentItemIndex();
 
 // Получение ссылки на узел, который соответствует выделенной строке
 TreeItem *item=knowTreeModel->getItem(index);

 // Если ветка зашифрована и пароль не был введен
 if(item->getField("crypt")=="1" &&
    AppContext::instance().globalParameters()->getCryptKey().length()==0)
  return;
 
 // Получение имени ветки
 QString name=item->getField("name");
  
 // Создается окно ввода данных
 bool ok;
 QString newname = QInputDialog::getText(this, 
                                      tr("Edit item name"),
                                      tr("Item name:"), 
                                      QLineEdit::Normal,
                                      name, 
                                      &ok);

 // Если была нажата отмена
 if (!( ok && !newname.isEmpty() )) return; 

 find_object<MainWindow>("mainwindow")->setDisabled(true);
 
 item->setField("name", newname);
 
 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

  // Имя ветки: пути записей в кеше статистики в пересчет
  knowTreeModel->invalidateStats();
 
 find_object<MainWindow>("mainwindow")->setEnabled(true);
}

// Удаление выбранных веток, вызывается при выборе соотвествущей
// кнопки или пункта меню


// Удаление выбранных веток, вызывается при выборе соотвествущей
// кнопки или пункта меню
void TreeScreen::delBranch(QString mode)
{
 qDebug() << "In del_branch()";

 // На время удаления блокируется главное окно
 find_object<MainWindow>("mainwindow")->setDisabled(true);
 find_object<MainWindow>("mainwindow")->blockSignals(true);

 // Получение списка индексов QModelIndex выделенных элементов
 QModelIndexList selectItems=knowTreeView->selectionModel()->selectedIndexes();

 // Список имен веток, которые нужно удалить
 QStringList branchesName;
 for(int i = 0; i < selectItems.size(); ++i)
  {
   QModelIndex index=selectItems.at(i);
   TreeItem *item=knowTreeModel->getItem(index);
   branchesName << item->getField("name");
  }

 
 // Если системный пароль не установлен, зашифрованные ветки удалять нельзя
 if(AppContext::instance().globalParameters()->getCryptKey().size()==0)
  {
   bool disableFlag=false;

   // Перебираются удаляемые ветки
   for(int i = 0; i < selectItems.size(); ++i)
    {
     QModelIndex index=selectItems.at(i);
     TreeItem *item=knowTreeModel->getItem(index);
     
     // Если у ветки установлен флаг шифрования
     if(item->getField("crypt")=="1")
      {
       disableFlag=true;
       break;
      }

     // Проверяется наличие флага шифрования у всех подветок
     QList<QStringList> cryptFlagsList=item->getAllChildrenPathAsField("crypt");
     foreach(QStringList cryptFlags, cryptFlagsList)
      if(cryptFlags.contains("1"))
       {
        disableFlag=true;
        break;
       }

     if(disableFlag)
      break;

    } // Закрылся цикл перебора всех выделенных для удаления веток

   // Если в какой-то ветке обнаружено шифрование
   if(disableFlag)
    {
     QMessageBox messageBox(this);
     messageBox.setWindowTitle(tr("Unavailable action"));
     messageBox.setText(tr("In your selected data found closed item. Action canceled."));
     messageBox.addButton(tr("OK"), QMessageBox::AcceptRole);
     messageBox.exec();

     // Разблокируется главное окно
     find_object<MainWindow>("mainwindow")->setEnabled(true);
     find_object<MainWindow>("mainwindow")->blockSignals(false);

     return;
    }

  } // Закрылось условие что системный пароль не установлен
 

 // Перебираются ветки, которые нужно удалить, и в них проверяется наличие заблокированных записей
 bool isSelectionContainBlockRecords=false;
 for(int i = 0; i < selectItems.size(); ++i)
 {
   QModelIndex index=selectItems.at(i);
   TreeItem *item=knowTreeModel->getItem(index);

   if( knowTreeModel->isItemContainsBlockRecords(item) )
   {
     isSelectionContainBlockRecords=true;
     break;
   }
 }

 // Если есть записи, помеченные как заблокированные
 if(isSelectionContainBlockRecords)
 {
   QMessageBox messageBox(this);
   messageBox.setWindowTitle(tr("Confirmation request")); // Запрос подтверждения
   messageBox.setText(tr("In the selected item has been found blocked notes. Do you really want to delete one?"));
   messageBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
   messageBox.setDefaultButton(QMessageBox::Cancel);

   int ret = messageBox.exec();

   if(ret==QMessageBox::Cancel)
   {
     // Разблокируется главное окно
     find_object<MainWindow>("mainwindow")->setEnabled(true);
     find_object<MainWindow>("mainwindow")->blockSignals(false);
     return;
   }
 }

 // Создается окно с вопросом, нужно удалять ветки или нет
 QString title, text, del_button;
 bool enable_question=true;
 if(mode=="delete")
  {
   title=tr("Delete item(s)");
   text=tr("Are you sure you wish to delete item(s) <b>") + branchesName.join(", ") + tr("</b> and all sub items?");
   del_button=tr("Delete");

   enable_question=true;
  }
 else if(mode=="cut")
  {
   title=tr("Cut item");
   text=tr("Are you sure you wish to cut item <b>") + branchesName.join(", ") + tr("</b> and all sub items?");
   del_button=tr("Cut");

   if(AppContext::instance().appConfig()->uiSection()->get_cutbranchconfirm()) enable_question=true;
   else enable_question=false;
  }

 bool enable_del=true;
 if(enable_question)
  {
   QMessageBox messageBox(this);
   messageBox.setWindowTitle(title);
   messageBox.setText(text);
   messageBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
   QAbstractButton *deleteButton =messageBox.addButton(del_button, QMessageBox::AcceptRole);
   messageBox.exec();

   if(messageBox.clickedButton() == deleteButton) enable_del=true;
   else enable_del=false;
  }

 // Если удаление подтверждено
 if(enable_del)
  {
   // Сохраняется текст в окне редактирования
   // Нужно, чтобы нормально удалилась текущая редактируемая запись,
   // если она находится в удаляемой ветке
   find_object<MainWindow>("mainwindow")->saveTextarea();

   knowTreeModel->deleteItemsByModelIndexList( selectItems );

   qDebug() << "Delete finish";

   // Сохранение дерева веток
   find_object<TreeScreen>("treeScreen")->saveKnowTree();

  // Ветка с записями ушла: кеш статистики в пересчет
  knowTreeModel->invalidateStats();

   qDebug() << "Save new tree finish";
  }

 // Разблокируется главное окно
 find_object<MainWindow>("mainwindow")->setEnabled(true);
 find_object<MainWindow>("mainwindow")->blockSignals(false);

 // Если была вырезана ветка, а ее больше нет - состояние вырезания сбрасывается
 if(!knowTreeModel->cutBranchId().isEmpty() &&
    knowTreeModel->getItemById(knowTreeModel->cutBranchId())==nullptr)
  knowTreeModel->clearCutBranchId();

 treeEmptyControl();
 treeCryptControl();
}

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


void TreeScreen::cutBranch(void)
{
 bool copy_result;
 
 copy_result=copyBranch();

 // Ветка сразу не удаляется: она помечается как вырезанная (становится
 // серой), а перемещение произойдет в момент вставки. Если вставка
 // не состоится (выход из программы, Esc), данные останутся на месте,
 // так как удаление происходит только как часть перемещения
 if(copy_result)
  {
   TreeItem *item=knowTreeModel->getItem(getCurrentItemIndex());

   if(item!=nullptr)
    knowTreeModel->setCutBranchId(item->getField("id"));
  }
}

// Отмена вырезания: серая ветка становится обычной, данные не трогаются


// Отмена вырезания: серая ветка становится обычной, данные не трогаются
void TreeScreen::cancelCutBranch(void)
{
 knowTreeModel->clearCutBranchId();
}


bool TreeScreen::copyBranch(void)
{
    qDebug() << "In copy_branch()";

    // Новое копирование отменяет pending вырезание: серая ветка остается
    // на месте и становится обычной
    knowTreeModel->clearCutBranchId();

    // Сохраняется текст в окне редактирования
    find_object<MainWindow>("mainwindow")->saveTextarea();

    // Получение списка индексов QModelIndex выделенных элементов
    QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

    // Если выбрано более одной ветки
    if(selectitems.size()>1)
    {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Unavailable action"));
        messageBox.setText(tr("Please select a single item for copy."));
        messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
        messageBox.exec();
        return false;
    }

    // Получение индекса выделенной ветки
    QModelIndex index=getCurrentItemIndex();

    // Получение ссылки на узел, который соответствует выделенной ветке
    TreeItem *item=knowTreeModel->getItem(index);

    // Получение пути к выделенной ветке
    QStringList path=item->getPath();

    // Получение путей ко всем подветкам
    QList<QStringList> subbranchespath=item->getAllChildrenPath();

    // Проверка, содержит ли данная ветка как шифрованные
    // так и незашифрованные данные
    bool nocryptPresence=false;
    bool encryptPresence=false;

    // Флаги на основе состояния текущей ветки
    if(knowTreeModel->getItem(path)->getField("crypt")=="1")
        encryptPresence=true;
    else
        nocryptPresence=true;

    // Флаги на основе состояния подветок
    foreach(QStringList currPath, subbranchespath)
        if(knowTreeModel->getItem(currPath)->getField("crypt")=="1")
            encryptPresence=true;
        else
            nocryptPresence=true;

    // Если ветка содержит как шифрованные так и нешифрованные данные
    // то такую ветку копировать в буфер нельзя
    if(nocryptPresence==true && encryptPresence==true)
    {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Unavailable action"));
        messageBox.setText(tr("This item contains both unencrypted and encrypted data. Copy/paste operation is possible only for item that contain similar type data."));
        messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
        messageBox.exec();
        return false;
    }

    // --------------------------------------
    // Копирование во внутренний буфер обмена
    // --------------------------------------

    qDebug() << "Tree item copy to buffer";

    // Данные в буфере обмена очищаются
    AppContext::instance().internalClipboard()->clear();

    // Создается объект с данными для заполнения буфера обмена
    ClipboardBranch *branch_clipboard_data=new ClipboardBranch();

    // Добавление корневой ветки
    addBranchToClipboard(branch_clipboard_data, path, true);

    // Добавление прочих веток
    foreach(QStringList curr_path, subbranchespath)
        addBranchToClipboard(branch_clipboard_data, curr_path, false);

    // branch_clipboard_data->print();

    // Объект с ветками помещается в буфер обмена, владение указателем передается
    // глобальному объекту буфера обмена, поэтому утечки нет
    AppContext::instance().internalClipboard()->setMimeData(branch_clipboard_data);

    return true;
}

// Вспомогательная функция при копировании ветки в буфер


// Вспомогательная функция при копировании ветки в буфер
void TreeScreen::addBranchToClipboard(ClipboardBranch *branch_clipboard_data, QStringList path, bool is_root)
{
  TreeItem *curr_item;
  QMap<QString, QString> curr_item_fields;
  QString branch_id;
  RecordTableData *curr_item_record_table;

  // Добавление ветки
  curr_item=knowTreeModel->getItem(path);
  curr_item_fields=curr_item->getAllFields(); // Раньше было getAllFieldsDirect()
  branch_id=curr_item_fields["id"];
  if(is_root)
    branch_clipboard_data->addBranch("-1",
                                     curr_item_fields);
  else
    branch_clipboard_data->addBranch(curr_item->getParentId(),
                                     curr_item_fields);

  // Добавление конечных записей
  curr_item_record_table=curr_item->recordtableGetTableData();
  for(unsigned int i=0; i<curr_item_record_table->size(); i++)
  {
    // Полный образ записи (с файлами и текстом)
    Record record=curr_item_record_table->getRecordFat(i);

    branch_clipboard_data->addRecord(branch_id, record);
  }
}

// Вставка ветки из буфера на том же уровне, что и выбранная


// Вставка ветки из буфера на том же уровне, что и выбранная
void TreeScreen::pasteBranch(void)
{
  qDebug() << "In paste_branch";

  pasteBranchSmart(true);
}

// Вставка ветки из буфера в виде подветки выбранной ветки


// Вставка ветки из буфера в виде подветки выбранной ветки
void TreeScreen::pasteSubbranch(void)
{
 qDebug() << "In paste_subbranch";

 pasteBranchSmart(false);
}


void TreeScreen::pasteBranchSmart(bool is_branch)
{
 // Если есть вырезанная ветка - это перемещение, а не копирование.
 // Данные из буфера обмена при этом не используются
 if(!knowTreeModel->cutBranchId().isEmpty())
  {
   pasteCutBranch(is_branch);
   return;
  }

 // Проверяется, содержит ли внутренний буфер обмена данные нужного формата
 const QMimeData *mimeData = AppContext::instance().internalClipboard()->mimeData();

 if(mimeData==nullptr)
  return;

 if( ! (mimeData->hasFormat(FixedParameters::appTextId+"/branch")) )
  return;

 // Получение списка индексов QModelIndex выделенных элементов
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

 // Если выбрано более одной ветки или вообще ветка не выбрана
 if(selectitems.size()!=1)
  {
   QMessageBox messageBox(this);
   messageBox.setWindowTitle(tr("Unavailable action"));
   messageBox.setText(tr("You've selected ")+QString::number(selectitems.size())+tr(" items.\nPlease select single item for enabling paste operation."));
   messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
   messageBox.exec();
   return;
  }

 // Блокируется главное окно, чтобы при продолжительном выполнении
 // не было возможности сделать другие действия
 find_object<MainWindow>("mainwindow")->setDisabled(true);

 // Извлечение объекта из внутреннего буфера обмена
 const ClipboardBranch *branch;
 branch=qobject_cast<const ClipboardBranch *>(AppContext::instance().internalClipboard()->mimeData());
 branch->print();
 branch->printIdTree();

 // Получение индекса выделенной строки дерева
 QModelIndex index=this->getCurrentItemIndex();

 // Добавление ветки
 QString pasted_branch_id;
 if(is_branch)
  pasted_branch_id=knowTreeModel->pasteNewSiblingBranch(index, (ClipboardBranch *)branch);
 else
  pasted_branch_id=knowTreeModel->pasteNewChildBranch(index, (ClipboardBranch *)branch);

 // Установка курсора на новую созданную ветку.
 // Ветки может не быть, если вставка не удалась, тогда дальше делать нечего
 TreeItem *pasted_branch_item=knowTreeModel->getItemById(pasted_branch_id);
 if(pasted_branch_item==nullptr)
 {
  find_object<MainWindow>("mainwindow")->setEnabled(true);
  return;
 }

 QStringList pasted_branch_path=pasted_branch_item->getPath();
 find_object<MainWindow>("mainwindow")->setTreePosition(pasted_branch_path);

 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

  // Ветка с записями приехала: кеш статистики в пересчет
  knowTreeModel->invalidateStats();

 // Разблокируется главное окно
 find_object<MainWindow>("mainwindow")->setEnabled(true);
}

// Вставка вырезанной ветки перемещением с сохранением идентификатора.
// В отличие от вставки копии здесь не генерируются новые ID и не копируются
// файлы записей: перемещается сам элемент дерева


// Вставка вырезанной ветки перемещением с сохранением идентификатора.
// В отличие от вставки копии здесь не генерируются новые ID и не копируются
// файлы записей: перемещается сам элемент дерева
void TreeScreen::pasteCutBranch(bool is_branch)
{
 qDebug() << "In paste_cut_branch";

 // Получение списка индексов QModelIndex выделенных элементов
 QModelIndexList selectitems=knowTreeView->selectionModel()->selectedIndexes();

 // Если выбрано более одной ветки или вообще ветка не выбрана
 if(selectitems.size()!=1)
  {
   QMessageBox messageBox(this);
   messageBox.setWindowTitle(tr("Unavailable action"));
   messageBox.setText(tr("You've selected ")+QString::number(selectitems.size())+tr(" items.\nPlease select single item for enabling paste operation."));
   messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
   messageBox.exec();
   return;
  }

 // Блокируется главное окно, чтобы при продолжительном выполнении
 // не было возможности сделать другие действия
 find_object<MainWindow>("mainwindow")->setDisabled(true);

 QString cutId=knowTreeModel->cutBranchId();
 TreeItem *target=knowTreeModel->getItem(getCurrentItemIndex());

 bool moveok=false;
 if(target!=nullptr)
  moveok=knowTreeModel->moveBranch(cutId, target->getField("id"), !is_branch);

 // Разблокируется главное окно
 find_object<MainWindow>("mainwindow")->setEnabled(true);

 if(!moveok)
  {
   // Вырезанная ветка могла быть удалена после вырезания. Тогда состояние
   // сбрасывается. Если ветка на месте, а цель внутри нее - состояние
   // сохраняется чтобы можно было выбрать другую цель
   if(knowTreeModel->getItemById(cutId)==nullptr)
    {
     knowTreeModel->clearCutBranchId();

     QMessageBox messageBox(this);
     messageBox.setWindowTitle(tr("Unavailable action"));
     messageBox.setText(tr("Cut item no longer exists."));
     messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
     messageBox.exec();
    }
   else
    {
     QMessageBox messageBox(this);
     messageBox.setWindowTitle(tr("Unavailable action"));
     messageBox.setText(tr("Cannot move item inside itself."));
     messageBox.addButton(tr("OK"),QMessageBox::AcceptRole);
     messageBox.exec();
    }

   return;
  }

 // Установка курсора на перемещенную ветку
 TreeItem *moved_branch_item=knowTreeModel->getItemById(cutId);
 QStringList moved_branch_path=moved_branch_item->getPath();
 find_object<MainWindow>("mainwindow")->setTreePosition(moved_branch_path);

 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

  // Ветка перемещена вырезанием: состав и пути поменялись
  knowTreeModel->invalidateStats();

 // Вырезание завершено, серая подсветка снимается
 knowTreeModel->clearCutBranchId();
}

// Шифрование ветки


// Шифрование ветки
void TreeScreen::encryptBranch(void)
{
 // Если пароль в данной сессии не вводился
 if(AppContext::instance().globalParameters()->getCryptKey().size()==0)
  {
   // Запрашивается пароль
   Password password(m_dispatcher);
   if(password.retrievePassword()==true) // Если пароль введен правильно
    encryptBranchItem(); // Ветка шифруется
  }
 else
  {
   // Иначе считается, что шифрующий ключ уже был задан и он правильный
 
   encryptBranchItem(); // Ветка шифруется
  }
}

// Расшифровка ветки


// Расшифровка ветки
void TreeScreen::decryptBranch(void)
{
 // Если пароль в данной сессии не вводился
 if(AppContext::instance().globalParameters()->getCryptKey().size()==0)
  {
   // Запрашивается пароль
   Password password(m_dispatcher);
   if(password.retrievePassword()==true) // Если пароль введен правильно
    decryptBranchItem(); // Ветка расшифровывается
  }
 else
  {
   // Иначе пароль в данной сессии вводился и он правильный

   decryptBranchItem(); // Ветка расшифровывается
  }
}


void TreeScreen::encryptBranchItem(void)
{
 // Получаем указатель на текущую выбранную ветку дерева
 TreeItem *item = knowTreeModel->getItem( getCurrentItemIndex() );

 // Шифрация ветки и всех подветок
 item->switchToEncrypt();

 // Объект редактора оповещается, что теперь идет работа с зашифрованными данными
 // Это необходимо делать для предотвращения бага,
 // когда запись начинала редактироваться незашифрованной, потом произошло шифрование, и редактирование записи было продолжено
 find_object<MetaEditor>("editorScreen")->setMiscField("crypt", "1");

 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

  // Текст шифрован: читаемость в кеше статистики в пересчет
  knowTreeModel->invalidateStats();

 // Обновляеются на экране ветка и ее подветки
 updateBranchOnScreen( getCurrentItemIndex() );
}


void TreeScreen::decryptBranchItem(void)
{
 // Получаем указатель на текущую выбранную ветку дерева
 TreeItem *item = knowTreeModel->getItem( getCurrentItemIndex() );

 // Расшифровка ветки и всех подветок
 item->switchToDecrypt();

 // Объект редактора оповещается, что теперь идет работа с расшифрованными данными
 find_object<MetaEditor>("editorScreen")->setMiscField("crypt", "0");

 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

  // Текст расшифрован: читаемость в кеше статистики в пересчет
  knowTreeModel->invalidateStats();

 // Обновляеются на экране ветка и ее подветки
 updateBranchOnScreen( getCurrentItemIndex() );

 // Проверяется, остались ли в дереве зашифрованные данные
 // если зашифрованных данных нет, будет предложено сбросить пароль
 treeCryptControl();
}

// Установка иконки для ветки


// Установка иконки для ветки
void TreeScreen::setIcon(void)
{
  QString startDirectory=AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/"+FixedParameters::iconsRelatedDirectory;
  qDebug() << "Set start directory for select icon: " << startDirectory;

  // Создается окно выбора файла иконки
  IconSelectDialog iconSelectDialog;
  iconSelectDialog.setDefaultSection( AppContext::instance().appConfig()->uiSection()->getIconCurrentSectionName() );
  iconSelectDialog.setPath( startDirectory );

  int result=iconSelectDialog.exec();

  if( result==QDialog::Accepted && iconSelectDialog.getSelectFileName().isEmpty() )
  {
    showMessageBox(tr("No icon selected.")); // Сообщение "Вы не выбрали иконку"
  }
  else if( result==QDialog::Accepted && !iconSelectDialog.getSelectFileName().isEmpty() ) // Если был выбран файл иконки и нажат Ok
  {
    QString fullIconFileName=iconSelectDialog.getSelectFileName();
    QFileInfo iconFileInfo(fullIconFileName);
    QString iconFileName=iconFileInfo.fileName();
    QString iconDir=iconFileInfo.dir().dirName();
    QString relatedFileName="/"+iconDir+"/"+iconFileName;

    TreeItem *currentItem=knowTreeModel->getItem( getCurrentItemIndex() );

    currentItem->setField("icon", relatedFileName);

    // Обновляеются на экране ветка и ее подветки
    updateBranchOnScreen( getCurrentItemIndex() );

    // Записывается дерево
    saveKnowTree();
  }
  else if( result==IconSelectDialog::RemoveIconCode ) // Если было выбрано действие убирание иконки, назначенной для ветки
  {
    TreeItem *currentItem=knowTreeModel->getItem( getCurrentItemIndex() );
    currentItem->setField("icon", "");

    // Обновляеются на экране ветка и ее подветки
    updateBranchOnScreen( getCurrentItemIndex() );

    // Записывается дерево
    saveKnowTree();
  }

  // Если текущая секция изменилась, ее имя запоминается чтобы в последующем открывать виджет с этой секцией
  if(iconSelectDialog.getCurrentSection()!="" &&
     iconSelectDialog.getCurrentSection()!=AppContext::instance().appConfig()->uiSection()->getIconCurrentSectionName())
   AppContext::instance().appConfig()->uiSection()->setIconCurrentSectionName( iconSelectDialog.getCurrentSection() );
}


void TreeScreen::exportBranchToDirectory(QString exportDir)
{
  // Проверка, является ли выбранная директория пустой. Выгрузка возможна только в полностью пустую директорию
  if( !DiskHelper::isDirectoryEmpty(exportDir) )
  {
    showMessageBox(tr("The export directory %1 is not empty. Please, select an empty directory.").arg(exportDir));
    return;
  }

  // Текущая выбранная ветка будет экспортироваться
  if( !getCurrentItemIndex().isValid() )
  {
    showMessageBox(tr("No export tree item selected. Please select a item."));
    return;
  }

  TreeItem *startItem=knowTreeModel->getItem( getCurrentItemIndex() );

  // ---------------------------------------------
  // Запрос пароля, если есть зашифрованные данные
  // ---------------------------------------------

  // Выясняется, есть ли в выбранной ветке или подветках есть шифрование
  bool isCryptPresent=false;
  if( knowTreeModel->isItemContainsCryptBranches(startItem) )
    isCryptPresent=true;

  // Если есть шифрование в выгружаемых данных, надо запросить пароль даже если он уже был введен в текущей сессии
  // Это необходимо для того, чтобы не было возможности выгрузить скопом все зашифрованные данные, если
  // пользователь отошел от компьютера
  if( isCryptPresent )
  {
    showMessageBox(tr("Exported tree item contains encrypted data.\nPlease click OK and enter the password.\nAll data will be exported unencrypted."));

    // Запрашивается пароль
    Password password(m_dispatcher);
    if(password.enterExistsPassword()==false) // Если пароль введен неверно, выгрузка работать не должна
      return;
  }

  // Экспорт данных
  bool result=knowTreeModel->exportBranchToDirectory(startItem, exportDir);

  if(result)
    showMessageBox(tr("Done exporting into <b>%1</b>.").arg(exportDir));
  else
    showMessageBox(tr("Errors occurred while exporting."));
}


void TreeScreen::importBranchFromDirectory(QString importDir)
{
  // Импорт будет идти в текущую выбранную ветку
  if( !getCurrentItemIndex().isValid() )
  {
    showMessageBox(tr("No tree item selected for importing. Please select a item."));
    return;
  }

  TreeItem *startItem=knowTreeModel->getItem( getCurrentItemIndex() );

  // -----------------------------------------------
  // Запрос пароля при импорте в зашифрованную ветку
  // -----------------------------------------------

  // Если импорт происходит в зашифрованную ветку, но пароль не был введен
  if(startItem->getField("crypt")=="1" && AppContext::instance().globalParameters()->getCryptKey().length()==0)
  {
    showMessageBox(tr("You are importing into an encrypted item.\nPlease click Ok and enter the password.\nAll data imported will be encrypted."));

    // Запрашивается пароль
    Password password(m_dispatcher);
    if(password.enterExistsPassword()==false) // Если пароль введен неверно, импорт работать не должен
      return;
  }

  // Импорт данных
  QString importNodeId=knowTreeModel->importBranchFromDirectory(startItem, importDir);

  // Если импорт данных был успешным
  if(importNodeId.count()>0)
  {
    setCursorToId(importNodeId); // Курсор устанавливается на только что импортированную ветку

    // Приехала целая ветка: кеш статистики в пересчет
    knowTreeModel->invalidateStats();
  }

  showMessageBox(tr("Item importing finished."));
}

// Обновление на экране ветки и подветок

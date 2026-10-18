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
#include <QInputDialog>

#include "TreeScreen.h"
#include "KnowTreeView.h"

#include "models/recordTable/RecordTableData.h"
#include "views/recordTable/RecordTableScreen.h"
#include "models/appConfig/AppConfig.h"
#include "views/mainWindow/MainWindow.h"
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


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern ShortcutManager shortcutManager;
extern InternalClipboard *internalClipboard;


TreeScreen::TreeScreen(QWidget *parent) : QWidget(parent)
{
  lastKnowTreeModifyDateTime=QDateTime();
  lastKnowTreeSize=0;

  setupActions();
  setupShortcuts();
  setupUI();
  setupModels();
  setupSignals();
  assembly();

  // Обязательно надо проинициализировать начальные значения, чтобы
  // не запускалось обновление дерева (и текущей записи) при первом обновлении.
  // Если при редактировании записи было вызвано окошко, например выбора нового цвета таблицы,
  // затем синхронизация завершилась, и если программа решит, что имеются на диске
  // какие-то изменения (которых на самом деле нет, просто небыло начальной инициализации),
  // то запустит обновление дерева (и текущей записи), в результате чего временные объекты,
  // над которыми производились действия, например по изменению цвета, будут инвалидированы,
  // и их использование после закрытия окна выбора цвета будет приводить
  // к некорректному завершению программы
  updateLastKnowTreeData( QFileInfo(), false );
}


TreeScreen::~TreeScreen()
{

}


void TreeScreen::setupActions(void)
{
 QAction *ac;

 // Разворачивание всех подветок
 ac=new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/expand_all_subbranch.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::expandAllSubbranch);
 actionList["expandAllSubbranch"]=ac;

 // Сворачивание всех подветок
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/collapse_all_subbranch.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::collapseAllSubbranch);
 actionList["collapseAllSubbranch"]=ac;

 // Перемещение ветки вверх
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/move_up.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::moveUpBranch);
 actionList["moveUpBranch"]=ac;

 // Перемещение ветки вниз
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/move_dn.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::moveDownBranch);
 actionList["moveDownBranch"]=ac;

 // Вставка новой подветки
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/add_subbranch.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::insSubbranch);
 actionList["insSubbranch"]=ac;

 // Вставка новой ветки
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/add_branch.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::insBranch);
 actionList["insBranch"]=ac;

 // Редактирование ветки
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/note_edit.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::editBranch);
 actionList["editBranch"]=ac;

 // Удаление ветки
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/note_delete.svg"));
 connect(ac, SIGNAL(triggered()), this, SLOT(delBranch())); // Разобраться с новым синтаксисом сигнал-слот для слота с параметром по-умолчанию
 actionList["delBranch"]=ac;

 // Удаление ветки с сохранением копии в буфер обмена
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/branch_cut.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::cutBranch);
 actionList["cutBranch"]=ac;

 // Копирование ветки в буфер обмена
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/branch_copy.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::copyBranch);
 actionList["copyBranch"]=ac;

 // Вставка ветки из буфера обмена
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/branch_paste.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::pasteBranch);
 actionList["pasteBranch"]=ac;

 // Вставка ветки из буфера обмена в виде подветки
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/branch_paste.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::pasteSubbranch);
 actionList["pasteSubbranch"]=ac;

 // Отмена вырезания по Esc: серая ветка становится обычной, данные не трогаются.
 // Шорткат задан напрямую, а не через таблицу шорткатов: отмена по Esc это
 // общепринятое поведение, не требующее настройки
 ac = new QAction(this);
 ac->setShortcut(QKeySequence(Qt::Key_Escape));
 ac->setShortcutContext(Qt::WidgetWithChildrenShortcut);
 connect(ac, &QAction::triggered, this, &TreeScreen::cancelCutBranch);
 actionList["cancelCutBranch"]=ac;

 // Шифрование ветки
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/branch_encrypt.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::encryptBranch);
 actionList["encryptBranch"]=ac;

 // Расшифровка ветки, т. е. снятие пароля
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/branch_decrypt.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::decryptBranch);
 actionList["decryptBranch"]=ac;

 // Добавление иконки к ветке
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/set_icon.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::setIcon);
 actionList["setIcon"]=ac;

 // Открытие поиска по базе (связывание клика происходит в MainWindows)
 ac = new QAction(tr("Find in base"), this);
 ac->setStatusTip(tr("Find in base"));
 ac->setIcon(QIcon(":/resource/pic/find_in_base.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::treeScreenFindInBaseClicked);
 actionList["findInBase"]=ac;

 // Синхронизация базы. Переехала сюда с тулбара записей: операция
 // уровня всей базы, а не отдельной записи. Клик дёргает MainWindow
 ac = new QAction(this);
 ac->setIcon(QIcon(":/resource/pic/synchronization.svg"));
 connect(ac, &QAction::triggered, this, &TreeScreen::onSyncroClick);
 actionList["syncro"]=ac;

 // Панель избранного между кнопками веток и деревом. Создается здесь,
 // а не в MainWindow, чтобы лежать ровно под тулбаром. Скрыта пока
 // пусто или фича выключена: дизайн не ломается
 favoritesPanel=new FavoritesPanel(this);
 favoritesPanel->setObjectName("favoritesPanel");
 favoritesPanel->hide();
}


void TreeScreen::setupShortcuts(void)
{
    qDebug() << "Setup shortcut for" << this->metaObject()->className();

    // Прямой текст кнопки поиска: шортката у нее нет,
    // поэтому освежается здесь же вместе со всеми
    actionList["findInBase"]->setText(tr("Find in base"));
    actionList["findInBase"]->setStatusTip(tr("Find in base"));

    shortcutManager.initAction("tree-expandAllSubbranch", actionList["expandAllSubbranch"] );
    shortcutManager.initAction("tree-collapseAllSubbranch", actionList["collapseAllSubbranch"] );
    shortcutManager.initAction("tree-moveUpBranch", actionList["moveUpBranch"] );
    shortcutManager.initAction("tree-moveDownBranch", actionList["moveDownBranch"] );
    shortcutManager.initAction("tree-insSubbranch", actionList["insSubbranch"] );
    shortcutManager.initAction("tree-insBranch", actionList["insBranch"] );
    shortcutManager.initAction("tree-editBranch", actionList["editBranch"] );
    shortcutManager.initAction("tree-delBranch", actionList["delBranch"] );
    shortcutManager.initAction("tree-cutBranch", actionList["cutBranch"] );
    shortcutManager.initAction("tree-copyBranch", actionList["copyBranch"] );
    shortcutManager.initAction("tree-pasteBranch", actionList["pasteBranch"] );
    shortcutManager.initAction("tree-pasteSubbranch", actionList["pasteSubbranch"] );
    shortcutManager.initAction("tree-encryptBranch", actionList["encryptBranch"] );
    shortcutManager.initAction("tree-decryptBranch", actionList["decryptBranch"] );
    shortcutManager.initAction("tree-setIcon", actionList["setIcon"] );

    // Синхронизация базы с тулбара веток
    shortcutManager.initAction("misc-syncro", actionList["syncro"] );
}


// Прямых текстов кроме кнопки поиска нет: все освежает setupShortcuts
// по сигналу updateWidgetShortcut. Метод оставлен для единого интерфейса
void TreeScreen::retranslateUi(void)
{
    setupShortcuts();
}


// Порядок кнопок тулбара веток по умолчанию. Объединение десктопного
// и мобильного наборов: лишнее режется режимом в rebuildToolbar.
// spring - невидимый распор (в дефолте стоит под мобильный),
// separator - разделитель, остальные - имена из actionList
QStringList TreeScreen::defaultToolbarOrder(void)
{
 return QStringList()
  << "insSubbranch" << "insBranch"
  << "editBranch" << "delBranch"
  << "separator"
  << "expandAllSubbranch" << "collapseAllSubbranch"
  << "separator"
  << "moveUpBranch" << "moveDownBranch"
  << "separator"
  << "syncro"
  << "spring" << "findInBase";
}


// Режим кнопки: desktop, mobile или обе (пусто - обе)
QString TreeScreen::toolbarItemMode(const QString &command)
{
 if(command=="editBranch" || command=="delBranch")
  return QString("desktop");

 if(command=="spring" || command=="findInBase")
  return QString("mobile");

 return QString();
}


// Перестроение тулбара из списка конфига (или дефолта).
// Вызывается при старте и вживую из диалога настроек.
// Ведущие и задвоенные разделители давятся; неизвестные имена
// из конфига пропускаются с предупреждением а не падением:
// в конфиге может лежать имя из более новой версии (откат бинарника)
void TreeScreen::rebuildToolbar(void)
{
 // Пружины прошлого построения: QToolBar::clear() виджеты не удаляет
 qDeleteAll(toolbarSprings);
 toolbarSprings.clear();

 toolsLine->clear();

 QStringList order=mytetraConfig.getTreeToolbarOrder();

 if(order.isEmpty())
  order=defaultToolbarOrder();

 const bool isMobile=(mytetraConfig.getInterfaceMode()=="mobile");
 bool lastSeparator=true; // Ведущие разделители давятся

 foreach(const QString &command, order)
  {
   if(command=="separator")
    {
     if(!lastSeparator)
      {
       toolsLine->addSeparator();
       lastSeparator=true;
      }

     continue;
    }

   if(command=="spring")
    {
     if(toolbarItemMode(command)=="mobile" && !isMobile)
      continue;

     QWidget *empty=new QWidget(this);
     empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
     toolsLine->addWidget(empty);
     toolbarSprings.append(empty);
     lastSeparator=false;

     continue;
    }

   if(!actionList.contains(command))
    {
     qWarning() << "TreeScreen: skip unknown toolbar command" << command;
     continue;
    }

   const QString mode=toolbarItemMode(command);

   if(mode=="desktop" && isMobile)
    continue;

   if(mode=="mobile" && !isMobile)
    continue;

   insertActionAsButton(toolsLine, actionList[command]);
   lastSeparator=false;
  }
}


void TreeScreen::setupUI(void)
{
 // Наполнение панели инструментов
 toolsLine=new QToolBar(this);

 // Порядок кнопок берётся из конфига (диалог настроек) либо дефолт.
 // Перестроение на лету - через rebuildToolbar()
 rebuildToolbar();

 // Действия буфера обмена, шифрования и иконки ветки на тулбар не
 // выносятся (там только самые важные кнопки), они доступны из
 // контекстного меню. Нулевые кнопки для них не создаются: иначе QToolBar
 // показывает кнопку-расширение » с фантомными пунктами, которые не
 // появляются при расширении панели. Регистрация действий на виджете
 // нужна чтобы продолжали работать шорткаты
 addAction(actionList["cutBranch"]);
 addAction(actionList["copyBranch"]);
 addAction(actionList["pasteBranch"]);
 addAction(actionList["pasteSubbranch"]);

 addAction(actionList["encryptBranch"]);
 addAction(actionList["decryptBranch"]);
 addAction(actionList["setIcon"]);


 knowTreeView=new KnowTreeView(this);
 knowTreeView->setObjectName("knowTreeView");
 knowTreeView->setMinimumSize(150,250);
 knowTreeView->setWordWrap(true);

 // Временно сделан одинарный режим выбора пунктов
 // todo: Множественный режим надо выставить тогда, когда
 // станет ясно, как удалять несколько произвольных веток так, чтобы
 // в процессе удаления QModelIndex нижестоящих еще не удаленных
 // веток не менялся
 // knowTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
 knowTreeView->setSelectionMode(QAbstractItemView::SingleSelection);

 knowTreeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
 
 // Нужно установить правила показа контекстного самодельного меню
 // чтобы оно могло вызываться
 knowTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

 // Представление не должно позволять редактировать элементы обычным путем
 knowTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}


void TreeScreen::setupModels(void)
{
 // Создание и первичная настройка модели
 knowTreeModel = new KnowTreeModel(this);

 // Установка заголовка
 // QStringList headers;
 // headers << tr("Info groups");
 // knowTreeModel->setHeaders(headers);

 // Загрузка данных
 knowTreeModel->initFromXML( mytetraConfig.get_tetradir()+"/mytetra.xml" );

 // Модель подключется к виду
 knowTreeView->setModel(knowTreeModel);

 // Первичная пересборка избранного после загрузки модели:
 // покажет панель если в базе уже есть звездочки
 if(favoritesPanel!=nullptr)
   favoritesPanel->refreshFavorites();
}


// Открытие контекстного меню в дереве разделов
void TreeScreen::onCustomContextMenuRequested(const QPoint &pos)
{
  qDebug() << "In TreeScreen::onCustomContextMenuRequested";

  // Конструирование меню
  QMenu menu(this);
  menu.addAction(actionList["insSubbranch"]);
  menu.addAction(actionList["insBranch"]);
  menu.addAction(actionList["editBranch"]);
  menu.addAction(actionList["delBranch"]);
  menu.addAction(actionList["setIcon"]);
  menu.addSeparator();
  menu.addAction(actionList["expandAllSubbranch"]);
  menu.addAction(actionList["collapseAllSubbranch"]);
  menu.addSeparator();
  menu.addAction(actionList["moveUpBranch"]);
  menu.addAction(actionList["moveDownBranch"]);
  menu.addSeparator();
  menu.addAction(actionList["cutBranch"]);
  menu.addAction(actionList["copyBranch"]);
  menu.addAction(actionList["pasteBranch"]);
  menu.addAction(actionList["pasteSubbranch"]);
  menu.addSeparator();
  menu.addAction(actionList["encryptBranch"]);
  menu.addAction(actionList["decryptBranch"]);

  // Получение индекса выделенной ветки
  QModelIndex index=getCurrentItemIndex();

  // Выясняется, зашифрована ли ветка или нет
  QString cryptFlag=knowTreeModel->getItem(index)->getField("crypt");

  // Выясняется, зашифрована ли родительская ветка
  QString parentCryptFlag=knowTreeModel->getItem(index)->parent()->getField("crypt");

  // Если ветка не зашифрована
  // Или ветка зашифрована, но пароль успешно введен
  if(cryptFlag!="1" ||
     (cryptFlag=="1" && globalParameters.getCryptKey().length()>0))
   {

    // Если во внутреннем буфере обмена есть ветки
    // или есть вырезанная ветка, ожидающая вставки-перемещения,
    // соответсвующие пункты становятся активными
    bool isBranch=false;
    const QMimeData *mimeData = internalClipboard->mimeData();
     if(mimeData!=nullptr and
        mimeData->hasFormat(FixedParameters::appTextId+"/branch"))
     {
        isBranch=true;
     }

     if(!isBranch)
      isBranch=!knowTreeModel->cutBranchId().isEmpty();

     if( isBranch )
     {
      actionList["pasteBranch"]->setEnabled(true);
      actionList["pasteSubbranch"]->setEnabled(true);
     }
    else
     {
      actionList["pasteBranch"]->setEnabled(false);
      actionList["pasteSubbranch"]->setEnabled(false);
     }


    // ----------------------------
    // Подсветка пунктов шифрования
    // ----------------------------

    // Если ветка незашифрована
    if(cryptFlag!="1")
     {
      // Шифровать можно
      // Дешифровать нельзя
      actionList["encryptBranch"]->setEnabled(true);
      actionList["decryptBranch"]->setEnabled(false);
     }
    else
     {
      // Ветка зашифрована

      // Шифровать нельзя
      actionList["encryptBranch"]->setEnabled(false);

      // Дешифровать можно только если верхнележащая ветка незашифрована
      if(parentCryptFlag!="1")
       actionList["decryptBranch"]->setEnabled(true);
      else
       actionList["decryptBranch"]->setEnabled(false);
     }
   }

  
  // Включение отображения меню на экране
  // menu.exec(event->globalPos());
  menu.exec(knowTreeView->viewport()->mapToGlobal(pos));
}


void TreeScreen::setupSignals(void)
{
 // Соединение сигнал-слот чтобы показать контекстное меню по правому клику на ветке
 connect(knowTreeView, &KnowTreeView::customContextMenuRequested,
         this,         &TreeScreen::onCustomContextMenuRequested);

 // Соединение сигнал-слот чтобы показать контекстное меню по долгому нажатию
 connect(knowTreeView, &KnowTreeView::tapAndHoldGestureFinished,
         this,         &TreeScreen::onCustomContextMenuRequested);


 // Сигнал что ветка выбрана мышкой или стрелками на клавиатуре
 // через selection-модель. Реакция происходит на перемещение курсора по дереву
 if(mytetraConfig.getInterfaceMode()=="desktop")
 {
   connect(knowTreeView->selectionModel(), &QItemSelectionModel::currentRowChanged,
           this,                           &TreeScreen::onKnowtreeClicked);
 }
 
 if(mytetraConfig.getInterfaceMode()=="mobile")
 {
   connect(knowTreeView, &KnowTreeView::clicked,
           this,         &TreeScreen::onKnowtreeClicked);
 }


 // Сигнал что ветка выбрана кликом мышки
 // используется для возможности ввести пароль, если в базе одна корневая ветка, и она зашифрована
 connect(knowTreeView, &KnowTreeView::pressed,
         this,         &TreeScreen::onKnowtreeClicked); // &TreeScreen::checkIfOneRootCryptItem


 // Сигнал чтобы открыть на редактирование параметры записи при двойном клике
 // connect(knowTreeView, SIGNAL(doubleClicked(const QModelIndex &)),
 //         actionList["editBranch"], SLOT(trigger(void)));

 // Сигнал что ветка выбрана мышкой
 // connect(knowTreeView,SIGNAL(pressed(const QModelIndex &)),
 //         this,SLOT(on_knowTreeView_clicked(const QModelIndex &)));
 // connect(knowTreeView, SIGNAL(clicked(const QModelIndex &)),
 //         this, SLOT(on_knowTreeView_clicked(const QModelIndex &)));

 // Обновление горячих клавиш, если они были изменены
 connect(&shortcutManager, &ShortcutManager::updateWidgetShortcut, this, &TreeScreen::setupShortcuts);
}


void TreeScreen::assembly(void)
{
 treeScreenLayout=new QVBoxLayout();
 treeScreenLayout->setObjectName("treescreen_QVBoxLayout");

 treeScreenLayout->addWidget(toolsLine);

 // Зазора между тулбаром и верхней веткой нет: штатный spacing
 // раскладки давал полосу пустоты под тулбаром
 treeScreenLayout->setSpacing(0);

 // Избранное и дерево в двигаемом разделителе: долю панели
 // выбирает пользователь. Панель не схлопывается в ноль чтобы
 // ее нельзя было потерять drag-ом, дерево забирает остаток
 QSplitter *favoritesSplitter=new QSplitter(Qt::Vertical, this);
 favoritesSplitter->setObjectName("favoritesSplitter");
 favoritesSplitter->addWidget(favoritesPanel);
 favoritesSplitter->addWidget(knowTreeView);
 favoritesSplitter->setStretchFactor(0, 0);
 favoritesSplitter->setStretchFactor(1, 1);
 favoritesSplitter->setCollapsible(0, false);
 favoritesSplitter->setCollapsible(1, false);

 // Ручной драг: панель уходит из компактного режима,
 // список наливается на выставленную высоту
 connect(favoritesSplitter, &QSplitter::splitterMoved,
         favoritesPanel, &FavoritesPanel::setUserResized);

 treeScreenLayout->addWidget(favoritesSplitter, 1);

 setLayout(treeScreenLayout);

 // Границы убираются, так как данный объект будет использоваться как виджет
 QLayout *lt;
 lt=layout();
 lt->setContentsMargins(0,1,0,0);
}


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
    globalParameters.getCryptKey().length()==0)
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
 if(globalParameters.getCryptKey().size()==0)
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

   if(mytetraConfig.get_cutbranchconfirm()) enable_question=true;
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
    internalClipboard->clear();

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
    internalClipboard->setMimeData(branch_clipboard_data);

    return true;
}


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
void TreeScreen::pasteBranch(void)
{
  qDebug() << "In paste_branch";

  pasteBranchSmart(true);
}


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
 const QMimeData *mimeData = internalClipboard->mimeData();

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
 branch=qobject_cast<const ClipboardBranch *>(internalClipboard->mimeData());
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
void TreeScreen::encryptBranch(void)
{
 // Если пароль в данной сессии не вводился
 if(globalParameters.getCryptKey().size()==0)
  {
   // Запрашивается пароль
   Password password;
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
void TreeScreen::decryptBranch(void)
{
 // Если пароль в данной сессии не вводился
 if(globalParameters.getCryptKey().size()==0)
  {
   // Запрашивается пароль
   Password password;
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
void TreeScreen::setIcon(void)
{
  QString startDirectory=mytetraConfig.get_tetradir()+"/"+FixedParameters::iconsRelatedDirectory;
  qDebug() << "Set start directory for select icon: " << startDirectory;

  // Создается окно выбора файла иконки
  IconSelectDialog iconSelectDialog;
  iconSelectDialog.setDefaultSection( mytetraConfig.getIconCurrentSectionName() );
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
     iconSelectDialog.getCurrentSection()!=mytetraConfig.getIconCurrentSectionName())
   mytetraConfig.setIconCurrentSectionName( iconSelectDialog.getCurrentSection() );
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
    Password password;
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
  if(startItem->getField("crypt")=="1" && globalParameters.getCryptKey().length()==0)
  {
    showMessageBox(tr("You are importing into an encrypted item.\nPlease click Ok and enter the password.\nAll data imported will be encrypted."));

    // Запрашивается пароль
    Password password;
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
    if(globalParameters.getCryptKey().length()==0) {

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
       globalParameters.getCryptKey().length()==0)
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
        Password password;
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
        if(mytetraConfig.getInterfaceMode()=="mobile")
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
        globalParameters.getWindowSwitcher()->switchFromTreeToRecordtable();
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
  if(mytetraConfig.getInterfaceMode()=="mobile")
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
QModelIndex TreeScreen::getCurrentItemIndex(void)
{
 return knowTreeView->selectionModel()->currentIndex();
}


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
void TreeScreen::setReadOnly(bool state)
{
 const QStringList mutatingActions=QStringList()
  << "moveUpBranch" << "moveDownBranch"
  << "insSubbranch" << "insBranch"
  << "editBranch" << "delBranch"
  << "cutBranch" << "pasteBranch" << "pasteSubbranch"
  << "encryptBranch" << "decryptBranch"
  << "setIcon" << "syncro";

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


QAction *TreeScreen::toolAction(const QString &command)
{
 if(!actionList.contains(command))
  return nullptr;

 return actionList[command];
}


// Клик по кнопке синхронизации: та же MainWindow::synchronization(),
// что раньше дёргалась с тулбара записей
void TreeScreen::onSyncroClick(void)
{
 find_object<MainWindow>("mainwindow")->synchronization();
}


// Разблокировка кнопки синхронизации по окончании команды.
// Подключено в MainWindow к завершению синхрокоманды
void TreeScreen::onSyncroCommandFinishWork(void)
{
 actionList["syncro"]->setEnabled(true);
}


void TreeScreen::setSyncroEnabled(bool state)
{
 actionList["syncro"]->setEnabled(state);
}


bool TreeScreen::isSyncroEnabled(void) const
{
 return actionList["syncro"]->isEnabled();
}


// Метод, следящий, не стало ли дерево содержать только незашифрованные записи
// Если в дереве нет шифрования, задается вопрос, нужно ли сбросить пароль
void TreeScreen::treeCryptControl(void)
{
 // Если в дереве нет шифрования
 if(knowTreeModel->isContainsCryptBranches()==false)
  {
   // Запускается диалог сброса пароля шифрования
   Password pwd;
   pwd.resetPassword();
  }
}


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
void TreeScreen::setFocusToBaseWidget()
{
    knowTreeView->setFocus();
}

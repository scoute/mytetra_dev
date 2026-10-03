#include <QAbstractItemView>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QString>
#include <QMap>
#include <QAction>
#include <QItemSelectionModel>
#include <QVBoxLayout>
#include <QToolBar>
#include <QInputDialog>
#include <QSplitter>
#include <QTimer>

#include "TreeScreen.h"
#include "KnowTreeView.h"
#include "PublishedBadgeDelegate.h"
#include "views/subscriptions/SubscriptionPanel.h"
#include "models/teamProfile/TeamProfile.h"
#include "libraries/SharedDirWatcher.h"
#include "libraries/BranchPublisher.h"

#include "models/recordTable/RecordTableData.h"
#include "views/recordTable/RecordTableScreen.h"
#include "models/appConfig/AppConfig.h"
#include "views/mainWindow/MainWindow.h"
#include "models/tree/TreeItem.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/ClipboardBranch.h"
#include "views/record/MetaEditor.h"
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
#include "libraries/ActionLogger.h"
#include "libraries/SubscriptionImportEngine.h"
#include "models/subscription/SubscriptionRegistry.h"
#include "views/subscriptions/BranchSliceDialog.h"
#include "views/subscriptions/ChangeViewDialog.h"
#include "views/appConfigWindow/AppConfigDialog.h"


extern AppConfig mytetraConfig;
extern GlobalParameters globalParameters;
extern ShortcutManager shortcutManager;
extern InternalClipboard *internalClipboard;
extern TeamProfile teamProfile;
extern SharedDirWatcher sharedDirWatcher;
extern ActionLogger actionLogger;
extern SubscriptionRegistry subscriptionRegistry;


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

  // Debounce отложенного автообновления публикаций: правки текста записей
  // прилетают часто (каждое сохранение текстового поля), экспорт и сверка
  // дайджестов на каждое сохранение — дорого, поэтому обновления копятся
  publicationAutoUpdateTimer=new QTimer(this);
  publicationAutoUpdateTimer->setSingleShot(true);
  publicationAutoUpdateTimer->setInterval(5000);
  connect(publicationAutoUpdateTimer, &QTimer::timeout,
          this, &TreeScreen::onPublicationAutoUpdateTimeout);

  // Первичное построение панели подписок и бейджей публикаций
  refreshPublicationState();
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

 // Шифрование ветки (пока нет иконки)
 ac = new QAction(this);
 connect(ac, &QAction::triggered, this, &TreeScreen::encryptBranch);
 actionList["encryptBranch"]=ac;

 // Расшифровка ветки, т. е. снятие пароля (пока нет иконки)
 ac = new QAction(this);
 connect(ac, &QAction::triggered, this, &TreeScreen::decryptBranch);
 actionList["decryptBranch"]=ac;

 // Публикация ветки в общий каталог SyncTetra (shareddir)
 ac = new QAction(tr("Publish branch to common directory..."), this);
 ac->setStatusTip(tr("Publish branch to common directory (shared)"));
 connect(ac, &QAction::triggered, this, &TreeScreen::publishBranch);
 actionList["publishBranch"]=ac;

 // Принудительное обновление публикации (автоапдейт идёт сам при правках;
 // этот пункт — дожать вручную, повторить после сбоя, починить зеркало)
 ac = new QAction(tr("Force branch update / recovery..."), this);
 ac->setStatusTip(tr("Force update of branch publication now (auto-update runs on edits anyway) and repair the sync/ mirror if broken"));
 connect(ac, &QAction::triggered, this, &TreeScreen::updatePublication);
 actionList["updatePublication"]=ac;

 // Отзыв публикации ветки из общего каталога (shareddir)
 ac = new QAction(tr("Revoke publication..."), this);
 ac->setStatusTip(tr("Revoke publication of branch from common directory (shared)"));
 connect(ac, &QAction::triggered, this, &TreeScreen::revokePublication);
 actionList["revokePublication"]=ac;

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
}


void TreeScreen::setupShortcuts(void)
{
    qDebug() << "Setup shortcut for" << this->metaObject()->className();

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
}


void TreeScreen::setupUI(void)
{
 // Наполнение панели инструментов
 toolsLine=new QToolBar(this);

 insertActionAsButton(toolsLine, actionList["insSubbranch"]);
 insertActionAsButton(toolsLine, actionList["insBranch"]);

 if(mytetraConfig.getInterfaceMode()=="desktop")
 {
   insertActionAsButton(toolsLine, actionList["editBranch"]);
   insertActionAsButton(toolsLine, actionList["delBranch"]);
 }

 toolsLine->addSeparator();

 insertActionAsButton(toolsLine, actionList["expandAllSubbranch"]);
 insertActionAsButton(toolsLine, actionList["collapseAllSubbranch"]);

 toolsLine->addSeparator();

 insertActionAsButton(toolsLine, actionList["moveUpBranch"]);
 insertActionAsButton(toolsLine, actionList["moveDownBranch"]);

 if(mytetraConfig.getInterfaceMode()=="mobile")
 {
     // Вставка невидимого автоматически расталкивающего виджета
     QWidget* empty = new QWidget(this);
     empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
     toolsLine->addWidget(empty);

     insertActionAsButton(toolsLine, actionList["findInBase"]); // Клик по этой кнопке связывается с действием в MainWindow
 }

 // Добавление скрытых действий, которые не видны на тулбаре, но видны на контекстном меню
 insertActionAsButton(toolsLine, actionList["cutBranch"], false);
 insertActionAsButton(toolsLine, actionList["copyBranch"], false);
 insertActionAsButton(toolsLine, actionList["pasteBranch"], false);
 insertActionAsButton(toolsLine, actionList["pasteSubbranch"], false);

 insertActionAsButton(toolsLine, actionList["encryptBranch"], false);
 insertActionAsButton(toolsLine, actionList["decryptBranch"], false);
 insertActionAsButton(toolsLine, actionList["setIcon"], false);


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
  menu.addSeparator();
  menu.addAction(actionList["publishBranch"]);
  menu.addAction(actionList["updatePublication"]);
  menu.addAction(actionList["revokePublication"]);

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

  // Состояние пунктов публикации/обновления/отзыва
  updatePublicationActionsState();

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

  // Изменение каталога обмена: обновить панель и бейджи
  connect(&sharedDirWatcher, &SharedDirWatcher::sharedDirChanged,
          this,              &TreeScreen::onSharedDirChanged);

  // Обновление при изменении профиля команды (поменяли shareddir/имя)
  connect(&teamProfile, &TeamProfile::teamProfileChanged,
          this,         &TreeScreen::onSharedDirChanged);
}


void TreeScreen::assembly(void)
{
 treeScreenLayout=new QVBoxLayout();
 treeScreenLayout->setObjectName("treescreen_QVBoxLayout");

 treeScreenLayout->addWidget(toolsLine);

 // Панель подписок (левая колонка, под деревом разделов)
 subscriptionPanel=new SubscriptionPanel(this);

 // Делегат бейджей опубликованных веток (наследуется от делегата
 // подсветки вырезанной ветки, обе дорисовки живут)
 publishedBadgeDelegate=qobject_cast<PublishedBadgeDelegate*>(knowTreeView->itemDelegate());

 // Дерево разделов и панель подписок в одном вертикальном разделителе,
 // чтобы пользователь мог изменять их высоту
 QSplitter *mainSplitter=new QSplitter(Qt::Vertical, this);
 mainSplitter->setObjectName("treeScreen_QSplitter");
 mainSplitter->addWidget(knowTreeView);
 mainSplitter->addWidget(subscriptionPanel);
 mainSplitter->setStretchFactor(0,1);
 mainSplitter->setCollapsible(0,false);
 mainSplitter->setCollapsible(1,false);
 mainSplitter->setChildrenCollapsible(false);
 mainSplitter->setSizes(QList<int>() << 400 << 150);

 // Запрос на открытие среза подписки обрабатывается самим TreeScreen
 connect(subscriptionPanel, &SubscriptionPanel::openSliceRequested,
         this,              &TreeScreen::openSubscriptionSlice);

 // Запрос на просмотр изменений и импорт обрабатывается TreeScreen
 connect(subscriptionPanel, &SubscriptionPanel::showChangesRequested,
         this,              &TreeScreen::showSubscriptionChanges);

 // Переход к исходной ветке при двойном клике на своей публикации
 connect(subscriptionPanel, &SubscriptionPanel::focusLocalBranchRequested,
         this,              &TreeScreen::focusLocalBranchInTree);

 // Принудительное обновление собственной публикации из панели «Мои публикации»
 connect(subscriptionPanel, &SubscriptionPanel::forceUpdatePublicationRequested,
         this,              &TreeScreen::forceUpdateOwnPublication);

 // Отзыв собственной публикации из панели «Мои публикации»
 connect(subscriptionPanel, &SubscriptionPanel::revokePublicationRequested,
         this,              &TreeScreen::revokeOwnPublication);

 treeScreenLayout->addWidget(mainSplitter,1);

 setLayout(treeScreenLayout);

 // Границы убираются, так как данный объект будет использоваться как виджет
 QLayout *lt;
 lt=layout();
 lt->setContentsMargins(0,2,0,0);
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

   qDebug() << "Save new tree finish";
  }


 // Если была вырезана ветка, а ее больше нет - состояние вырезания сбрасывается
 if(!knowTreeModel->cutBranchId().isEmpty() &&
    knowTreeModel->getItemById(knowTreeModel->cutBranchId())==nullptr)
  knowTreeModel->clearCutBranchId();


 // Разблокируется главное окно
 find_object<MainWindow>("mainwindow")->setEnabled(true);
 find_object<MainWindow>("mainwindow")->blockSignals(false);

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


 // Установка курсора на новую созданную ветку
 TreeItem *pasted_branch_item=knowTreeModel->getItemById(pasted_branch_id);
 QStringList pasted_branch_path=pasted_branch_item->getPath();
 find_object<MainWindow>("mainwindow")->setTreePosition(pasted_branch_path);

 // Сохранение дерева веток
 find_object<TreeScreen>("treeScreen")->saveKnowTree();

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
    setCursorToId(importNodeId); // Курсор устанавливается на только что импортированную ветку

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
  // имена, авторы, теги и url заметок, состав веток. Словари
  // автодополнения их собирают заново, пока экран открыт. Текст
  // заметок сюда не попадает, он сохраняется отдельно
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



void TreeScreen::publishBranch(void)
{
  publishCurrentBranch(false);
}


// Обновление публикации ветки в общий каталог (shareddir)


void TreeScreen::updatePublication(void)
{
  publishCurrentBranch(true);
}


// Публикация (isUpdate=false) или обновление (isUpdate=true) текущей ветки


void TreeScreen::publishCurrentBranch(bool isUpdate)
{
  QModelIndex index=getCurrentItemIndex();
  if(!index.isValid())
    return;

  TreeItem *branchItem=knowTreeModel->getItem(index);
  QString branchId=branchItem->getField("id");
  QString branchName=branchItem->getField("name");

  // Фактический каталог обмена
  QString sharedDir=teamProfile.getSharedDir();
  if(sharedDir.isEmpty())
  {
    QMessageBox::critical(this, tr("Publish branch"),
                          tr("Shared directory is not defined. "
                             "Set it in the settings."));
    return;
  }

  // Каталог обмена создается, если его нет
  QDir shared(sharedDir);
  if(!shared.exists() && !shared.mkpath("."))
  {
    QMessageBox::critical(this, tr("Publish branch"),
                          tr("Can't create shared directory:\n"));
    return;
  }

  // Проверка: вложенная публикация (ветка не входит в опубликованную
  // и не содержит опубликованных подветок). Собственный ключ ветки из набора
  // исключается: иначе обновление уже опубликованной ветки всегда отклонялось
  // бы как «вложенная публикация» по её же ключу
  QSet<QString> publishedKeys=BranchPublisher::listPublishedBranchKeys(sharedDir);
  publishedKeys.remove(branchId.section('_', 0, 0));
  if(BranchPublisher::hasNestedPublication(branchItem, publishedKeys))
  {
    QMessageBox::warning(this, tr("Publish branch"),
                         tr("The branch is nested in an already published branch "
                            "or contains published sub-branches.\n"
                            "Publishing nested branches is not supported."));
    return;
  }

  // Проверка: зашифрованная ветка публикуется расшифрованной
  if(knowTreeModel->isItemContainsCryptBranches(branchItem))
  {
    QMessageBox::StandardButton answer=QMessageBox::question(
          this, tr("Publish branch"),
          tr("The branch contains encrypted content.\n"
             "Encrypted data is not stored in the common directory; "
             "the publication will be in decrypted form.\n\n"
             "Continue?"),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if(answer!=QMessageBox::Yes)
      return;
  }

  // Определение операции и факт наличия существующей публикации
  QString ownerDirName=BranchPublisher::buildOwnerDirName(teamProfile);
  QString branchDirName=BranchPublisher::buildBranchDirName(branchId, branchName);

  QString existingPublication=BranchPublisher::findPublicationDir(sharedDir,
                                                                  ownerDirName,
                                                                  branchDirName);
  bool isAlreadyPublished=!existingPublication.isEmpty();

  if(!isUpdate && isAlreadyPublished)
  {
    QMessageBox::StandardButton answer=QMessageBox::question(
          this, tr("Publish branch"),
          tr("The branch is already published at:\n%1\n\n"
             "Update the publication?").arg(existingPublication),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if(answer!=QMessageBox::Yes)
      return;
    isUpdate=true;
  }

  if(isUpdate && !isAlreadyPublished)
  {
    QMessageBox::warning(this, tr("Update publication"),
                         tr("Publication of the branch was not found."));
    return;
  }

  // Выполнение публикации/обновления
  BranchPublisher::Operation operation=isUpdate ? BranchPublisher::Operation::Update
                                                : BranchPublisher::Operation::Publish;

  BranchPublisher::Result result=BranchPublisher::publishBranch(knowTreeModel,
                                                                branchItem,
                                                                teamProfile,
                                                                operation);

  if(!result.success)
  {
    QMessageBox::critical(this, tr("Publish branch"),
                          tr("Publication failed:\n%1").arg(result.errorMessage));
    return;
  }

  // Предупреждение о большом объёме (рост журнала при каждой публикации).
  // Без git журнала нет — предупреждать не о чем
  if(result.largeContent && !result.journalDisabled)
  {
    QMessageBox::warning(this, tr("Publish branch"),
                         tr("The published content is large (more than 20 MB).\n"
                            "The git-history in the common directory will grow "
                            "with each publication."));
  }

  // Уведомление об успехе. Без git в PATH журнал/восстановление отключены —
  // подсказка об этом добавляется к тексту (дифф/импорт/детект работают)
  QString successText=tr("Branch \"%1\" successfully %2.\n"
                         "Publication directory:\n%3\n"
                         "Publish version: %4\n\n"
                         "The update will travel to colleagues via Syncthing "
                         "(shared folder = sync/).")
                         .arg(branchName)
                         .arg(isUpdate ? tr("updated") : tr("published"))
                         .arg(result.publicationDir)
                         .arg(result.publishVersion);
  if(result.journalDisabled)
    successText+=tr("\n\nJournal is unavailable (git not found in PATH):\n"
                    "recovery from the journal is disabled.");

  // Уведомление об успехе
  QMessageBox box(this);
  box.setIcon(QMessageBox::Information);
  box.setWindowTitle(isUpdate ? tr("Update publication") : tr("Publish branch"));
  box.setText(successText);
  QPushButton *settingsButton=box.addButton(tr("Settings..."), QMessageBox::ActionRole);
  box.addButton(tr("Close"), QMessageBox::RejectRole);
  box.exec();

  if(box.clickedButton()==settingsButton)
  {
    AppConfigDialog dialog("pageTeam", this);
    dialog.exec();
  }

  // Запись действия в лог
  QMap<QString, QString> logData;
  logData["branchId"]=branchId;
  logData["branchName"]=branchName;
  logData["publicationPath"]=result.publicationDir;
  logData["publishVersion"]=QString().number(result.publishVersion);
  actionLogger.addAction(isUpdate ? "updatePublication" : "publishBranch", logData);

  // Журнал сразу фиксирует новое легитимное состояние sync/ (без debounce),
  // чтобы предложение восстановления не всплывало после собственной публикации
  sharedDirWatcher.requestImmediateSnapshot();

  // Публикация появилась/изменилась — обновляются бейджи и панель
  refreshPublicationState();
}


// Отзыв публикации ветки из общего каталога (shareddir)


void TreeScreen::revokePublication(void)
{
  QModelIndex index=getCurrentItemIndex();
  if(!index.isValid())
    return;

  TreeItem *branchItem=knowTreeModel->getItem(index);
  QString branchId=branchItem->getField("id");
  QString branchName=branchItem->getField("name");

  QString sharedDir=teamProfile.getSharedDir();
  if(sharedDir.isEmpty())
    return;

  QString ownerDirName=BranchPublisher::buildOwnerDirName(teamProfile);
  QString branchDirName=BranchPublisher::buildBranchDirName(branchId, branchName);

  QString publicationDir=BranchPublisher::findPublicationDir(sharedDir, ownerDirName,
                                                             branchDirName);
  if(publicationDir.isEmpty())
  {
    QMessageBox::information(this, tr("Revoke publication"),
                             tr("Publication of the branch was not found."));
    return;
  }

  QMessageBox::StandardButton answer=QMessageBox::question(
        this, tr("Revoke publication"),
        tr("Revoke the publication at:\n%1\n\n"
           "The publication directory will be moved to the trash.").arg(publicationDir),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if(answer!=QMessageBox::Yes)
    return;

  BranchPublisher::revokePublication(publicationDir);

  QMessageBox::information(this, tr("Revoke publication"),
                           tr("Publication revoked."));

  QMap<QString, QString> logData;
  logData["branchId"]=branchId;
  logData["branchName"]=branchName;
  logData["publicationPath"]=publicationDir;
  actionLogger.addAction("revokePublication", logData);

  // Журнал сразу фиксирует отзыв публикации: осознанное удаление не
  // должно восприниматься как потеря данных («восстановить из журнала?»)
  sharedDirWatcher.requestImmediateSnapshot();

  // Публикация исчезла из общего каталога — обновляются бейджи и панель
  refreshPublicationState();
}


// Изменение общего каталога или профиля команды


void TreeScreen::updatePublicationActionsState(void)
{
  // По умолчанию пункты выключены
  actionList["publishBranch"]->setEnabled(false);
  actionList["updatePublication"]->setEnabled(false);
  actionList["revokePublication"]->setEnabled(false);

  QModelIndex index=getCurrentItemIndex();
  if(!index.isValid())
    return;

  TreeItem *branchItem=knowTreeModel->getItem(index);

  QString sharedDir=teamProfile.getSharedDir();
  if(sharedDir.isEmpty())
    return;

  QString ownerDirName=BranchPublisher::buildOwnerDirName(teamProfile);
  QString branchDirName=BranchPublisher::buildBranchDirName(branchItem->getField("id"),
                                                            branchItem->getField("name"));

  QString publicationDir=BranchPublisher::findPublicationDir(sharedDir, ownerDirName, branchDirName);
  bool isPublished=!publicationDir.isEmpty();

  actionList["publishBranch"]->setEnabled(!isPublished);
  actionList["updatePublication"]->setEnabled(isPublished);
  actionList["revokePublication"]->setEnabled(isPublished);
}


void TreeScreen::onSharedDirChanged(void)
{
  refreshPublicationState();
}


// Открытие среза публикации в режиме только чтения.
// Для подсветки изменений считается дифф с базовой точки подписки;
// без подписки (нет baseline) срез показывается без подсветки


void TreeScreen::openSubscriptionSlice(const QString &branchId, const QString &publicationDir)
{
  if(publicationDir.isEmpty())
    return;

  QList<DiffChange> sliceChanges;

  SubscriptionImportEngine sliceEngine;
  sliceEngine.setPublicationDir(publicationDir);
  if(sliceEngine.loadPublication() && sliceEngine.isPublicationComplete())
  {
    const QString baselineSnapshot=subscriptionRegistry.getSubscription(branchId).baselineSnapshot;
    if(!baselineSnapshot.isEmpty())
      sliceChanges=sliceEngine.pendingChanges(&subscriptionRegistry, knowTreeModel,
                                              baselineSnapshot, nullptr);
  }

  BranchSliceDialog sliceDialog(publicationDir, this, sliceChanges);
  sliceDialog.exec();
}


// Двойной клик на собственной публикации в панели «Подписки»:
// позиционируем курсор на исходную ветку в дереве разделов


void TreeScreen::focusLocalBranchInTree(const QString &branchId)
{
  if(branchId.isEmpty())
    return;

  TreeItem *targetItem=knowTreeModel->getItemById(branchId);
  if(!targetItem)
  {
    QMessageBox::warning(this, tr("Branch"),
                         tr("Исходная ветка не найдена в базе."));
    return;
  }

  const QModelIndex index=knowTreeModel->getIndexByItem(targetItem);

  // Раскрываем всех предков, чтобы ветка стала видимой
  QModelIndex parentIndex=index.parent();
  while(parentIndex.isValid())
  {
    knowTreeView->expand(parentIndex);
    parentIndex=parentIndex.parent();
  }

  knowTreeView->setCurrentIndex(index);
  knowTreeView->selectionModel()->setCurrentIndex(
      index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
  knowTreeView->scrollTo(index);
  knowTreeView->setFocus();
}


// Принудительное обновление собственной публикации из панели «Мои публикации»:
// позиционирование на исходную ветку + штатный update-поток
// (тот же, что пункт дерева Force branch update / recovery)


void TreeScreen::forceUpdateOwnPublication(const QString &branchId)
{
  if(branchId.isEmpty())
    return;

  TreeItem *targetItem=knowTreeModel->getItemById(branchId);
  if(!targetItem)
  {
    QMessageBox::warning(this, tr("Force branch update"),
                         tr("Исходная ветка не найдена в базе."));
    return;
  }

  const QModelIndex index=knowTreeModel->getIndexByItem(targetItem);

  // Раскрываем всех предков, чтобы ветка стала видимой
  QModelIndex parentIndex=index.parent();
  while(parentIndex.isValid())
  {
    knowTreeView->expand(parentIndex);
    parentIndex=parentIndex.parent();
  }

  knowTreeView->setCurrentIndex(index);
  knowTreeView->selectionModel()->setCurrentIndex(
      index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
  knowTreeView->scrollTo(index);

  publishCurrentBranch(true);
}


// Отзыв собственной публикации из панели «Мои публикации»:
// позиционирование на исходную ветку + штатный revoke-поток
// (диалог подтверждения, корзина, журнал, лог, обновление панели)
void TreeScreen::revokeOwnPublication(const QString &branchId)
{
  if(branchId.isEmpty())
    return;

  TreeItem *targetItem=knowTreeModel->getItemById(branchId);
  if(!targetItem)
  {
    QMessageBox::warning(this, tr("Revoke publication"),
                         tr("Исходная ветка не найдена в базе."));
    return;
  }

  const QModelIndex index=knowTreeModel->getIndexByItem(targetItem);

  // Раскрываем всех предков, чтобы ветка стала видимой
  QModelIndex parentIndex=index.parent();
  while(parentIndex.isValid())
  {
    knowTreeView->expand(parentIndex);
    parentIndex=parentIndex.parent();
  }

  knowTreeView->setCurrentIndex(index);
  knowTreeView->selectionModel()->setCurrentIndex(
      index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
  knowTreeView->scrollTo(index);

  revokePublication();
}


// Просмотр изменений публикации и выборочный импорт (Фаза 4)


void TreeScreen::showSubscriptionChanges(const QString &branchId, const QString &publicationDir)
{
  qDebug() << "showSubscriptionChanges: branchId" << branchId
           << "publicationDir" << publicationDir;

  if(publicationDir.isEmpty())
    return;

  // Движок загружает актуальный срез публикации и её метаданные
  SubscriptionImportEngine engine;
  engine.setPublicationDir(publicationDir);

  // Дифф/импорт строятся только по целостной версии публикации.
  // Если Syncthing доставил лишь часть обновления — показываем «ожидание
  // данных» и даём повторную проверку (без полного прохода: готовность
  // определяется по манифесту в момент чтения)
  while(true)
  {
    QString loadError;
    if(!engine.loadPublication(&loadError))
    {
      qDebug() << "showSubscriptionChanges: loadPublication FAILED:" << loadError;
      QMessageBox::warning(this, tr("Import"), loadError);
      return;
    }

    if(engine.isPublicationComplete())
      break;

    QMessageBox* box(new QMessageBox(QMessageBox::Information, tr("Changes"),
                    tr("Обновление ещё в пути: данные публикации доехали не "
                       "полностью. Дифф и импорт выполняются только по целостной "
                       "версии — попробуйте после завершения синхронизации "
                       "(проверьте, что Syncthing на обоих устройствах "
                       "синхронизирует папку sync/ и сошёлся)."),
                    QMessageBox::NoButton, this));
    QPushButton* retryButton(box->addButton(tr("Проверить статус"), QMessageBox::AcceptRole));
    box->addButton(QMessageBox::Cancel);
    box->exec();
    bool retryClicked(box->clickedButton()==retryButton);
    delete box;
    if(!retryClicked)
      return;
  }
  qDebug() << "showSubscriptionChanges: publication loaded and complete";

  // Метаданные валидны только после успешной загрузки (до loadPublication
  // getMeta() возвращает пустую структуру — брать раньше нельзя)
  const PublicationMeta meta=engine.getMeta();

  // Битый meta.json: без branchId нельзя ни сверить базовую точку, ни
  // привязать импорт — иначе зацикливание на «первом импорте» с пустым именем
  if(meta.branchId.isEmpty())
  {
    qDebug() << "showSubscriptionChanges: empty branchId in meta.json";
    QMessageBox::warning(this, tr("Import"),
                         tr("Метаданные публикации повреждены (meta.json без branchId)."));
    return;
  }

  // Первый импорт: полное поддерево создаётся в зону imported/<owner>/<title>.
  // Пока у подписки нет локальной копии, изменения не просматриваются поштучно —
  // создаётся вся структура (ветки + записи), после чего базовая точка выравнивается.
  // Проверка идёт до вычисления диффа, чтобы первый импорт (в т.ч. сразу
  // в момент подписки) не считал ненужные изменения
  qDebug() << "showSubscriptionChanges: check local copy, branchId" << meta.branchId;
  if(subscriptionRegistry.getSubscription(branchId).branchesMap.value(meta.branchId).isEmpty())
  {
    QString importError;
    if(!engine.importFullSubtree(&subscriptionRegistry, knowTreeModel, &importError))
    {
      qDebug() << "showSubscriptionChanges: importFullSubtree FAILED:" << importError;
      QMessageBox::warning(this, tr("Import"), importError);
      return;
    }

    const QString copyId=engine.localTargetBranchId();
    qDebug() << "showSubscriptionChanges: imported copy root" << copyId;

    subscriptionRegistry.setBaselineState(branchId, engine.buildBaselineSnapshot(),
                                          meta.publishVersion);
    saveKnowTree();

    TreeItem *copyItem=knowTreeModel->getItemById(copyId);
    if(copyItem)
    {
      const QModelIndex copyIndex=knowTreeModel->getIndexByItem(copyItem);
      knowTreeView->setCurrentIndex(copyIndex);
      knowTreeView->selectionModel()->setCurrentIndex(
          copyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
      knowTreeView->scrollTo(copyIndex);
      updateBranchOnScreen(copyIndex);
    }

    QMap<QString, QString> importData;
    importData["branchId"]=branchId;
    importData["branchName"]=meta.title;
    importData["publicationPath"]=publicationDir;
    importData["mode"]="fullSubtree";
    actionLogger.addAction("importSubscriptionFull", importData);

    QMessageBox::information(this, tr("Import"),
                             tr("Поддерево «%1» импортировано в раздел\n%2")
                             .arg(meta.title)
                             .arg(tr("imported/%1/%2").arg(meta.ownerName).arg(meta.title)));

    refreshPublicationState();
    return;
  }

  const SubscriptionRecord subscription=subscriptionRegistry.getSubscription(branchId);
  const QString baselineSnapshot=subscription.baselineSnapshot;
  qDebug() << "showSubscriptionChanges: baselineSnapshot present" << !baselineSnapshot.isEmpty();

  // Целевая ветка — локальная копия поддерева (корень в зоне imported/…).
  // Нужна заранее: корневая таблица копии — фолбэк маршрутизации
  // и остаточного diff
  QString targetBranchId=subscriptionRegistry.getSubscription(branchId)
                         .branchesMap.value(meta.branchId);
  qDebug() << "showSubscriptionChanges: targetBranchId" << targetBranchId;

  if(targetBranchId.isEmpty())
  {
    QMessageBox::warning(this, tr("Import"),
                         tr("Локальная копия публикации не найдена."));
    return;
  }

  TreeItem *targetItem=knowTreeModel->getItemById(targetBranchId);
  if(!targetItem)
  {
    QMessageBox::warning(this, tr("Import"),
                         tr("Ветка-приёмник не найдена в дереве."));
    return;
  }

  if(targetItem->getField("crypt")=="1" && globalParameters.getCryptKey().isEmpty())
  {
    QMessageBox::information(this, tr("Import"),
                             tr("Ветка-приёмник зашифрована. Откройте её "
                                "паролем перед импортом."));
    return;
  }

  RecordTableData *table=targetItem->recordtableGetTableData();

  // Остаточный diff: свежие изменения минус уже отражённые в локальной
  // копии минус применённые удаления. После полного импорта список пуст,
  // после частичного — только остаток
  QList<DiffChange> changes=engine.pendingChanges(&subscriptionRegistry, knowTreeModel,
                                                  baselineSnapshot, table);
  qDebug() << "showSubscriptionChanges: pending changes count" << changes.size();

  if(changes.isEmpty())
  {
    QMessageBox::information(this, tr("Changes"),
                             tr("Новых изменений в публикации нет."));
    return;
  }

  // Тексты для построчного diff: sharedRecordId -> (локальный, владельца).
  // Только для изменённых текстов; картинки в тексте — плейсхолдерами,
  // вложения — списками в диалоге (бинарное содержимое не сравнивается).
  // Плюс базовые каталоги для рендера «Было / Стало» (картинки грузятся
  // относительно каталогов записей)
  QMap<QString, QPair<QString, QString>> recordTexts;
  QMap<QString, QPair<QString, QString>> recordBaseDirs;
  for(const DiffChange &change : changes)
  {
    if(change.type!=DiffChange::RecordUpdate || !change.textChanged)
      continue;
    const QString headText=engine.headRecordText(change.recordId);
    if(headText.isEmpty())
      continue;
    const QString localText=engine.localRecordTextForChange(&subscriptionRegistry, knowTreeModel,
                                                            change, table);
    recordTexts.insert(change.recordId, QPair<QString, QString>(localText, headText));
    recordBaseDirs.insert(change.recordId,
                          engine.recordBaseDirsForChange(&subscriptionRegistry, knowTreeModel,
                                                         change, table));
  }

  // Диалог со списком изменений и выбором действия (+ вкладка истории)
  ChangeViewDialog dialog(meta.title, meta.ownerName, changes, this, publicationDir,
                          recordTexts, recordBaseDirs);
  dialog.setWindowTitle(tr("Changes: %1").arg(meta.title));

  if(dialog.exec()!=QDialog::Accepted)
  {
    qDebug() << "showSubscriptionChanges: dialog rejected";
    return;
  }
  qDebug() << "showSubscriptionChanges: dialog action"
           << (int)dialog.action()
           << "selected indices count" << dialog.selectedChangeIndices().size();

  // «Отметить просмотренным» — только сдвигаем базовую точку
  if(dialog.action()==ChangeViewDialog::Action::MarkViewed)
  {
    subscriptionRegistry.setBaselineState(branchId, engine.buildBaselineSnapshot(),
                                          meta.publishVersion);
    refreshPublicationState();
    return;
  }

  // Таблица-приёмник резолвится движком на каждое изменение
  // (SubscriptionImportEngine::resolveTable): где запись реально живёт
  // либо таблица своей ветки-копии, корень — фолбэк

  // Подсчитывается количество изменений, которые можно применить
  // (записи + структура веток)
  int applicableCount=0;
  for(const DiffChange &change : changes)
  {
    if(change.type==DiffChange::RecordAdd
       || change.type==DiffChange::RecordUpdate
       || change.type==DiffChange::RecordDelete
       || change.type==DiffChange::BranchAdd
       || change.type==DiffChange::BranchRename
       || change.type==DiffChange::BranchMove
       || change.type==DiffChange::BranchDelete)
      applicableCount++;
  }

  // Применение выбранных изменений
  const QList<int> selectedIndices=dialog.selectedChangeIndices();
  int appliedCount=0;

  qDebug() << "showSubscriptionChanges: targetBranchId" << targetBranchId;

  for(int idx : selectedIndices)
  {
    if(idx<0 || idx>=changes.size())
      continue;
const DiffChange &change=changes.at(idx);

    // Защита от частичной доставки в момент импорта: добавляем/обновляем
    // запись, только если её файлы совпадают с манифестом версии.
    // Удаления под гард не попадают: удалённой записи в публикации нет
    // по определению (isRecordComplete для неё всегда false — иначе удаление
    // молча пропускалось бы вечно). Целостность версии уже проверена выше
    if(change.type==DiffChange::RecordAdd
       || change.type==DiffChange::RecordUpdate)
    {
      if(!engine.isRecordComplete(change.recordId))
      {
        qWarning() << "showSubscriptionChanges: record not complete (data still in transit), skip"
                   << change.recordId;
        continue;
      }
    }

    qDebug() << "showSubscriptionChanges: applying change" << idx
             << "type" << (int)change.type << "id" << change.recordId;

    QString errorText;
    QString localRecordId;

    SubscriptionImportEngine::AppStatus status=
        engine.applyChange(&subscriptionRegistry,
                           engine.resolveTable(&subscriptionRegistry, knowTreeModel, change, table),
                           change, &errorText, &localRecordId,
                           knowTreeModel);

    qDebug() << "showSubscriptionChanges: status" << (int)status
             << "error" << errorText << "localRecordId" << localRecordId;

    if(status==SubscriptionImportEngine::AppStatus::NeedsDecision)
    {
      // Конфликт: локальная правка против версии владельца
      QMessageBox conflictBox(QMessageBox::Question, tr("Import conflict"),
                              tr("Запись «%1» была изменена локально.\n"
                                 "Импортировать версию владельца?").arg(change.title),
                              QMessageBox::NoButton, this);

      QPushButton *replaceButton=conflictBox.addButton(tr("Заменить версией владельца"),
                                                       QMessageBox::AcceptRole);
      Q_UNUSED(replaceButton)
      QPushButton *keepButton=conflictBox.addButton(tr("Сохранить обе"),
                                                    QMessageBox::ActionRole);
      QPushButton *skipButton=conflictBox.addButton(tr("Пропустить"),
                                                    QMessageBox::DestructiveRole);
      conflictBox.exec();

      QAbstractButton *clickedButton=conflictBox.clickedButton();
      if(clickedButton==skipButton)
        continue;

      const SubscriptionImportEngine::ConflictDecision decision=
          (clickedButton==keepButton)
              ? SubscriptionImportEngine::ConflictDecision::DecisionKeepBoth
              : SubscriptionImportEngine::ConflictDecision::DecisionReplace;

      status=engine.applyChangeWithDecision(&subscriptionRegistry,
                                            engine.resolveTable(&subscriptionRegistry, knowTreeModel,
                                                                change, table),
                                            change,
                                            decision, &errorText, &localRecordId,
                                            knowTreeModel);

      QMap<QString, QString> conflictData;
      conflictData["branchId"]=branchId;
      conflictData["recordId"]=change.recordId;
      conflictData["recordName"]=change.title;
      conflictData["decision"]=(decision==SubscriptionImportEngine::ConflictDecision::DecisionKeepBoth)
                                   ? "keepBoth" : "replace";
      actionLogger.addAction("importConflictSolver", conflictData);
    }

    if(status==SubscriptionImportEngine::AppStatus::AppliedOk)
    {
      appliedCount++;
    }
    else if(status==SubscriptionImportEngine::AppStatus::Failed && !errorText.isEmpty())
    {
      QMessageBox::warning(this, tr("Import"), errorText);
      break;
    }
  }

  // Если что-то применилось — БД сохраняется и обновляется на экране
  qDebug() << "showSubscriptionChanges: appliedCount" << appliedCount;
  if(appliedCount>0)
  {
    saveKnowTree();

    // Переименование веток идёт напрямую через setField без уведомлений
    // модели — принудительно перерисовываем дерево
    knowTreeView->viewport()->update();

    const QModelIndex targetIndex=knowTreeModel->getIndexByItem(targetItem);
    if(targetIndex.isValid())
    {
      knowTreeView->setCurrentIndex(targetIndex);
      updateBranchOnScreen(targetIndex);
    }
  }

  // Базовая точка продвигается только если применены ВСЕ применимые изменения
  // (иначе непросмотренное остаётся видимым)
  if(applicableCount>0 && appliedCount==applicableCount)
    subscriptionRegistry.setBaselineState(branchId, engine.buildBaselineSnapshot(),
                                          meta.publishVersion);

  if(appliedCount>0)
  {
    QMap<QString, QString> importData;
    importData["branchId"]=branchId;
    importData["branchName"]=meta.title;
    importData["publicationPath"]=publicationDir;
    importData["mode"]=dialog.importModeName();
    importData["appliedCount"]=QString::number(appliedCount);
    importData["totalCount"]=QString::number(applicableCount);
    actionLogger.addAction("importSubscriptionChanges", importData);

    QMessageBox::information(this, tr("Import"),
                             tr("Импортировано изменений: %1 из %2.")
                             .arg(appliedCount).arg(applicableCount));
  }

  refreshPublicationState();
}


// Обновление состояния панели подписок и бейджей публикаций


void TreeScreen::refreshPublicationState(void)
{
  qDebug() << "TreeScreen::refreshPublicationState, shared dir:" << teamProfile.getSharedDir();
  updatePublishedBadges();

  if(subscriptionPanel)
    subscriptionPanel->refresh();

  this->checkJournalRecovery();
  this->checkJournalSize();
}


// Проверка доступности восстановления sync/.
// Порядок: сначала local-авторитет (без git, только свои публикации),
// затем единый журнал. Вызывается при каждом обновлении состояния подписок;
// предложение показывается один раз за сессию


void TreeScreen::checkJournalRecovery(void)
{
  if(recoveryPromptShown)
    return;

  // Первично — восстановление из local/ (без git)
  if(sharedDirWatcher.isLocalRestoreAvailable())
  {
    recoveryPromptShown=true;

    QMessageBox box(this);
    box.setWindowTitle(tr("Restore from local"));
    box.setIcon(QMessageBox::Warning);
    box.setText(tr("Область sync/ пуста или содержит неполные данные.\n"
                   "Авторитет local/ содержит целые публикации владельца.\n\n"
                   "Восстановить зеркало sync/ из local/?"));

    QPushButton *restoreButton=box.addButton(tr("Restore"), QMessageBox::YesRole);
    box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.exec();

    if(box.clickedButton()==restoreButton && subscriptionPanel)
      subscriptionPanel->restoreFromLocal();

    return;
  }

  if(!sharedDirWatcher.isRecoveryAvailable())
    return;

  recoveryPromptShown=true;

  const int snapshotCount=sharedDirWatcher.journalSnapshotCount();

  QMessageBox box(this);
  box.setWindowTitle(tr("Restore from journal"));
  box.setIcon(QMessageBox::Warning);
  box.setText(tr("Область sync/ пуста или содержит неполные данные.\n"
                 "Единый журнал содержит %1 снимков(а) с последним известным "
                 "состоянием публикаций.\n\n"
                 "Восстановить публикации из журнала?")
              .arg(snapshotCount));

  QPushButton *restoreButton=box.addButton(tr("Restore"), QMessageBox::YesRole);
  box.addButton(tr("Later"), QMessageBox::RejectRole);
  box.exec();

  if(box.clickedButton()==restoreButton && subscriptionPanel)
  {
    subscriptionPanel->restoreFromJournal(true, snapshotCount);
  }
}


// Порог снимков журнала, после которого предлагается архивирование истории.
// Тихого автосжатия нет осознанно: удаление истории необратимо и убивает
// глубину восстановления — решение всегда за пользователем
const int JournalArchiveSuggestSnapshots=1000;


// Предупреждение о разросшемся журнале обмена (один раз за сессию).
// Считается только число снимков (дешёво); размер каталога — только если
// порог превышен, для текста диалога


void TreeScreen::checkJournalSize(void)
{
  if(journalSizePromptShown)
    return;

  const int snapshotCount=sharedDirWatcher.journalSnapshotCount();
  if(snapshotCount<JournalArchiveSuggestSnapshots)
    return;

  journalSizePromptShown=true;

  const quint64 journalBytes=sharedDirWatcher.journalDirSizeBytes();
  const double journalMb=static_cast<double>(journalBytes)/(1024.0*1024.0);

  QMessageBox box(this);
  box.setWindowTitle(tr("Journal size"));
  box.setIcon(QMessageBox::Information);
  box.setText(tr("Журнал обмена разросся: %1 снимков (%2 МБ).\n"
                 "Старые снимки нужны только для восстановления давно "
                 "удалённого. Архивирование оставит только текущее состояние "
                 "(необратимо). Архивировать сейчас?")
              .arg(snapshotCount)
              .arg(journalMb, 0, 'f', 1));

  QPushButton *archiveButton=box.addButton(tr("Archive now"), QMessageBox::YesRole);
  box.addButton(tr("Later"), QMessageBox::RejectRole);
  box.exec();

  if(box.clickedButton()==archiveButton && subscriptionPanel)
    subscriptionPanel->archiveJournal(snapshotCount);
}


// Обновление набора ключей опубликованных веток и перерисовка бейджей


void TreeScreen::updatePublishedBadges(void)
{
  publishedBranchKeys.clear();

  QString sharedDir=teamProfile.getSharedDir();
  if(!sharedDir.isEmpty())
    publishedBranchKeys=BranchPublisher::listPublishedBranchKeys(sharedDir);

  if(publishedBadgeDelegate)
    publishedBadgeDelegate->setPublishedKeys(publishedBranchKeys);

  if(knowTreeView)
    knowTreeView->viewport()->update();
}


void TreeScreen::autoUpdatePublishedBranches(void)
{
  const QString sharedDir=teamProfile.getSharedDir();
  const QString teamId=teamProfile.getTeamId();

  // Без настроенного профиля свои публикации не определить
  if(sharedDir.isEmpty() || teamId.isEmpty())
    return;

  // Публикации обновляются только после того, как база хотя бы раз
  // загрузилась (защита от срабатывания на этапе старта приложения)
  if(lastKnowTreeModifyDateTime.isValid()==false)
    return;

  const QList<PublicationMeta> publications=BranchPublisher::listPublications(sharedDir);

  for(const PublicationMeta &meta : publications)
  {
    // Только собственные публикации (по машинному ключу владельца)
    if(meta.branchId.isEmpty() || meta.ownerId!=teamId)
      continue;

    TreeItem *item=knowTreeModel->getItemById(meta.branchId);
    if(!item)
      continue; // Ветка удалена из базы — публикация не трогается

    const BranchPublisher::Result result=BranchPublisher::publishBranch(
        knowTreeModel, item, teamProfile, BranchPublisher::Operation::Update);

    qDebug() << "autoUpdatePublishedBranches: branchId" << meta.branchId
             << "title" << meta.title
             << "success" << result.success
             << "unchanged" << result.unchanged
             << "publishVersion" << result.publishVersion
             << "error" << result.errorMessage;
  }

  // Собственные публикации приложением актуализированы — журнал фиксирует
  // новое состояние немедленно (после авто-обновления восстановление не нужно)
  sharedDirWatcher.requestImmediateSnapshot();
}


// Планирование отложенного автообновления публикаций после сохранения
// текста записей (правки текста дерево не пересохраняют, поэтому прямой
// вызов autoUpdatePublishedBranches из saveKnowTree их не покрывает)


void TreeScreen::schedulePublicationsAutoUpdate(void)
{
  if(publicationAutoUpdateTimer)
    publicationAutoUpdateTimer->start();
}


// Срабатывание отложенного автообновления публикаций


void TreeScreen::onPublicationAutoUpdateTimeout(void)
{
  autoUpdatePublishedBranches();
}


// Перечитывание дерева веток с диска
// Метод возвращает true, если обнаружено что данные были изменены и перечитаны

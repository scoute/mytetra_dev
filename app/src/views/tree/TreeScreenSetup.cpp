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

 // Кнопка синхронизации переехала на тулбар записей (после предпросмотра
 // печати): действие создается и живет в RecordTableScreen

 // Панель избранного между кнопками веток и деревом. Создается здесь,
 // а не в MainWindow, чтобы лежать ровно под тулбаром. Скрыта пока
 // пусто или фича выключена: дизайн не ломается
 favoritesPanel=new FavoritesPanel(m_favorites, this);
 favoritesPanel->setObjectName("favoritesPanel");
 favoritesPanel->hide();

 // Строка фильтра над деревом: тот же приём что в опросе клипа
 treeFilterEdit=new QLineEdit(this);
 treeFilterEdit->setObjectName(QStringLiteral("treeFilterEdit"));
 treeFilterEdit->setPlaceholderText(tr("Filter branches..."));
 treeFilterEdit->setClearButtonEnabled(true);
 treeFilterEdit->setVisible(AppContext::instance().appConfig()->uiSection()->get_treeFilterVisible());
}


void TreeScreen::setupShortcuts(void)
{
    qDebug() << "Setup shortcut for" << this->metaObject()->className();

    // Прямой текст кнопки поиска: шортката у нее нет,
    // поэтому освежается здесь же вместе со всеми
    actionList["findInBase"]->setText(tr("Find in base"));
    actionList["findInBase"]->setStatusTip(tr("Find in base"));

    AppContext::instance().shortcutManager()->initAction("tree-expandAllSubbranch", actionList["expandAllSubbranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-collapseAllSubbranch", actionList["collapseAllSubbranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-moveUpBranch", actionList["moveUpBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-moveDownBranch", actionList["moveDownBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-insSubbranch", actionList["insSubbranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-insBranch", actionList["insBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-editBranch", actionList["editBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-delBranch", actionList["delBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-cutBranch", actionList["cutBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-copyBranch", actionList["copyBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-pasteBranch", actionList["pasteBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-pasteSubbranch", actionList["pasteSubbranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-encryptBranch", actionList["encryptBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-decryptBranch", actionList["decryptBranch"] );
    AppContext::instance().shortcutManager()->initAction("tree-setIcon", actionList["setIcon"] );
}

// Прямых текстов кроме кнопки поиска нет: все освежает setupShortcuts
// по сигналу updateWidgetShortcut. Метод оставлен для единого интерфейса


// Прямых текстов кроме кнопки поиска нет: все освежает setupShortcuts
// по сигналу updateWidgetShortcut. Метод оставлен для единого интерфейса
void TreeScreen::retranslateUi(void)
{
    setupShortcuts();

    treeFilterEdit->setPlaceholderText(tr("Filter branches..."));
}

// Строка фильтра над деревом: показать/скрыть (тумблер в меню Вид).
// Скрытие сбрасывает текст и возвращает все ветки


// Строка фильтра над деревом: показать/скрыть (тумблер в меню Вид).
// Скрытие сбрасывает текст и возвращает все ветки
void TreeScreen::setTreeFilterVisible(bool visible)
{
  treeFilterEdit->setVisible(visible);

  if(!visible)
    treeFilterEdit->clear();
}


void TreeScreen::applyTreeFilter(const QString &text)
{
  if(knowTreeModel==nullptr)
    return;

  QAbstractItemDelegate *rawDelegate=knowTreeView->itemDelegate();
  qDebug() << "DBG delegate ptr" << rawDelegate << "view" << knowTreeView;
  if(KnowTreeDelegate *delegate=qobject_cast<KnowTreeDelegate *>(rawDelegate))
    delegate->setFilterText(text);

  if(text.isEmpty())
  {
    showAllBranches(QModelIndex());
    return;
  }

  filterBranches(QModelIndex(), text);
}

// Совпадение или видимый потомок — показать, предков совпадений
// раскрыть (иначе находки не видно). Индексы исходной модели целы:
// фильтр режет скрытием строк, а не прокси-моделью


// Совпадение или видимый потомок — показать, предков совпадений
// раскрыть (иначе находки не видно). Индексы исходной модели целы:
// фильтр режет скрытием строк, а не прокси-моделью
bool TreeScreen::filterBranches(const QModelIndex &parent, const QString &text)
{
  KnowTreeModel *model=knowTreeModel;
  bool anyVisible=false;

  for(int row=0; row<model->rowCount(parent); ++row)
  {
    const QModelIndex index=model->index(row, 0, parent);
    TreeItem *item=model->getItem(index);

    const bool selfMatch=item!=nullptr &&
        item->getField(QStringLiteral("name")).contains(text, Qt::CaseInsensitive);
    const bool childVisible=filterBranches(index, text);
    const bool visible=selfMatch || childVisible;

    knowTreeView->setRowHidden(row, parent, !visible);

    if(childVisible)
      knowTreeView->setExpanded(index, true);

    if(visible)
      anyVisible=true;
  }

  return anyVisible;
}


void TreeScreen::showAllBranches(const QModelIndex &parent)
{
  KnowTreeModel *model=knowTreeModel;
  if(model==nullptr)
    return;

  for(int row=0; row<model->rowCount(parent); ++row)
  {
    knowTreeView->setRowHidden(row, parent, false);
    showAllBranches(model->index(row, 0, parent));
  }
}

// Прыжок извне при активном фильтре: цель может быть скрыта.
// Скрыта — фильтр сбросить (строка при этом остается на месте),
// видна — ничего не трогать. Невалидный путь — тоже не трогать:
// дальше setTreePosition сам разберется


// Прыжок извне при активном фильтре: цель может быть скрыта.
// Скрыта — фильтр сбросить (строка при этом остается на месте),
// видна — ничего не трогать. Невалидный путь — тоже не трогать:
// дальше setTreePosition сам разберется
void TreeScreen::ensurePathVisible(const QStringList &path)
{
  if(treeFilterEdit->text().isEmpty() || knowTreeModel==nullptr)
    return;

  TreeItem *item=knowTreeModel->getItem(path);
  if(item==nullptr)
    return;

  const QModelIndex index=knowTreeModel->getIndexByItem(item);
  for(QModelIndex current=index; current.isValid(); current=current.parent())
    if(knowTreeView->isRowHidden(current.row(), current.parent()))
    {
      treeFilterEdit->clear();
      return;
    }
}

// Порядок кнопок тулбара веток по умолчанию. Объединение десктопного
// и мобильного наборов: лишнее режется режимом в rebuildToolbar.
// spring - невидимый распор (в дефолте стоит под мобильный),
// separator - разделитель, остальные - имена из actionList


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
  << "spring" << "findInBase";
}

// Режим кнопки: desktop, mobile или обе (пусто - обе)


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

 QStringList order=AppContext::instance().appConfig()->uiSection()->getTreeToolbarOrder();

 // Разовая миграция: кнопка синхронизации переехала с тулбара веток
 // на тулбар записей вместе со своим разделителем. Старый сохраненный
 // порядок еще содержит "syncro" - убираем его и разделитель перед ним,
 // затем один раз дописываем пару "separator, syncro" в порядок записей
 // после предпросмотра печати. Пустой порядок означает дефолт.
 const int syncroPos=order.indexOf("syncro");

 if(!order.isEmpty() && syncroPos>=0)
  {
   if(syncroPos>0 && order.at(syncroPos-1)=="separator")
    order.removeAt(syncroPos-1);

   order.removeAll("syncro");

   // После переноса не должно остаться двойных и хвостовых разделителей
   for(int i=order.size()-1; i>0; --i)
    if(order.at(i)=="separator" && order.at(i-1)=="separator")
     order.removeAt(i);

   if(!order.isEmpty() && order.last()=="separator")
    order.removeLast();

   AppContext::instance().appConfig()->uiSection()->setTreeToolbarOrder(order);

   QStringList recordOrder=AppContext::instance().appConfig()->uiSection()->getRecordToolbarOrder();

   if(!recordOrder.isEmpty() && !recordOrder.contains("syncro"))
    {
     const int previewPos=recordOrder.indexOf("printPreview");

     if(previewPos>=0)
      {
       int insertPos=previewPos+1;

       // Разделитель уже стоит сразу после предпросмотра - не задваиваем
       if(insertPos<recordOrder.size() && recordOrder.at(insertPos)=="separator")
        insertPos++;
       else
        recordOrder.insert(insertPos++, "separator");

       recordOrder.insert(insertPos, "syncro");
      }
     else
      {
       if(!recordOrder.isEmpty() && recordOrder.last()!="separator")
        recordOrder.append("separator");

       recordOrder.append("syncro");
      }

     AppContext::instance().appConfig()->uiSection()->setRecordToolbarOrder(recordOrder);
    }

   // Миграция должна пережить даже аварийное завершение сразу после старта
   AppContext::instance().appConfig()->sync();
  }

 if(order.isEmpty())
  order=defaultToolbarOrder();

 const bool isMobile=(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile");
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
 knowTreeModel = new KnowTreeModel(m_backlinks, m_dispatcher, this);

 // Установка заголовка
 // QStringList headers;
 // headers << tr("Info groups");
 // knowTreeModel->setHeaders(headers);

 // Загрузка данных
 knowTreeModel->initFromXML( AppContext::instance().appConfig()->storageSection()->get_tetradir()+"/mytetra.xml" );

 // Модель подключется к виду
 knowTreeView->setModel(knowTreeModel);

 // Первичная пересборка избранного после загрузки модели:
 // покажет панель если в базе уже есть звездочки
 if(favoritesPanel!=nullptr)
   favoritesPanel->refreshFavorites();
}

// Контекстное меню тулбара веток: единственный пункт открывает
// диалог настройки кнопок. Показывается по ПКМ в любом месте тулбара:
// у кнопок своего меню нет, конфликтов нет


// Контекстное меню тулбара веток: единственный пункт открывает
// диалог настройки кнопок. Показывается по ПКМ в любом месте тулбара:
// у кнопок своего меню нет, конфликтов нет
void TreeScreen::onToolbarContextMenu(const QPoint &pos)
{
 QMenu menu(this);
 QAction *setupAction=menu.addAction(tr("Customize toolbar..."));

 if(menu.exec(toolsLine->mapToGlobal(pos))==setupAction)
  {
   MainToolbarSettingsScreen settingsScreen(this);
   settingsScreen.exec();
  }
}

// Открытие контекстного меню в дереве разделов


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
     (cryptFlag=="1" && AppContext::instance().globalParameters()->getCryptKey().length()>0))
   {

    // Если во внутреннем буфере обмена есть ветки
    // или есть вырезанная ветка, ожидающая вставки-перемещения,
    // соответсвующие пункты становятся активными
    bool isBranch=false;
    const QMimeData *mimeData = AppContext::instance().internalClipboard()->mimeData();
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

 // Фильтр дерева: живой поиск по подстроке имени
 connect(treeFilterEdit, &QLineEdit::textChanged,
         this,            &TreeScreen::applyTreeFilter);

 // Контекстное меню тулбара: только на пустом месте, там единственный
 // пункт настройки порядка и видимости кнопок
 toolsLine->setContextMenuPolicy(Qt::CustomContextMenu);
 connect(toolsLine, &QToolBar::customContextMenuRequested,
         this,      &TreeScreen::onToolbarContextMenu);

 // Соединение сигнал-слот чтобы показать контекстное меню по долгому нажатию
 connect(knowTreeView, &KnowTreeView::tapAndHoldGestureFinished,
         this,         &TreeScreen::onCustomContextMenuRequested);

 // Сигнал что ветка выбрана мышкой или стрелками на клавиатуре
 // через selection-модель. Реакция происходит на перемещение курсора по дереву
 if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="desktop")
 {
   connect(knowTreeView->selectionModel(), &QItemSelectionModel::currentRowChanged,
           this,                           &TreeScreen::onKnowtreeClicked);
 }
 
 if(AppContext::instance().appConfig()->uiSection()->getInterfaceMode()=="mobile")
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
 connect(AppContext::instance().shortcutManager(), &ShortcutManager::updateWidgetShortcut, this, &TreeScreen::setupShortcuts);
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
 // ее нельзя было потерять drag-ом, дерево забирает остаток.
 // Дерево в контейнере со строкой фильтра сверху
 QSplitter *favoritesSplitter=new QSplitter(Qt::Vertical, this);
 favoritesSplitter->setObjectName("favoritesSplitter");
 favoritesSplitter->addWidget(favoritesPanel);

 QWidget *treeContainer=new QWidget(this);
 QVBoxLayout *treeContainerLayout=new QVBoxLayout(treeContainer);
 treeContainerLayout->setContentsMargins(0, 0, 0, 0);
 treeContainerLayout->setSpacing(0);
 treeContainerLayout->addWidget(treeFilterEdit);
 treeContainerLayout->addWidget(knowTreeView);

 favoritesSplitter->addWidget(treeContainer);
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


QAction *TreeScreen::toolAction(const QString &command)
{
 if(!actionList.contains(command))
  return nullptr;

 return actionList[command];
}

// Метод, следящий, не стало ли дерево содержать только незашифрованные записи
// Если в дереве нет шифрования, задается вопрос, нужно ли сбросить пароль

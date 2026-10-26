#include <QObject>
#include <QMimeData>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolBar>
#include <QApplication>
#include <QMenu>

#include "RecordTableScreen.h"
#include "RecordTableView.h"

#include "views/mainWindow/MainWindow.h"
#include "views/record/MetaEditor.h"
#include "views/toolbarSettings/MainToolbarSettingsScreen.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/WindowSwitcher.h"
#include "libraries/GlobalParameters.h"
#include "libraries/FixedParameters.h"
#include "libraries/ShortcutManager.h"
#include "libraries/InternalClipboard.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/ActionHelper.h"
#include "views/tree/KnowTreeView.h"
#include "models/tree/KnowTreeModel.h"
#include "models/recordTable/Record.h"
#include "controllers/recordTable/RecordTableController.h"


extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;
extern ShortcutManager shortcutManager;
extern InternalClipboard *internalClipboard;


// Виджет, который отображает список записей в ветке
// c кнопками управления

RecordTableScreen::RecordTableScreen(QWidget *parent) : QWidget(parent)
{
  setupActions();

  // Инициализируется контроллер списка записей
  recordTableController=new RecordTableController(this);
  recordTableController->setObjectName("recordTableController");
  recordTableController->init();

  setupUI();
  setupShortcuts();
  setupSignals();
  assembly();
}


RecordTableScreen::~RecordTableScreen()
{

}


// Настройка возможных действий
void RecordTableScreen::setupActions(void)
{
 // Добавление записи
 actionAddNewToEnd = new QAction(this);
 actionAddNewToEnd->setIcon(QIcon(":/resource/pic/note_add.svg"));

 // Добавление записи до
 actionAddNewBefore = new QAction(this);

 // Добавление записи после
 actionAddNewAfter = new QAction(this);

 // Редактирование свойств записи
 actionEditField = new QAction(this);
 actionEditField->setIcon(QIcon(":/resource/pic/note_edit.svg"));

 // Блокировка записи
 actionBlock = new QAction(this);
 actionBlock->setIcon(QIcon(":/resource/pic/note_block.svg"));

 // Звездочка избранного
 actionFavorite = new QAction(this);
 actionFavorite->setIcon(QIcon(":/resource/pic/note_favorite.svg"));

 // Удаление записи
 actionDelete = new QAction(this);
 actionDelete->setIcon(QIcon(":/resource/pic/note_delete.svg"));

 // Удаление записи с копированием в буфер обмена
 actionCut = new QAction(this);
 actionCut->setIcon(QIcon(":/resource/pic/cb_cut.svg"));

 // Копирование записи (записей) в буфер обмена
 actionCopy = new QAction(this);
 actionCopy->setIcon(QIcon(":/resource/pic/cb_copy.svg"));

 // Вставка записи из буфера обмена
 actionPaste = new QAction(this);
 actionPaste->setIcon(QIcon(":/resource/pic/cb_paste.svg"));

 // Настройка внешнего вида таблицы конечных записей (горячая кнопка не требуется)
 actionSettings = new QAction(tr("View settings"), this);
 actionSettings->setStatusTip(tr("Setup table view settins"));
 actionSettings->setIcon(QIcon(":/resource/pic/edit_settings.svg"));

 // Перемещение записи вверх
 actionMoveUp = new QAction(this);
 actionMoveUp->setIcon(QIcon(":/resource/pic/move_up.svg"));

 // Перемещение записи вниз
 actionMoveDn=new QAction(this);
 actionMoveDn->setIcon(QIcon(":/resource/pic/move_dn.svg"));

 // Поиск по базе (клик связывается с действием в MainWindow)
 actionFindInBase=new QAction(shortcutManager.getDescription("misc-findInBase"), this);
 actionFindInBase->setIcon(QIcon(":/resource/pic/find_in_base.svg"));

 // Перемещение по истории посещаемых записей назад
 actionWalkHistoryPrevious=new QAction(tr("Previous viewing note"), this);
 actionWalkHistoryPrevious->setIcon(QIcon(":/resource/pic/walk_history_previous.svg"));

 // Перемещение по истории посещаемых записей вперед
 actionWalkHistoryNext=new QAction(tr("Next viewing note"), this);
 actionWalkHistoryNext->setIcon(QIcon(":/resource/pic/walk_history_next.svg"));

 // Кнопка Назад (Back) в мобильном интерфейсе
 actionBack=new QAction(tr("Back to item tree"), this);
 actionBack->setStatusTip(tr("Back to item tree"));
 actionBack->setIcon(QIcon(":/resource/pic/mobile_back.svg"));

 // Действия по сортировке (горячая кнопка не требуется)
 actionSort = new QAction(tr("Toggle sorting"), this);
 actionSort->setStatusTip(tr("Enable/disable sorting by column"));
 actionSort->setIcon(QIcon(":/resource/pic/sort.svg"));

 // Кнопка вызова печати таблицы конечных записей
 actionPrint = new QAction(tr("Print table"), this);
 actionPrint->setStatusTip(tr("Print current notes table"));
 actionPrint->setIcon(QIcon(":/resource/pic/print_table.svg"));

 // Кнопка предпросмотра печати текущей записи (клик связывается
 // с действием в MainWindow). Своя иконка-принтер: глаз предпросмотра
 // вложений здесь смотрелся неуместно
 actionPrintPreview = new QAction(tr("Print preview"), this);
 actionPrintPreview->setStatusTip(tr("Preview current note before printing"));
 actionPrintPreview->setIcon(QIcon(":/resource/pic/print_preview.svg"));

 // Синхронизация базы. Операция уровня всей базы, поэтому стоит
 // в конце первой линии тулбара записей, сразу после предпросмотра
 actionSyncro = new QAction(tr("Synchronization"), this);
 actionSyncro->setStatusTip(tr("Run synchronization"));
 actionSyncro->setIcon(QIcon(":/resource/pic/synchronization.svg"));

 // Кнопка копирования ссылки на запись
 actionCopyRecordReference = new QAction(tr("Copy note reference"), this);
 actionCopyRecordReference->setStatusTip(tr("Copy note reference to clipboard"));
 actionCopyRecordReference->setIcon(QIcon(":/resource/pic/note_reference.svg"));

 // Кнопка копирования готовой ссылки с заголовком: в буфер кладётся
 // HTML-якорь, вставка в другую запись даёт сразу именованную ссылку
 actionCopyRecordLink = new QAction(tr("Copy link with title"), this);
 actionCopyRecordLink->setStatusTip(tr("Copy titled link to clipboard for pasting into another note"));
 actionCopyRecordLink->setIcon(QIcon(":/resource/pic/note_reference.svg"));

 // Кнопка истории изменений записи
 actionChangeHistory = new QAction(tr("Change history"), this);
 actionChangeHistory->setStatusTip(tr("Show change history with diff and rollback"));
 actionChangeHistory->setIcon(QIcon(":/resource/pic/history.svg"));

 // Кнопка переключения режима одинарного выбора и мультивыбора (горячая кнопка не требуется)
 actionSwitchSelectionMode = new QAction(tr("Switch select/multiselect"), this);
 actionSwitchSelectionMode->setStatusTip(tr("Switch note selection mode (Notice: if multiselect is on, drag-and-drop is disabled)"));
 actionSwitchSelectionMode->setIcon(QIcon(":/resource/pic/switch_note_selection_mode.svg"));

 // Сразу после создания все действия запрещены
 disableAllActions();
}


// Порядок кнопок первой линии по умолчанию. Объединение десктопного
// и мобильного наборов: лишнее режется режимом в rebuildToolbars.
// Скрытые shortcut-only кнопки (addNewBefore, addNewAfter, block)
// в список не входят: регистрируются всегда отдельно
QStringList RecordTableScreen::defaultToolbarOrder(void)
{
 return QStringList()
  << "back" << "separator"
  << "addNewToEnd"
  << "editField" << "delete"
  << "separator"
  << "cut" << "copy" << "paste"
  << "separator"
  << "moveUp" << "moveDn"
  << "separator"
  << "printPreview"
  << "separator"
  << "syncro";
}


// Порядок кнопок второй линии по умолчанию.
// Внешние кнопки (переключатели панелей) сюда не входят:
// их докладывает addExtraToolAction() всегда в конец
QStringList RecordTableScreen::defaultExtraToolbarOrder(void)
{
 return QStringList()
  << "walkHistoryPrevious" << "walkHistoryNext"
  << "findInBase";
}


// Режим кнопки: desktop, mobile или обе (пусто - обе)
QString RecordTableScreen::toolbarItemMode(const QString &command)
{
 if(command=="back")
  return QString("mobile");

 if(command=="editField" || command=="delete" ||
    command=="walkHistoryPrevious" || command=="walkHistoryNext")
  return QString("desktop");

 return QString();
}


// Скрытые shortcut-only кнопки первой линии: регистрируются всегда
// нулевыми кнопками, как раньше, в диалог настройки не выносятся
QSet<QString> RecordTableScreen::hiddenToolbarItems(void)
{
 return QSet<QString>() << "addNewBefore" << "addNewAfter" << "block";
}


// Действие тулбаров по имени для построения из списков
QAction *RecordTableScreen::toolAction(const QString &command)
{
 if(command=="addNewToEnd") return actionAddNewToEnd;
 if(command=="addNewBefore") return actionAddNewBefore;
 if(command=="addNewAfter") return actionAddNewAfter;
 if(command=="editField") return actionEditField;
 if(command=="block") return actionBlock;
 if(command=="delete") return actionDelete;
 if(command=="cut") return actionCut;
 if(command=="copy") return actionCopy;
 if(command=="paste") return actionPaste;
 if(command=="settings") return actionSettings;
 if(command=="back") return actionBack;
 if(command=="findInBase") return actionFindInBase;
 if(command=="sort") return actionSort;
 if(command=="print") return actionPrint;
 if(command=="printPreview") return actionPrintPreview;
 if(command=="syncro") return actionSyncro;
 if(command=="copyRecordReference") return actionCopyRecordReference;
 if(command=="changeHistory") return actionChangeHistory;
 if(command=="switchSelectionMode") return actionSwitchSelectionMode;
 if(command=="moveUp") return actionMoveUp;
 if(command=="moveDn") return actionMoveDn;
 if(command=="walkHistoryPrevious") return actionWalkHistoryPrevious;
 if(command=="walkHistoryNext") return actionWalkHistoryNext;

 return nullptr;
}


// Построение одной линии тулбара из списка. Ведущие и задвоенные
// разделители давятся; неизвестные имена пропускаются с предупреждением
void RecordTableScreen::buildToolbarLine(QToolBar *line,
                                         const QStringList &order,
                                         bool isMobile)
{
 bool lastSeparator=true; // Ведущие разделители давятся

 foreach(const QString &command, order)
  {
   if(command=="separator")
    {
     if(!lastSeparator)
      {
       line->addSeparator();
       lastSeparator=true;
      }

     continue;
    }

   if(command=="spring")
    {
     QWidget *empty=new QWidget(this);
     empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
     line->addWidget(empty);
     toolbarSprings.append(empty);
     lastSeparator=false;

     continue;
    }

   QAction *action=toolAction(command);

   if(action==nullptr)
    {
     qWarning() << "RecordTableScreen: skip unknown toolbar command" << command;
     continue;
    }

   const QString mode=toolbarItemMode(command);

   if(mode=="desktop" && isMobile)
    continue;

   if(mode=="mobile" && !isMobile)
    continue;

   insertActionAsButton(line, action, !hiddenToolbarItems().contains(command));
   lastSeparator=false;
  }
}


// Перестроение обоих тулбаров из списков конфига (или дефолта).
// Вызывается при старте и вживую из диалога настроек
void RecordTableScreen::rebuildToolbars(void)
{
 // Пружины прошлого построения: QToolBar::clear() виджеты не удаляет
 qDeleteAll(toolbarSprings);
 toolbarSprings.clear();

 toolsLine->clear();
 extraToolsLine->clear();

 const bool isMobile=(mytetraConfig.getInterfaceMode()=="mobile");

 QStringList order=mytetraConfig.getRecordToolbarOrder();

 if(order.isEmpty())
  order=defaultToolbarOrder();

 buildToolbarLine(toolsLine, order, isMobile);

 // Скрытые shortcut-only кнопки регистрируются всегда нулевым размером
 // (в списки не входят, в диалог не выносятся). Порядок среди невидимых
 // не важен, поэтому пачкой в конец
 foreach(const QString &command, hiddenToolbarItems())
  {
   QAction *action=toolAction(command);

   if(action!=nullptr)
    insertActionAsButton(toolsLine, action, false);
  }

 QStringList extraOrder=mytetraConfig.getRecordExtraToolbarOrder();

 if(extraOrder.isEmpty())
  extraOrder=defaultExtraToolbarOrder();

 buildToolbarLine(extraToolsLine, extraOrder, isMobile);

 // Внешние кнопки (переключатели панелей) всегда в конце второй линии,
 // отделённые разделителем своей группой: их в диалоге настройки нет,
 // и без разделителя непонятно откуда они взялись
 if(!extraToolActions.isEmpty())
  {
   // Разделитель не задваивается с пользовательским в конце списка
   if(extraOrder.isEmpty() || extraOrder.last()!="separator")
    extraToolsLine->addSeparator();

   foreach(QAction *action, extraToolActions)
    insertActionAsButton(extraToolsLine, action);
  }
}


void RecordTableScreen::setupUI(void)
{
 toolsLine=new QToolBar(this);
 extraToolsLine=new QToolBar(this);

 // Первая линия тянется на всю свободную ширину ряда (вторая ужата
 // по кнопкам справа): пустого места лейаута не остаётся, и ПКМ
 // для настройки панели работает в любой точке ряда
 toolsLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

 /*
 QSize toolBarIconSize(16,16);
 toolsLine->setIconSize(toolBarIconSize);
 */

 // Порядок кнопок берётся из конфига (диалог настроек) либо дефолт.
 // Перестроение на лету - через rebuildToolbars()
 rebuildToolbars();

 treePathLabel=new QLabel(this);
 treePathLabel->setWordWrap(true);
 if(mytetraConfig.getInterfaceMode()=="desktop")
   treePathLabel->hide();
}


// Добавить кнопку во вторую линию инструментов (для внешних действий
// вроде переключателя панели тегов). Кнопки запоминаются чтобы пережить
// перестроение тулбара из настроек: rebuildToolbars() докладывает их в конец
void RecordTableScreen::addExtraToolAction(QAction *action)
{
 if(!extraToolActions.contains(action))
  extraToolActions.append(action);

 insertActionAsButton(extraToolsLine, action);
}


void RecordTableScreen::setupShortcuts(void)
{
    qDebug() << "Setup shortcut for" << this->metaObject()->className();

    // Добавление записи
    shortcutManager.initAction("note-addNewToEnd", actionAddNewToEnd);

    // Добавление записи до
    shortcutManager.initAction("note-addNewBefore", actionAddNewBefore);

    // Добавление записи после
    shortcutManager.initAction("note-addNewAfter", actionAddNewAfter);

    // Редактирование свойств записи
    shortcutManager.initAction("note-editField", actionEditField);

    // Блокировка/разблокировка записи
    shortcutManager.initAction("note-block", actionBlock);

    // Удаление записи (записей)
    shortcutManager.initAction("note-delete", actionDelete);

    // Удаление записи с копированием в буфер обмена
    shortcutManager.initAction("note-cut", actionCut);

    // Копирование записи (записей)
    shortcutManager.initAction("note-copy", actionCopy);

    // Вставка записи (записей)
    shortcutManager.initAction("note-paste", actionPaste);

    // Перемещение записи вверх
    shortcutManager.initAction("note-moveUp", actionMoveUp);

    // Перемещение записи вниз
    shortcutManager.initAction("note-moveDn", actionMoveDn);


    // Переход на предыдущую запись в истории
    shortcutManager.initAction("note-previousNote", actionWalkHistoryPrevious);

    // Переход на следующую запись в истории
    shortcutManager.initAction("note-nextNote", actionWalkHistoryNext);

    // Синхронизация базы (кнопка на первой линии, после предпросмотра)
    shortcutManager.initAction("misc-syncro", actionSyncro);

    // Поиск по базе: шорткат регистрируется здесь, чтобы работал
    // везде и в меню показывался один раз в своей колонке
    shortcutManager.initAction("misc-findInBase", actionFindInBase);
}


void RecordTableScreen::setupSignals(void)
{
    // Контекстные меню тулбаров: только на пустом месте, там единственный
    // пункт настройки порядка и видимости кнопок
    toolsLine->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(toolsLine, &QToolBar::customContextMenuRequested,
            this, &RecordTableScreen::onToolbarContextMenu);

    extraToolsLine->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(extraToolsLine, &QToolBar::customContextMenuRequested,
            this, &RecordTableScreen::onToolbarContextMenu);

    // Добавление записи
    connect(actionAddNewToEnd, &QAction::triggered, recordTableController, &RecordTableController::addNewToEndContext);

    // Добавление записи до
    connect(actionAddNewBefore, &QAction::triggered, recordTableController, &RecordTableController::addNewBeforeContext);

    // Добавление записи после
    connect(actionAddNewAfter, &QAction::triggered, recordTableController, &RecordTableController::addNewAfterContext);

    // Редактирование записи
    connect(actionEditField, &QAction::triggered, recordTableController, &RecordTableController::onEditFieldContext);

    // Блокировка записи
    connect(actionBlock, &QAction::triggered, recordTableController, &RecordTableController::onBlockContext);

    // Звездочка избранного
    connect(actionFavorite, &QAction::triggered, recordTableController, &RecordTableController::onFavoriteContext);

    // Удаление записи
    connect(actionDelete, &QAction::triggered, recordTableController, &RecordTableController::deleteContext);

    // Удаление записи с копированием в буфер обмена
    connect(actionCut, &QAction::triggered, recordTableController, &RecordTableController::cut);

    // Копирование записи (записей) в буфер обмена
    connect(actionCopy, &QAction::triggered, recordTableController, &RecordTableController::copy);

    // Вставка записи из буфера обмена
    connect(actionPaste, &QAction::triggered, recordTableController, &RecordTableController::paste);

    // Настройка внешнего вида таблицы конечных записей
    connect(actionSettings, &QAction::triggered, recordTableController, &RecordTableController::settings);

    // Перемещение записи вверх
    connect(actionMoveUp, &QAction::triggered, recordTableController, &RecordTableController::moveUp);

    // Перемещение записи вниз
    connect(actionMoveDn, &QAction::triggered, recordTableController, &RecordTableController::moveDn);

    // Поиск по базе (клик связывается с действием в MainWindow)
    connect(actionFindInBase, &QAction::triggered, find_object<MainWindow>("mainwindow"), &MainWindow::toolsFindInBase);

    // Предпросмотр печати текущей записи (действие живет в MainWindow)
    connect(actionPrintPreview, &QAction::triggered, find_object<MainWindow>("mainwindow"), &MainWindow::filePrintPreview);

    // Синхронизация базы (действие переехало с тулбара веток)
    connect(actionSyncro, &QAction::triggered, this, &RecordTableScreen::onSyncroClick);

    // Перемещение по истории посещаемых записей назад
    connect(actionWalkHistoryPrevious, &QAction::triggered, this, &RecordTableScreen::onWalkHistoryPreviousClick);

    // Перемещение по истории посещаемых записей вперед
    connect(actionWalkHistoryNext, &QAction::triggered, this, &RecordTableScreen::onWalkHistoryNextClick);

    // Кнопка Назад (Back) в мобильном интерфейсе
    connect(actionBack, &QAction::triggered, this, &RecordTableScreen::onBackClick);

    // Действия по сортировке
    connect(actionSort, &QAction::triggered, recordTableController, &RecordTableController::onSortClick);

    // Кнопка вызова печати таблицы конечных записей
    connect(actionPrint, &QAction::triggered, recordTableController, &RecordTableController::onPrintClick);

    // Кнопка истории изменений записи
    connect(actionChangeHistory, &QAction::triggered, recordTableController, &RecordTableController::onChangeHistoryClick);

    // Кнопка копирования ссылки на запись
    connect(actionCopyRecordReference, &QAction::triggered, this, &RecordTableScreen::onCopyRecordReference);

    // Кнопка копирования готовой ссылки с заголовком
    connect(actionCopyRecordLink, &QAction::triggered, this, &RecordTableScreen::onCopyRecordLink);

    // Кнопка переключения режима одинарного выбора и мультивыбора
    connect(actionSwitchSelectionMode, &QAction::triggered, recordTableController, &RecordTableController::onSwitchSelectionMode);

    // Обновление горячих клавиш, если они были изменены
    connect(&shortcutManager, &ShortcutManager::updateWidgetShortcut, this, &RecordTableScreen::setupShortcuts);
}


void RecordTableScreen::assembly(void)
{
 recordTableToolsLayout=new QHBoxLayout();
 recordTableToolsLayout->addWidget(toolsLine, 1);
 recordTableToolsLayout->addWidget(extraToolsLine);
 
 recordTableScreenLayout=new QVBoxLayout();
 recordTableScreenLayout->setObjectName("recordtablescreen_QVBoxLayout");

 recordTableScreenLayout->addLayout(recordTableToolsLayout);
 recordTableScreenLayout->addWidget(treePathLabel);
 recordTableScreenLayout->addWidget(recordTableController->getView());

 // Зазоры по 1px: штатный spacing давал полосу пустоты
 // между тулбаром и таблицей
 recordTableScreenLayout->setSpacing(1);

 setLayout(recordTableScreenLayout);

 // Границы убираются, так как данный объект будет использоваться как виджет
 QLayout *lt;
 lt=layout();
 lt->setContentsMargins(0,1,0,0);
}


// Отключение всех действий над записями
// (но не всех действий на панели инструментов,
// так как на панели инструментов есть действия,
// не оказывающие воздействия на записи)
void RecordTableScreen::disableAllActions(void)
{
 actionAddNewToEnd->setEnabled(false);
 actionAddNewBefore->setEnabled(false);
 actionAddNewAfter->setEnabled(false);
 actionEditField->setEnabled(false);
 actionBlock->setEnabled(false);
 actionDelete->setEnabled(false);

 actionCut->setEnabled(false);
 actionCopy->setEnabled(false);
 actionPaste->setEnabled(false);

 actionMoveUp->setEnabled(false);
 actionMoveDn->setEnabled(false);

 actionCopyRecordReference->setEnabled(false);
 actionCopyRecordLink->setEnabled(false);

 // Предпросмотр печати возможен только для одной выбранной записи
 actionPrintPreview->setEnabled(false);

 actionChangeHistory->setEnabled(false);

 actionSwitchSelectionMode->setEnabled(false);
}


void RecordTableScreen::toolsUpdate(void)
{
  toolsWidgetsUpdate();
  editorModesUpdate();
}


void RecordTableScreen::toolsWidgetsUpdate()
{
 // qDebug() << "recordtablescreen::tools_update()";

 // Отключаются все действия
 disableAllActions();

 if(recordTableController->isTableNotExists())
  return;

 // Выясняется, содержит ли текущая ветка подчиненные ветки
 /*
 QModelIndex index = find_object<TreeScreen>("treeScreen")->get_selection_model()->currentIndex();
 TreeItem *item = find_object<TreeScreen>("treeScreen")->kntrmodel->getItem(index);
 int branch_have_children=0;
 if(item->childCount()>0)branch_have_children=1;
 */

 // Включаются те действия которые разрешены

 // Добавление записи
 // Добавлять можно к любой ветке
 actionAddNewToEnd->setEnabled(true);

 // Добавление записи до
 // Добавлять "до" можно только тогда, когда выбрана только одна строка
 // и не включена сортировка
 if(recordTableController->getView()->selectionModel()->hasSelection() &&
         (recordTableController->getView()->selectionModel()->selectedRows()).size()==1 &&
         recordTableController->getView()->isSortingEnabled()==false )
   actionAddNewBefore->setEnabled(true);

 // Добавление записи после
 // Добавлять "после" можно только тогда, когда выбрана только одна строка
 // и не включена сортировка
 if(recordTableController->getView()->selectionModel()->hasSelection() &&
    (recordTableController->getView()->selectionModel()->selectedRows()).size()==1 &&
    recordTableController->getView()->isSortingEnabled()==false )
   actionAddNewAfter->setEnabled(true);

 // Редактирование записи и получение ссылки на запись
 // Редактировать можно только тогда, когда выбрана только одна строка
 if(recordTableController->getView()->selectionModel()->hasSelection() &&
    (recordTableController->getView()->selectionModel()->selectedRows()).size()==1)
 {
   actionEditField->setEnabled(true);
   actionBlock->setEnabled(true);
   actionCopyRecordReference->setEnabled(true);
   actionCopyRecordLink->setEnabled(true);
   actionPrintPreview->setEnabled(true);

   // История выключена мастер-тумблером: пункт серый сразу,
   // а не вежливым отказом по клику
   actionChangeHistory->setEnabled(mytetraConfig.get_historyEnable());
 }

 // Удаление записи
 // Пункт активен только если запись (или записи) выбраны в списке
 if(recordTableController->getView()->selectionModel()->hasSelection())
  actionDelete->setEnabled(true);

 // Удаление с копированием записи в буфер обмена
 // Пункт активен только если запись (или записи) выбраны в списке
 if(recordTableController->getView()->selectionModel()->hasSelection())
  actionCut->setEnabled(true);

 // Копирование записи в буфер обмена
 // Пункт активен только если запись (или записи) выбраны в списке
 if(recordTableController->getView()->selectionModel()->hasSelection())
  actionCopy->setEnabled(true);

 // Вставка записи из буфера обмена
 // Вставлять записи можно только тогда, когда выбрана
 // только одна строка (добавляется после выделеной строки)
 // или не выбрано ни одной строки (тогда добавляется в конец списка)
 // или записей вообще нет
 // И проверяется, содержит ли буфер обмена данные нужного формата
 if((recordTableController->getView()->selectionModel()->hasSelection() && (recordTableController->getView()->selectionModel()->selectedRows()).size()==1) ||
     recordTableController->getView()->selectionModel()->hasSelection()==false ||
     recordTableController->getView()->model()->rowCount()==0)
  {
   const QMimeData *mimeData = internalClipboard->mimeData();
   if(mimeData!=nullptr and
      mimeData->hasFormat(FixedParameters::appTextId+"/records"))
   {
     actionPaste->setEnabled(true);
   }
  }

 // Перемещение записи вверх
 // Пункт возможен только когда выбрана одна строка
 // и указатель стоит не на начале списка
 // и не включена сортировка
 if(recordTableController->getView()->selectionModel()->hasSelection() &&
    (recordTableController->getView()->selectionModel()->selectedRows()).size()==1 &&
    recordTableController->getView()->isSelectedSetToTop()==false &&
    recordTableController->getView()->isSortingEnabled()==false)
  actionMoveUp->setEnabled(true);

 // Перемещение записи вниз
 // Пункт возможен только когда выбрана одна строка
 // и указатель стоит не в конце списка
 // и не включена сортировка
 if(recordTableController->getView()->selectionModel()->hasSelection() &&
    (recordTableController->getView()->selectionModel()->selectedRows()).size()==1 &&
    recordTableController->getView()->isSelectedSetToBottom()==false &&
    recordTableController->getView()->isSortingEnabled()==false)
  actionMoveDn->setEnabled(true);

 // Переключение между режимами выбора в списке записей возможно всегда
 actionSwitchSelectionMode->setEnabled(true);

 // Глобальный режим только чтения: мутирующие действия недоступны.
 // Копирование записей, печать, поиск и навигация разрешены
 if(mytetraConfig.get_readOnly())
  {
   actionAddNewToEnd->setEnabled(false);
   actionAddNewBefore->setEnabled(false);
   actionAddNewAfter->setEnabled(false);
   actionEditField->setEnabled(false);
   actionBlock->setEnabled(false);
   actionDelete->setEnabled(false);

   actionCut->setEnabled(false);
   actionPaste->setEnabled(false);

   actionMoveUp->setEnabled(false);
   actionMoveDn->setEnabled(false);
  }

 // Перетаскивание записей запрещено, выбор работает
 recordTableController->getView()->setDragEnabled(!mytetraConfig.get_readOnly());
}


// Обновление режимов редактора согласно состоянию записи в таблице
void RecordTableScreen::editorModesUpdate()
{
 // Если ни одна запись не выбрана
 if( !(recordTableController->getView()->selectionModel()->hasSelection() &&
       recordTableController->getRowCount()>0) )
 {
   qDebug() << "In table select non present";

   // Блокировка возможности редактирования текста записи (разблокировка произойдет при клике на запись)
   find_object<MetaEditor>("editorScreen")->setTextareaEditable(false);
 }
}


// Получение номера первого выделенного элемента в таблице записи на экране
int RecordTableScreen::getFirstSelectionPos(void)
{
  return recordTableController->getFirstSelectionPos();
}


// Получение ID первого выделенного элемента в таблице записи на экране
QString RecordTableScreen::getFirstSelectionId(void)
{
  return recordTableController->getFirstSelectionId();
}


// Установка засветки в нужную строку в таблице записи на экране
void RecordTableScreen::setSelectionToPos(int pos)
{
  recordTableController->setSelectionToPos(pos);
}


// Установка засветки в нужную запись в таблице записей на экране
void RecordTableScreen::setSelectionToId(QString id)
{
  recordTableController->setSelectionToId(id);
}



void RecordTableScreen::onWalkHistoryPreviousClick(void)
{
  find_object<MainWindow>("mainwindow")->goWalkHistoryPrevious();
}


void RecordTableScreen::onWalkHistoryNextClick(void)
{
  find_object<MainWindow>("mainwindow")->goWalkHistoryNext();
}


// Возвращение к дереву разделов в мобильном интерфейсе
void RecordTableScreen::onBackClick(void)
{
  globalParameters.getWindowSwitcher()->switchFromRecordtableToTree();
}


// Копирование в системный буфер обмена ссылки на запись
void RecordTableScreen::onCopyRecordReference()
{
  QString reference=FixedParameters::appTextId+"://note/"+getFirstSelectionId();

  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(reference);
}


// Готовая ссылка с заголовком: HTML-якорь для вставки в другую запись
// (там сразу именованная ссылка) плюс голый href как plain-текст.
// Имя берётся из модели дерева тем же путём что ReferenceFormatter,
// пустое имя — голый href как раньше
void RecordTableScreen::onCopyRecordLink()
{
  const QString id=getFirstSelectionId();
  const QString reference=FixedParameters::appTextId+"://note/"+id;

  QString title;
  KnowTreeView *treeView=find_object<KnowTreeView>("knowTreeView");

  if(treeView!=nullptr)
  {
    Record *record=static_cast<KnowTreeModel *>(treeView->model())->getRecord(id);

    if(record!=nullptr)
      title=record->getField(QStringLiteral("name"));
  }

  if(title.isEmpty())
    title=reference;

  QMimeData *mime=new QMimeData();
  mime->setHtml(QStringLiteral("<a href=\"")+reference+
                QStringLiteral("\">")+title.toHtmlEscaped()+
                QStringLiteral("</a>"));
  mime->setText(reference);

  QApplication::clipboard()->setMimeData(mime);
}


// Клик по кнопке синхронизации: операция уровня базы, выполняется
// главным окном
void RecordTableScreen::onSyncroClick(void)
{
  MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

  if(mainWindow!=nullptr)
    mainWindow->synchronization();
}


// Разблокировка кнопки синхронизации по окончании команды.
// Подключено в MainWindow к завершению синхрокоманды
void RecordTableScreen::onSyncroCommandFinishWork(void)
{
  actionSyncro->setEnabled(true);
}


void RecordTableScreen::setSyncroEnabled(bool state)
{
  actionSyncro->setEnabled(state);
}


bool RecordTableScreen::isSyncroEnabled(void) const
{
  return actionSyncro->isEnabled();
}


// Контекстное меню тулбаров записей: единственный пункт открывает
// диалог настройки кнопок. Показывается по ПКМ в любом месте тулбара:
// у кнопок своего меню нет, а пустого места на ужатых по кнопкам
// линиях может не быть вовсе
void RecordTableScreen::onToolbarContextMenu(const QPoint &pos)
{
 QToolBar *line=qobject_cast<QToolBar *>(sender());

 if(line==nullptr)
  return;

 QMenu menu(this);
 QAction *setupAction=menu.addAction(tr("Customize toolbar..."));

 if(menu.exec(line->mapToGlobal(pos))==setupAction)
  {
   MainToolbarSettingsScreen settingsScreen(this);
   settingsScreen.exec();
  }
}


void RecordTableScreen::setTreePath(QString path)
{
  treePath=path; // Запоминается путь к ветке в виде строки
  treePathLabel->setText(tr("<b>Path:</b> ")+treePath);
}


QString RecordTableScreen::getTreePath(void)
{
  return treePath;
}


// Прямые тексты действий и пути. Тексты на шорткатах освежает
// setupShortcuts по сигналу updateWidgetShortcut
void RecordTableScreen::retranslateUi(void)
{
  actionSettings->setText(tr("View settings"));
  actionSettings->setStatusTip(tr("Setup table view settins"));

  actionFindInBase->setText(shortcutManager.getDescription("misc-findInBase"));

  actionWalkHistoryPrevious->setText(tr("Previous viewing note"));
  actionWalkHistoryNext->setText(tr("Next viewing note"));

  actionBack->setText(tr("Back to item tree"));
  actionBack->setStatusTip(tr("Back to item tree"));

  actionSort->setText(tr("Toggle sorting"));
  actionSort->setStatusTip(tr("Enable/disable sorting by column"));

  actionPrint->setText(tr("Print table"));
  actionPrint->setStatusTip(tr("Print current notes table"));

  actionPrintPreview->setText(tr("Print preview"));
  actionPrintPreview->setStatusTip(tr("Preview current note before printing"));

  actionSyncro->setText(tr("Synchronization"));
  actionSyncro->setStatusTip(tr("Run synchronization"));

  actionCopyRecordReference->setText(tr("Copy note reference"));
  actionCopyRecordReference->setStatusTip(tr("Copy note reference to clipboard"));

  actionCopyRecordLink->setText(tr("Copy link with title"));
  actionCopyRecordLink->setStatusTip(tr("Copy titled link to clipboard for pasting into another note"));

  actionChangeHistory->setText(tr("Change history"));
  actionChangeHistory->setStatusTip(tr("Show change history with diff and rollback"));

  actionSwitchSelectionMode->setText(tr("Switch select/multiselect"));
  actionSwitchSelectionMode->setStatusTip(tr("Switch note selection mode (Notice: if multiselect is on, drag-and-drop is disabled)"));

  treePathLabel->setText(tr("<b>Path:</b> ")+treePath);
}


// Установка фокуса на базовый виджет (на список веток дерева)
void RecordTableScreen::setFocusToBaseWidget()
{
    recordTableController->setFocusToBaseWidget();
}


// Переприменить read-only редактора (для глобального режима без смены засветки)
void RecordTableScreen::refreshEditorReadOnly(void)
{
 recordTableController->refreshEditorReadOnly();
}


// Звездочка текущей записи для меню
bool RecordTableScreen::isCurrentRecordFavorite(void)
{
  if(recordTableController==nullptr)
    return false;

  return recordTableController->isCurrentRecordFavorite();
}

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


TreeScreen::TreeScreen(Favorites *favorites, BacklinkIndex *backlinks, EditorShowTextDispatcher *dispatcher, QWidget *parent) : QWidget(parent),
  m_favorites(favorites),
  m_backlinks(backlinks),
  m_dispatcher(dispatcher)
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

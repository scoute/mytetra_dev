// Цель Windows — до ЛЮБЫХ инклудов: Qt-заголовки сами тянут windows.h,
// и поздний _WIN32_WINNT уже ничего не даёт (QueryFullProcessImageNameA
// из mcpPeerAlive() тогда не объявлена, MinGW32 падает)
#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <QDialog>
#include <QMap>
#include <QString>
#include <QDir>
#include <QString>
#include <QDesktopWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPrintDialog>
#include <QPrinter>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSplitter>
#include <QDockWidget>
#include <QStatusBar>
#include <QActionGroup>
#include <QColorDialog>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>

#ifdef Q_OS_WIN32
#include <windows.h>
#else
#include <signal.h>
#include <errno.h>
#endif
#include <QTime>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include "models/appConfig/AppConfig.h"
#include "MainWindow.h"
#include "MenuBuilder.h"
#include "views/appConfigWindow/AppConfigDialog.h"
#include "views/tree/TreeScreen.h"
#include "views/record/MetaEditor.h"
#include "libraries/wyedit/EditorToolBarAssistant.h"
#include "libraries/wyedit/EditorConfig.h"
#include "views/recordTable/RecordTableScreen.h"
#include "views/favoritesPanel/FavoritesPanel.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigUi.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/helpers/CssHelper.h"

// Создание раздела меню File

void MainWindow::runDirectPreferences(QAction *action)
{
    QMap< QString, QAction *> map;

    map["pageMain"]       = menuBuilder->action(MenuBuilder::ActionId::DirectMain);
    map["pageAppearance"] = menuBuilder->action(MenuBuilder::ActionId::DirectAppearance);
    map["pageCrypt"]      = menuBuilder->action(MenuBuilder::ActionId::DirectCrypt);
    map["pageSynchro"]    = menuBuilder->action(MenuBuilder::ActionId::DirectSyncro);
    map["pageRecordTable"]= menuBuilder->action(MenuBuilder::ActionId::DirectRecordTable);
    map["pageAttach"]     = menuBuilder->action(MenuBuilder::ActionId::DirectAttach);
    map["pageKeyboard"]   = menuBuilder->action(MenuBuilder::ActionId::DirectKeyboard);
    map["pageHistory"]    = menuBuilder->action(MenuBuilder::ActionId::DirectHistory);
    map["pageMisc"]       = menuBuilder->action(MenuBuilder::ActionId::DirectMisc);

    AppConfigDialog *dialog=nullptr;

    for(auto pageName : map.keys())
    {
        if( map[pageName]==action )
        {
            dialog=new AppConfigDialog(pageName, this);
            break;
        }
    }

    if(dialog!=nullptr)
    {
        dialog->setMenuListVisible(false);
        dialog->exec();

        // Настройки могли поменять мастер-тумблеры (история и пр.):
        // состояние пунктов перечитывается сразу, а не по следующему клику
        if(recordTableScreen!=nullptr)
            recordTableScreen->toolsUpdate();
    }

    delete dialog;
}

// Создание раздела меню Help

// Создание раздела меню Help

// Пометка текущих состояний при каждом открытии: все меняется
// и мимо меню (диалог настроек, контекстное меню секрета)
void MainWindow::syncViewMenu(void)
{
    if(menuBuilder->action(MenuBuilder::ActionId::ViewHeader)!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::ViewHeader)->setChecked(AppContext::instance().appConfig()->uiSection()->get_recordHeaderVisible());

    if(menuBuilder->action(MenuBuilder::ActionId::ViewFavorites)!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::ViewFavorites)->setChecked(AppContext::instance().appConfig()->uiSection()->get_favoritesEnabled());

    if(menuBuilder->action(MenuBuilder::ActionId::ViewTreeFilter)!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::ViewTreeFilter)->setChecked(AppContext::instance().appConfig()->uiSection()->get_treeFilterVisible());

    // Панель может прятаться мимо меню (кнопка toggleTools, хоткей):
    // пометка подтягивается при каждом открытии
    if(menuBuilder->action(MenuBuilder::ActionId::ViewTools)!=nullptr && editorScreen!=nullptr &&
       editorScreen->editorToolBarAssistant!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::ViewTools)->setChecked(!editorScreen->editorConfig->get_hide_tools_lines());

    if(menuBuilder->action(MenuBuilder::ActionId::ViewReadOnly)!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::ViewReadOnly)->setChecked(AppContext::instance().appConfig()->recordTableSection()->isGlobalReadOnly());

    if(menuBuilder->action(MenuBuilder::ActionId::ViewHtmlSource)!=nullptr && editorScreen!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::ViewHtmlSource)->setChecked(editorScreen->isHtmlSourceVisible());
}

void MainWindow::onViewHeaderToggled(bool checked)
{
    AppContext::instance().appConfig()->uiSection()->set_recordHeaderVisible(checked);

    if(editorScreen!=nullptr)
        editorScreen->updateRecordHeaderVisibility();
}

void MainWindow::onViewFavoritesToggled(bool checked)
{
    AppContext::instance().appConfig()->uiSection()->set_favoritesEnabled(checked);

    // Сам refresh решит показать или спрятать по наличию звездочек
    FavoritesPanel *favoritesPanel=find_object<FavoritesPanel>("favoritesPanel");
    if(favoritesPanel!=nullptr)
        favoritesPanel->refreshFavorites();
}

void MainWindow::onViewTreeFilterToggled(bool checked)
{
    AppContext::instance().appConfig()->uiSection()->set_treeFilterVisible(checked);

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen!=nullptr)
        treeScreen->setTreeFilterVisible(checked);
}

// Возврат спрятанной панели инструментов: чекбокс Вид всегда доступен,
// в отличие от кнопки toggleTools, которая уезжает вместе с панелью

// Возврат спрятанной панели инструментов: чекбокс Вид всегда доступен,
// в отличие от кнопки toggleTools, которая уезжает вместе с панелью

// Возврат спрятанной панели инструментов: чекбокс Вид всегда доступен,
// в отличие от кнопки toggleTools, которая уезжает вместе с панелью
void MainWindow::onViewToolsToggled(bool checked)
{
    if(editorScreen==nullptr || editorScreen->editorToolBarAssistant==nullptr)
        return;

    editorScreen->editorToolBarAssistant->switchToolsVisibility(checked ? 1 : -1);
}

// Переключение глобального режима только чтения: навигация и просмотр
// разрешены, любые изменения базы запрещены

// Переключение глобального режима только чтения: навигация и просмотр
// разрешены, любые изменения базы запрещены

// Переключение глобального режима только чтения: навигация и просмотр
// разрешены, любые изменения базы запрещены
void MainWindow::onViewReadOnlyToggled(bool checked)
{
    AppContext::instance().appConfig()->recordTableSection()->set_readOnly(checked);

    applyReadOnly();
}


// Живой исходник под заметкой: чекбокс только переключает панель,
// состояние на сессию, в конфиг не пишется
void MainWindow::onViewHtmlSourceToggled(bool checked)
{
    if(editorScreen==nullptr)
        return;

    editorScreen->setHtmlSourceVisible(checked);
}

// Применить глобальный режим только чтения ко всем экранам.
// Вызывается при переключении и при старте программы

// Применить глобальный режим только чтения ко всем экранам.
// Вызывается при переключении и при старте программы

// Применить глобальный режим только чтения ко всем экранам.
// Вызывается при переключении и при старте программы
void MainWindow::applyReadOnly(void)
{
    const bool readOnly=AppContext::instance().appConfig()->recordTableSection()->get_readOnly();

    if(treeScreen!=nullptr)
        treeScreen->setReadOnly(readOnly);

    // Действия таблицы записей пересчитываются с учётом флага,
    // редактор — по текущей записи и флагу
    if(recordTableScreen!=nullptr)
        {
         recordTableScreen->toolsUpdate();
         recordTableScreen->refreshEditorReadOnly();

         // Кнопка синхронизации меняет базу (и дёргает её же автосинхронизация)
         recordTableScreen->setSyncroEnabled(!readOnly);
        }

    // Импорт ветки и управление базами меняют данные
    if(menuBuilder->action(MenuBuilder::ActionId::FileImportTree)!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::FileImportTree)->setEnabled(!readOnly);

    if(menuBuilder->action(MenuBuilder::ActionId::FileImportTelegram)!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::FileImportTelegram)->setEnabled(!readOnly);

    if(menuBuilder->action(MenuBuilder::ActionId::FileDatabases)!=nullptr)
        menuBuilder->action(MenuBuilder::ActionId::FileDatabases)->setEnabled(!readOnly);
}

void MainWindow::onViewSecretColor(void)
{
    const QColor current(AppContext::instance().appConfig()->uiSection()->get_secretColor());
    const QColor chosen=QColorDialog::getColor(current, this);

    if(!chosen.isValid() || chosen==current)
        return;

    AppContext::instance().appConfig()->uiSection()->set_secretColor(chosen.name());

    // Сразу на диск как в контекстном меню секрета
    AppContext::instance().appConfig()->sync();

    // Цвет действует только на Custom: быстрый выбор сам переводит
    // туда с текущей темой как базой, иначе клик ничего не меняет
    if(AppContext::instance().appConfig()->uiSection()->getInterfaceTheme()!="custom")
    {
        AppContext::instance().appConfig()->uiSection()->set_themeCustomBaseTheme(AppContext::instance().appConfig()->uiSection()->getInterfaceTheme());

        if(AppContext::instance().appConfig()->uiSection()->setInterfaceTheme("custom"))
            CssHelper::loadCurrentTheme();
    }

    if(editorScreen!=nullptr)
        editorScreen->repaintSecrets();
}

void MainWindow::onViewPaperColor(void)
{
    const QColor current(
        MetaEditor::effectiveNotePaperColor(AppContext::instance().appConfig()->uiSection()->getInterfaceTheme(),
                                           AppContext::instance().appConfig()->uiSection()->get_notePaperColor()));
    const QColor chosen=QColorDialog::getColor(current, this);

    if(!chosen.isValid() || chosen==current)
        return;

    AppContext::instance().appConfig()->uiSection()->set_notePaperColor(chosen.name());

    // Сразу на диск как в соседнем пункте про цвет секрета
    AppContext::instance().appConfig()->sync();

    // Бумага действует только на Custom: быстрый выбор сам переводит
    // туда с текущей темой как базой, иначе клик ничего не меняет
    if(AppContext::instance().appConfig()->uiSection()->getInterfaceTheme()!="custom")
    {
        AppContext::instance().appConfig()->uiSection()->set_themeCustomBaseTheme(AppContext::instance().appConfig()->uiSection()->getInterfaceTheme());

        if(AppContext::instance().appConfig()->uiSection()->setInterfaceTheme("custom"))
            CssHelper::loadCurrentTheme();
    }

    if(editorScreen!=nullptr)
        editorScreen->applyNoteColors();
}

// Клик по пункту меню Темы: применить выбранную тему сразу

// Клик по пункту меню Темы: применить выбранную тему сразу

// Клик по пункту меню Темы: применить выбранную тему сразу
void MainWindow::onThemeMenuTriggered(QAction *action)
{
    if(action==nullptr)
        return;

    QString themeName=action->data().toString();

    if(themeName==AppContext::instance().appConfig()->uiSection()->getInterfaceTheme())
        return;

    if(AppContext::instance().appConfig()->uiSection()->setInterfaceTheme(themeName))
    {
        CssHelper::loadCurrentTheme();

        // Своя бумага поверх темы переживает смену темы,
        // пустая настройка подхватывает цвет новой темы
        if(editorScreen!=nullptr)
            editorScreen->applyNoteColors();
    }
}

// Пункт «Настроить тему»: то же окно, что страница Themes
// в диалоге настроек. Пометка меню обновится при следующем
// открытии через aboutToShow

// Пункт «Настроить тему»: то же окно, что страница Themes
// в диалоге настроек. Пометка меню обновится при следующем
// открытии через aboutToShow

// Пункт «Настроить тему»: то же окно, что страница Themes
// в диалоге настроек. Пометка меню обновится при следующем
// открытии через aboutToShow
void MainWindow::onThemeConfigure(void)
{
    AppConfigDialog dialog("pageTheme", this);
    dialog.exec();
}

// Создание скрытых действий для работы прочих шорткатов уровня приложения

// Создание скрытых действий для работы прочих шорткатов уровня приложения

// Создание скрытых действий для работы прочих шорткатов уровня приложения
void MainWindow::initHiddenActions(void)
{
    // Действия добавляются в сам виджет
    // Для таких действий нет визуального представления, но горячие клавиши для них работают
    actionFocusTree = new QAction(this);
    this->addAction(actionFocusTree);

    actionFocusNoteTable = new QAction(this);
    this->addAction(actionFocusNoteTable);

    actionFocusEditor = new QAction(this);
    this->addAction(actionFocusEditor);
}

// Пересчитать и показать статистику базы в статусбаре.
// Вызывается по сигналам модели и точечно после правок

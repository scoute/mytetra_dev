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
#include "views/tree/TreeScreen.h"
#include "views/record/MetaEditor.h"
#include "views/recordTable/RecordTableScreen.h"
#include "models/appConfig/AppConfigFind.h"
#include "models/appConfig/AppConfigRecordTable.h"
#include "models/appConfig/AppConfigUi.h"
#include "models/tree/TreeItem.h"
#include "views/findInBaseScreen/FindScreen.h"
#include "models/tree/KnowTreeModel.h"
#include "libraries/GlobalParameters.h"
#include "libraries/WindowSwitcher.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/wyedit/EditorShowTextDispatcher.h"

// Восстанавливается геометрия окна и позиции основных разделителей
void MainWindow::restoreWindowGeometry(void)
{
    if(AppContext::instance().globalParameters()->getTargetOs()=="android")
        setWindowState(Qt::WindowMaximized); // Для Андроида окно просто разворачивается на весь экран
    else
        restoreGeometry( AppContext::instance().appConfig()->uiSection()->get_mainwingeometry() );

    // Размеры применяются только при совпадении числа виджетов:
    // в режиме "список под деревом" у vSplitter один виджет, списки
    // из другого режима сюда не подходят
    if(AppContext::instance().appConfig()->uiSection()->get_vspl_size_list().size()==vSplitter->count())
        vSplitter->setSizes(AppContext::instance().appConfig()->uiSection()->get_vspl_size_list());

    if(AppContext::instance().appConfig()->uiSection()->get_hspl_size_list().size()==hSplitter->count())
        hSplitter->setSizes(AppContext::instance().appConfig()->uiSection()->get_hspl_size_list());

    findSplitter->setSizes(AppContext::instance().appConfig()->uiSection()->get_findsplitter_size_list());
}

// Перестановка списка записей: справа от дерева (дефолт) либо под
// деревом в левой колонке. Вызывается при старте после восстановления
// геометрии и вживую из настроек. Существующие высоты виджетов
// сохраняются как есть, поэтому переключение туда-обратно не прыгает

// Перестановка списка записей: справа от дерева (дефолт) либо под
// деревом в левой колонке. Вызывается при старте после восстановления
// геометрии и вживую из настроек. Существующие высоты виджетов
// сохраняются как есть, поэтому переключение туда-обратно не прыгает
void MainWindow::applyRecordTablePosition(void)
{
    const bool under=AppContext::instance().appConfig()->uiSection()->get_recordTableUnderTree();
    const bool isUnder=(leftColumnSplitter!=nullptr);

    if(under==isUnder)
        return;

    if(under)
    {
        // Список записей переезжает из правой колонки под дерево.
        // vSplitter остаётся с одним редактором.
        // Важно: только addWidget/insertWidget, никакого replaceWidget -
        // после него сплиттер навсегда игнорирует setSizes (стенд).
        // addWidget сам вытаскивает виджет из старого сплиттера
        leftColumnSplitter=new QSplitter(Qt::Vertical, this);
        leftColumnSplitter->setObjectName("leftColumnSplitter");

        leftColumnSplitter->addWidget(treeScreen);
        leftColumnSplitter->addWidget(recordTableScreen);
        hSplitter->insertWidget(0, leftColumnSplitter);

        leftColumnSplitter->setCollapsible(0, false);
        leftColumnSplitter->setCollapsible(1, false);

        // Размеры - после показа: до show() раздавать некому.
        // Живьём окно уже показано - ставим сразу, на холодном старте
        // ждём показа через синглшот-цепочку со сходимостью
        if(isVisible())
            applyRecordTableSizes();
        else
        {
            recordTableSizesRetry=0;
            QTimer::singleShot(0, this, &MainWindow::applyRecordTableSizes);
        }
    }
    else
    {
        // Список записей возвращается в правую колонку над редактором.
        // Порядок важен: сначала записи в vSplitter, потом дерево в
        // начало hSplitter. Без replaceWidget, по той же причине
        vSplitter->insertWidget(0, recordTableScreen);
        hSplitter->insertWidget(0, treeScreen);
        vSplitter->setCollapsible(0, false);
        vSplitter->setCollapsible(1, false);

        // Синхронное удаление: виджеты уже переехали, детей нет.
        // deleteLater оставлял бы пустой сплиттер болтаться до
        // следующего прохода событийной петли
        delete leftColumnSplitter;
        leftColumnSplitter=nullptr;

        // vSplitter c одним редактором хранил одно число: вернуть
        // обе панели к осмысленным долям текущей высоты.
        // Назад переключаются только живьём (окно показано),
        // поэтому immediate setSizes здесь работает
        const int total=vSplitter->height();

        if(total>0)
            vSplitter->setSizes(QList<int>() << total/3 << total-total/3);
    }

    recordTableScreen->toolsUpdate();
}

// Отложенное применение размеров после перестановки списка записей.
// Вызывается через singleShot(0): setSizes сразу после replaceWidget
// сплиттер игнорирует, а до show() раздавать некому. К моменту вызова
// окно уже показано, размеры живые. Идемпотентно: при повторных
// вызовах и смене режима туда-сюда пересчитывает текущее состояние,
// сохранённые списки имеют приоритет

// Отложенное применение размеров после перестановки списка записей.
// Вызывается через singleShot(0): setSizes сразу после replaceWidget
// сплиттер игнорирует, а до show() раздавать некому. К моменту вызова
// окно уже показано, размеры живые. Идемпотентно: при повторных
// вызовах и смене режима туда-сюда пересчитывает текущее состояние,
// сохранённые списки имеют приоритет
void MainWindow::applyRecordTableSizes(void)
{
    if(leftColumnSplitter==nullptr)
        return;

    // Ширина левой колонки: сохранённая, иначе треть окна
    int leftW=-1;
    const QList<int> hs=hSplitter->sizes();

    if(hs.size()>0 && hs.at(0)>0)
        leftW=hs.at(0);

    if(leftW<=0)
    {
        int total=hSplitter->width();

        if(total<=0)
            total=900;

        leftW=total/3;
    }

    hSplitter->setSizes(QList<int>() << leftW << (hSplitter->width()-leftW>0 ? hSplitter->width()-leftW : leftW*2));

    // Высота дерева и списка внутри колонки: сохранённые, иначе поровну
    const QList<int> saved=AppContext::instance().appConfig()->uiSection()->get_leftspl_size_list();

    if(saved.size()==leftColumnSplitter->count())
        leftColumnSplitter->setSizes(saved);
    else
    {
        const int total=leftColumnSplitter->height();
        const int each=(total>0) ? total/2 : -1;

        if(each>0)
            leftColumnSplitter->setSizes(QList<int>() << each << (total-each));
    }

    // Сходимость: если окно ещё не разложено (холодный старт без
    // сохранённой геометрии), выставленные размеры может снести
    // первый проход раскладки. Проверяем видимый итог и повторяем,
    // но не бесконечно
    if(leftColumnSplitter->width()<=0 && recordTableSizesRetry<20)
    {
        recordTableSizesRetry++;
        QTimer::singleShot(100, this, &MainWindow::applyRecordTableSizes);
    }
    else
        recordTableSizesRetry=0;
}

// Запоминается геометрия окна и позиции основных разделителей

// Запоминается геометрия окна и позиции основных разделителей
void MainWindow::saveWindowGeometry(void)
{
    qDebug() << "Save window geometry and splitter sizes";

    AppContext::instance().appConfig()->uiSection()->set_mainwingeometry( saveGeometry() );

    // Размеры vSplitter осмысленны только с двумя виджетами
    // (в режиме "список под деревом" там один редактор)
    if(vSplitter->count()==2)
        AppContext::instance().appConfig()->uiSection()->set_vspl_size_list(vSplitter->sizes());

    AppContext::instance().appConfig()->uiSection()->set_hspl_size_list(hSplitter->sizes());

    // Размеры левой колонки (дерево над списком) - только в этом режиме
    if(leftColumnSplitter!=nullptr)
        AppContext::instance().appConfig()->uiSection()->set_leftspl_size_list(leftColumnSplitter->sizes());

    // Запоминается размер сплиттера только при видимом виджете поиска,
    // т.к. если виджета поиска невидно, будет запомнен нуливой размер

    // if(findScreenDisp->isVisible()) - так делать нельзя, т.к.
    // данный метод вызывается из декструктора главного окна, и к этому моменту
    // виджет уже невиден

    if(AppContext::instance().appConfig()->findSection()->get_findscreen_show())
        AppContext::instance().appConfig()->uiSection()->set_findsplitter_size_list(findSplitter->sizes());
}

void MainWindow::restoreTreePosition(void)
{
    // Путь к последнему выбранному в дереве элементу
    QStringList path=AppContext::instance().appConfig()->uiSection()->get_tree_position();

    qDebug() << "MainWindow::restoreTreePosition() : " << path;

    this->setTreePosition(path);
}

void MainWindow::saveTreePosition(void)
{
    // Получение QModelIndex выделенного в дереве элемента
    QModelIndex index=treeScreen->getCurrentItemIndex();

    // Получаем указатель вида TreeItem
    TreeItem *item =treeScreen->knowTreeModel->getItem(index);

    // Сохраняем путь к элементу item
    AppContext::instance().appConfig()->uiSection()->set_tree_position(item->getPath());
}

void MainWindow::setTreePosition(QStringList path)
{
    if(treeScreen->knowTreeModel->isItemValid(path)==false)
        return;

    // Получаем указатель на элемент вида TreeItem, используя путь, этот указатель может вернуть и корень (если путь пустой)
    TreeItem *item =treeScreen->knowTreeModel->getItem(path);

    // qDebug() << "Set tree position to " << item->getField("name") << " id " << item->getField("id");

    // Из указателя на элемент TreeItem получаем QModelIndex
    QModelIndex setto=treeScreen->knowTreeModel->getIndexByItem(item);

    // Курсор устанавливается в нужную позицию
    treeScreen->setCursorToIndex(setto);
}

bool MainWindow::isTreePositionCrypt()
{
    QStringList path=AppContext::instance().appConfig()->uiSection()->get_tree_position();

    if(treeScreen->knowTreeModel->isItemValid(path)==false)
        return false;

    // Получаем указатель на элемент вида TreeItem, используя путь
    TreeItem *item =treeScreen->knowTreeModel->getItem(path);

    if(item->getField("crypt")=="1")
        return true;
    else
        return false;
}

void MainWindow::restoreRecordTablePosition(void)
{
    QString id=AppContext::instance().appConfig()->recordTableSection()->get_recordtable_selected_record_id();

    if(id.length()>0)
        this->setRecordtablePositionById(id);
}

void MainWindow::saveRecordTablePosition(void)
{
    QString id=recordTableScreen->getFirstSelectionId();

    AppContext::instance().appConfig()->recordTableSection()->set_recordtable_selected_record_id(id);
}

void MainWindow::setRecordtablePositionById(QString id)
{
    recordTableScreen->setSelectionToId(id);
}

void MainWindow::setTreeAndRecordtablePositions(QStringList treePath, QString recordId)
{
    // Прыжок извне при активном фильтре дерева: цель может быть скрыта,
    // тогда фильтр сбрасывается здесь, до позиционирования
    if(treeScreen!=nullptr)
        treeScreen->ensurePathVisible(treePath);

    // В дереве выставляется ветка с заданным путем
    // При этом автоматически вызовется слот TreeScreen::onKnowtreeClicked(),
    // так как происходит выбор в selection-модели
    this->setTreePosition(treePath);

    // Так как выбор ветки в дереве приводит к обновлению таблицы списка записей
    // только на следующей итерации обработки основного цикла событий,
    // то здась цикл событий обновляется, чтобы нормально сработал
    // дальнейший вызов setRecordtablePositionById(), чтобы он выставлял запись
    // уже на обновленной таблице конечных записей
    QCoreApplication::processEvents();

    // В таблице конечных записей выставляется заданная запись
    this->setRecordtablePositionById(recordId);
}

void MainWindow::saveEditorCursorPosition(void)
{
    int n=editorScreen->getCursorPosition();

    AppContext::instance().appConfig()->uiSection()->setEditorCursorPosition(n);
}

void MainWindow::restoreEditorCursorPosition(void)
{
    int n=AppContext::instance().appConfig()->uiSection()->getEditorCursorPosition();

    editorScreen->setCursorPosition(n);
}

void MainWindow::saveEditorScrollBarPosition(void)
{
    int n=editorScreen->getScrollBarPosition();

    AppContext::instance().appConfig()->uiSection()->setEditorScrollBarPosition(n);
}

void MainWindow::restoreEditorScrollBarPosition(void)
{
    int n=AppContext::instance().appConfig()->uiSection()->getEditorScrollBarPosition();

    editorScreen->setScrollBarPosition(n);
}

void MainWindow::restoreFindOnBaseVisible(void)
{
    bool n=AppContext::instance().appConfig()->findSection()->get_findscreen_show();

    // Определяется ссылка на виджет поиска
    FindScreen *findScreenRel=find_object<FindScreen>("findScreenDisp");

    if(n)
        findScreenRel->show();
    else
        findScreenRel->hide();
}

void MainWindow::restoreAllWindowState(void)
{
    AppContext::instance().globalParameters()->getWindowSwitcher()->disableSwitch();

    restoreFindOnBaseVisible();
    restoreWindowGeometry();

    // Раскладка списка записей применяется после геометрии:
    // ей нужны живые размеры виджетов
    applyRecordTablePosition();

    restoreTreePosition();

    // Так как выбор ветки в дереве приводит к обновлению таблицы списка записей
    // только на следующей итерации обработки основного цикла событий,
    // то здась цикл событий обновляется, чтобы нормально сработал
    // дальнейший вызов setRecordtablePositionById(), чтобы он выставлял запись
    // уже на обновленной таблице конечных записей
    QCoreApplication::processEvents();

    restoreRecordTablePosition();

    restoreEditorCursorPosition();
    restoreEditorScrollBarPosition();

    AppContext::instance().globalParameters()->getWindowSwitcher()->enableSwitch();
}

void MainWindow::restoreDockableWindowsState()
{
    // Восстанавливаются открепляемые окна
    AppContext::instance().dispatcher()->restoreOpenWindows();
}

// Создание раздела меню File

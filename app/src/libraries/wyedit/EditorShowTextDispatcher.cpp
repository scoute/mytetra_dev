#include <QThread>

#include "EditorShowTextDispatcher.h"
#include "EditorShowText.h"
#include "Editor.h"
#include "views/mainWindow/MainWindow.h"
#include "views/tree/TreeScreen.h"
#include "views/tree/KnowTreeView.h"
#include "models/tree/KnowTreeModel.h"
#include "models/recordTable/Record.h"
#include "libraries/helpers/ObjectHelper.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/BacklinkIndex.h"


extern AppConfig mytetraConfig;


EditorShowTextDispatcher::EditorShowTextDispatcher(QObject *parent) : QObject(parent)
{
    // Диспетчер живет в GUI-потоке: объект трогает KnowTreeModel
    // и QWidget::close(), рабочий поток запрещен (см. fix/541)
    mThread=nullptr;
}


EditorShowTextDispatcher::~EditorShowTextDispatcher()
{
    // mThread всегда nullptr (см. ctor): гарда на случай
    // будущего возврата потока, условие исправлено (!wait -> terminate)
    if(mThread!=nullptr)
    {
        mThread->quit();
        if(!mThread->wait(1000))
        {
            mThread->terminate();
            mThread->wait();
        }

        delete mThread;
    }
}


EditorShowTextDispatcher *EditorShowTextDispatcher::instance()
{
    static EditorShowTextDispatcher inst;
    return &inst;
}


void EditorShowTextDispatcher::createWindow(const QString &noteId, int x, int y, int w, int h, int vScroll)
{
    // Уже созданное окно не должно открываться дважды
    if( mWindowsList.contains( noteId ) )
    {
        // Если окно было свернуто, его надо показать
        if(mWindowsList[noteId].data()->window()->isMinimized())
        {
            mWindowsList[noteId].data()->window()->showNormal();
        }

        mWindowsList[noteId]->setWindowState(Qt::WindowNoState);
        mWindowsList[noteId]->setWindowState(Qt::WindowActive);

        return;
    }

    // Выясняется ссылка на модель дерева данных
    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    // Если в дереве нет вообще записи с указанным идентификатором, окно с такой записью создать невозможно
    if( !dataModel->isRecordIdExists( noteId ))
    {
        return;
    }


    // Создается открепленное окно
    // Отсутствие родителя будет приводить к отдельному управлению
    // окном виджета при сворачивании/разворачивании.
    // Наличие родителя будет приводить к групповой работе с окном и родительским окном
    // при сворачивании/разворачивании окна виджета.
    // Примечание: нельзя в качестве parent указывать виджет редактора: если редактор
    // станет неактивным (например, когда запись не выбрана)
    // то данное окно тоже станет неактивным, и невозможно будет выделить в нем текст
    QPointer<EditorShowText> editorShowText;
    if(mytetraConfig.getDockableWindowsBehavior()=="single")
    {
        editorShowText=new EditorShowText(nullptr, Qt::Window);
    }
    else if(mytetraConfig.getDockableWindowsBehavior()=="together")
    {
        editorShowText=new EditorShowText( find_object<MainWindow>("mainwindow"), Qt::Dialog );
    }


    // Устанавливается флаг удаления диалога после закрытия его окна
    editorShowText->setAttribute( Qt::WA_DeleteOnClose );

    // Новое окно запоминается в список окон
    mWindowsList.insert( noteId, editorShowText );


    // Создается соединение, обрабатывающее закрытие окна
    connect(editorShowText.data(), &EditorShowText::editorShowTextClose,
            this, &EditorShowTextDispatcher::onCloseWindow);


    // Запись открывается во внутреннем редакторе окна: тот же движок
    // что в главном окне (ссылки, голос, оглавление, картинки, поиск),
    // только без панелей кнопок. Текст грузится из файла записи
    editorShowText->openNote( noteId );

    // Установка координат окна
    if( !(x==-1 and y==-1 and w==-1 and h==-1) )
    {
        editorShowText->setGeometry(x, y, w, h);
    }

    editorShowText->show();

    // Установка прокрутки текста, должна быть после того, как отображаемое окно сформировано методом show()
    if(vScroll!=0)
    {
        editorShowText->setTextVerticalScroll(vScroll);
    }

    this->saveOpenWindows();
}


// Обновление содержимого открепляемого окна, содержащего запись с указанным ID.
// Несохраненные правки окна не затираются, инициатор сейва себя не перечитывает
void EditorShowTextDispatcher::updateWindow(const QString &noteId, const QObject *initiator)
{
    if( !mWindowsList.contains( noteId ) )
        return;

    EditorShowText *win=mWindowsList[noteId].data();

    if(win==nullptr)
    {
        criticalError("Incorrect pointer in mWindowsList");
        return;
    }

    if(win->editor()==nullptr)
        return;

    // Сейв инициатора: свое окно не перечитывает, файл и так свежий
    if(initiator!=nullptr && win->editor()==initiator)
        return;

    // В окне есть несохраненные правки: чужим сейвом не затираются,
    // их сейв позже перезапишет файл (last-writer-wins)
    if(win->isModified())
        return;

    // Выясняется ссылка на модель дерева данных
    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    // Выясняется ссылка на объект записи (заголовок мог поменяться)
    Record *note=dataModel->getRecord(noteId);

    if(note==nullptr)
        return;

    // Курсор и прокрутка переживают перечитывание
    const int cursorPos=win->cursorPosition();
    const int scrollPos=win->getTextVerticalScroll();

    win->editor()->loadTextarea(); // Текст окна
    win->setWindowTitle( note->getField("name") ); // Заголовок окна

    win->setCursorPosition(cursorPos);
    win->setTextVerticalScroll(scrollPos);
}


// Обновление содержимого всех открепляемых окон
void EditorShowTextDispatcher::updateAllWindows()
{
    for( auto noteId : mWindowsList.keys() )
    {
        this->updateWindow( noteId );
    }
}


// После сейва открепленного окна: индексы, статистика и главное окно.
// Состав действий один в один как MainWindow::saveTextarea, кроме
// истории обхода (курсор чужого окна там не нужен)
void EditorShowTextDispatcher::refreshAfterDetachedSave(const QString &noteId)
{
    if(noteId.isEmpty())
        return;

    // Текст на диске обновился: перепарсить исходящие ссылки записи
    BacklinkIndex::instance().updateSource(noteId);

    MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

    // Текст мог принести/убрать картинки и ссылки: статистика записи
    // и отложенные публикации (как при сейве главного окна)
    if(mainWindow!=nullptr && mainWindow->treeScreen!=nullptr &&
       mainWindow->treeScreen->knowTreeModel!=nullptr)
    {
        mainWindow->treeScreen->knowTreeModel->refreshRecordStats(noteId);
        mainWindow->treeScreen->schedulePublicationsAutoUpdate();
    }

    if(mainWindow!=nullptr)
        mainWindow->updateStats();

    // Главное окно на той же записи и не правилось: подтянуть свежий
    // текст без сброса курсора. Правленое не трогается: его сейв позже
    // перезапишет файл (last-writer-wins, как везде в программе)
    Editor *mainEditor=find_object<Editor>(QStringLiteral("editorScreen"));

    if(mainEditor!=nullptr &&
       mainEditor->getMiscField(QStringLiteral("id"))==noteId &&
       !mainEditor->getTextareaModified())
    {
        const int cursorPos=mainEditor->getCursorPosition();
        const int scrollPos=mainEditor->getScrollBarPosition();

        mainEditor->loadTextarea();

        mainEditor->setCursorPosition(cursorPos);
        mainEditor->setScrollBarPosition(scrollPos);
    }
}


// Переприменить read-only всех окон при переключении глобального режима
void EditorShowTextDispatcher::refreshAllEditability(void)
{
    for( auto widgetWindow : mWindowsList )
    {
        if( !widgetWindow.isNull() )
            widgetWindow->refreshEditability();
    }
}


// Перекрасить бумагу всех открепляемых окон под текущую тему
void EditorShowTextDispatcher::applyNoteColorsToAll(void)
{
    for( auto widgetWindow : mWindowsList )
    {
        if( !widgetWindow.isNull() )
            widgetWindow->applyNoteColors();
    }
}


// Закрытие указанного окна
void EditorShowTextDispatcher::closeWindow(const QString &noteId)
{
    if( mWindowsList.contains( noteId ) )
    {
        mWindowsList[ noteId ]->close();
        mWindowsList.remove( noteId );
    }
}


// Закрытие окон по списку
// Метод сделан в виде слота, чтобы он мог выполняться в отдельном потоке
// Так как списки могут быть длинными
void EditorShowTextDispatcher::closeWindowByIdVector(const QVector<QString> &ids)
{
    for( auto id : ids )
    {
        this->closeWindow(id);
    }
}


// Закрытие окон по списку
// Метод сделан в виде слота, чтобы он мог выполняться в отдельном потоке
// Так как списки могут быть длинными
void EditorShowTextDispatcher::closeWindowByIdList(const QStringList &ids)
{
    for( auto id : ids )
    {
        this->closeWindow(id);
    }
}


// Закрытие окон по списку в виде множества
// Метод сделан в виде слота, чтобы он мог выполняться в отдельном потоке
// Так как списки могут быть длинными
void EditorShowTextDispatcher::closeWindowByIdSet(const QSet<QString> &ids)
{
    for( auto id : ids )
    {
        this->closeWindow(id);
    }
}


// Закрытие окон, для которых не существует записи
// Используется при синхронизации, т.к. после синхронизации может оказаться
// что открепляемое окно не связано ни с какой записью
// Метод сделан в виде слота, чтобы он мог выполняться в отдельном потоке
void EditorShowTextDispatcher::closeWindowForNonExistentRecords()
{
    // Выясняется ссылка на модель дерева данных
    KnowTreeModel *knowTreeModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

    QSharedPointer< QSet<QString> > availableIds=knowTreeModel->getAllRecordsIdList();

    for( auto noteId : mWindowsList.keys() )
    {
        if( !availableIds.data()->contains(noteId) )
        {
            this->closeWindow(noteId);
        }
    }
}


// Закрытие всех открепляемых окон
void EditorShowTextDispatcher::closeAllWindows()
{
    for( auto noteId : mWindowsList.keys() )
    {
        // Если существует connect() между
        // EditorShowText::editorShowTextClose и EditorShowTextDispatcher::onCloseWindow
        // то при выполнении метода close() открепляемого окна, должен будет вызваться слот
        // EditorShowTextDispatcher::onCloseWindow()
        // и в нем произойдет удаление окна из списка окон,
        // и новое состояние списка открепляемых окон будет сохранено в конфиг.
        // Объект окна должен будет сам удалиться, так как стоит флаг WA_DeleteOnClose
        mWindowsList[ noteId ]->close();
    }

    mWindowsList.clear();
}


// Закрытие всех открепляемых окон без сохранения списка удаляемых окон.
// Используется при закрытии программы, чтобы само закрытие открепляемых окон
// не повлияло на список открепляемых окон, сохраненный при выполнении предыдущих
// действий с открепляемыми окнами
void EditorShowTextDispatcher::closeAllWindowsForExit()
{
    for( auto noteId : mWindowsList.keys() )
    {
        mWindowsList[ noteId ]->disconnect(); // Все соединения закрываемого окна отключаются
        mWindowsList[ noteId ]->close();
    }

    mWindowsList.clear();
}


// Существует ли открепляемое окно с указанной записью
bool EditorShowTextDispatcher::isWindowPresent(const QString &noteId)
{
    return mWindowsList.contains( noteId );
}


// Сохранение списка ID записей, для которых открыты открепляемые окна с их координатами
void EditorShowTextDispatcher::saveOpenWindows()
{
    QStringList windowsState;

    for( auto noteId : mWindowsList.keys() )
    {
        QString state;
        state=noteId+",";

        QRect geom=mWindowsList[noteId]->geometry();
        state+=QString::number( geom.x() )+",";
        state+=QString::number( geom.y() )+",";
        state+=QString::number( geom.width() )+",";
        state+=QString::number( geom.height() )+",";
        state+=QString::number( mWindowsList[noteId]->getTextVerticalScroll() );

        windowsState << state;
    }

    // Формат запоминаемых данных
    // "IDокна1,x1,y1,w1,h1;IDокна2,x2,y2,w2,h2" и т. д.
    mytetraConfig.setDockableWindowsState( windowsState.join(";") );
}


// Восстановление открепляемых окон с записями
void EditorShowTextDispatcher::restoreOpenWindows()
{
    // Строка со всеми окнами
    QString state=mytetraConfig.getDockableWindowsState();

    // Строка со всеми окнами разделяется на подстроки с описанием одного окна
    QStringList windowsState=state.split(";");

    // Перебираются описания окон в виде ID и координат
    for( auto window : windowsState )
    {
        if(window.trimmed().size()>0) // Если описание существует, а не пустая строка
        {
            QStringList chunks=window.split(",");

            this->createWindow(chunks[0],
                               chunks[1].toInt(),
                               chunks[2].toInt(),
                               chunks[3].toInt(),
                               chunks[4].toInt(),
                               chunks[5].toInt() );
        }
    }
}


// Установка поведения окон, которое запускается при сворачивании/разворачивании
void EditorShowTextDispatcher::switchBehavior(const QString &mode)
{
    MainWindow *mainWindow=find_object<MainWindow>("mainwindow");

    for( auto widgetWindow : mWindowsList )
    {
        int x=widgetWindow->geometry().x();
        int y=widgetWindow->geometry().y();

        // Скрепка "поверх всех окон" переживает смену поведения:
        // setWindowFlags ниже затирает все флаги, хинт возвращается обратно.
        // Окна в этот момент прячутся и показываются заново, так что
        // флаг применяется штатно
        const Qt::WindowFlags topHint=widgetWindow->windowFlags() & Qt::WindowStaysOnTopHint;

        if(mode=="single")
        {
            // Если идет переключение режима together->single,
            // и это не первое переключение (когда поведение еще не было установлено)
            if(mBehavior!=mode && mBehavior!="")
            {
                // Необходим пересчет координат

                // Ранее виджет имел родительский виджет, и его координаты были относительны родительскому
                int parentX=widgetWindow->parentWidget()->geometry().x();
                int parentY=widgetWindow->parentWidget()->geometry().y();

                // Высчитываются абсолютные координаты
                x=parentX+x;
                y=parentY+y;
            }

            widgetWindow->setParent(nullptr);
            widgetWindow->setWindowFlags(Qt::WindowFlags(Qt::Window) | topHint);
        }
        else if(mode=="together")
        {
            // Если идет переключение режима single->together,
            // и это не первое переключение (когда поведение еще не было установлено)
            if(mBehavior!=mode && mBehavior!="")
            {
                // Необходим пересчет координат

                // Ранее виджет не имел родительский виджет, и его координаты были абсолютными
                // и для вычисления новых относительных координат требуются координаты
                // устанавливаемого родительского виджета
                int parentX=mainWindow->geometry().x();
                int parentY=mainWindow->geometry().y();

                // Высчитываются новые относительные координаты
                x=x-parentX;
                y=y-parentY;
            }

            widgetWindow->setParent( mainWindow );
            widgetWindow->setWindowFlags(Qt::WindowFlags(Qt::Dialog) | topHint);
        }

        widgetWindow->move(x, y);

        widgetWindow->hide();
        widgetWindow->show();
    }

    mBehavior=mode;
}


// Обновление поведения окон согласно настройкам
// метод вызывается после разворачивания основного окна MyTetra из иконки трея
// так как при сворачивании окна MyTetra, все открепляемые окна
// теряют указатель на родителя, и, соответственно меняют свое поведение
void EditorShowTextDispatcher::restoreBehavior()
{
    this->switchBehavior( mytetraConfig.getDockableWindowsBehavior() );

    qDebug() << "Restore detached window behavior";
}


// При обнаружении события что одно из открепляемых окон закрывается
void EditorShowTextDispatcher::onCloseWindow(const QString &noteId)
{
    if( mWindowsList.contains( noteId ) )
    {
        // Все соединения закрываемого окна отключаются
        // так как код и так уже находится в обработчике события закрытия окна, и
        // больше никаких событий вызываться не должно
        mWindowsList[ noteId ]->disconnect();

        mWindowsList.remove(noteId); // Закрываемое окно убирается из списка окон
    }

    this->saveOpenWindows();
}


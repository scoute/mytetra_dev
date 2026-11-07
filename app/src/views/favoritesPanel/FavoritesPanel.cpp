#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QIcon>
#include <QBrush>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QEvent>
#include <QCursor>
#include <QSplitter>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QAbstractScrollArea>
#include <QDebug>

#include "FavoritesPanel.h"

#include "libraries/Favorites.h"
#include "libraries/ClipboardRecords.h"
#include "libraries/FixedParameters.h"
#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/Record.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/TreeScreen.h"
#include "views/tree/KnowTreeView.h"
#include "views/mainWindow/MainWindow.h"
#include "libraries/GlobalParameters.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/helpers/ObjectHelper.h"
#include "libraries/AppContext.h"
#include "models/appConfig/AppConfigUi.h"

FavoritesPanel::FavoritesPanel(Favorites *favorites, QWidget *parent) : QWidget(parent),
    treeMetadataConnected(false),
    treeScreenRef(qobject_cast<TreeScreen *>(parent)),
    m_favorites(favorites)
{
    setupUi();
    assembly();
    setupSignals();

    // Стартуем скрытыми чтобы не ломать дизайн пустой панелью
    headerLabel->setText(tr("Favorites")+QStringLiteral(" (0)"));
    hide();

    // Хранилище-синглтон уже есть: звездочки из меню и настроек
    // сами дергают пересборку
    connect(m_favorites, &Favorites::favoritesChanged,
            this,        &FavoritesPanel::onFavoritesChanged);

    // Подписка сразу через родителя: скрытая панель не получит showEvent,
    // а find_object() в конструкторе завершил бы программу (TreeScreen
    // еще без имени)
    if(treeScreenRef!=nullptr)
    {
        treeMetadataConnected=true;
        connect(treeScreenRef, &TreeScreen::treeMetadataSaved,
                this,          &FavoritesPanel::onTreeMetadataSaved);
    }
}

FavoritesPanel::~FavoritesPanel(void)
{

}

void FavoritesPanel::setupUi(void)
{
    // Шапка чтобы панель не висела одиноко: звездочка и слово
    headerIcon=new QLabel(this);
    headerIcon->setPixmap(QIcon(QStringLiteral(":/resource/pic/note_favorite.svg")).pixmap(16, 16));

    headerLabel=new QLabel(this);
    QFont headerFont=headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);

    favoritesList=new QListWidget(this);
    favoritesList->setSelectionMode(QAbstractItemView::SingleSelection);

    // Старт внутреннего перетаскивания для смены порядка.
    // Дропы не вьюхе: их перехватывает eventFilter панели, иначе
    // дефолт QListWidget вставил бы чужие данные строками. Режим
    // DragDrop чтобы вьюха рисовала индикатор сброса своих драгов
    favoritesList->setDragEnabled(true);
    favoritesList->setDragDropMode(QAbstractItemView::DragDrop);
    favoritesList->setDropIndicatorShown(true);
    favoritesList->installEventFilter(this);

    // Вертикаль Expanding: лишний простор панели (если сплиттер
    // вдруг даст больше пина) забирает белый список, а не серая
    // распорка. В компактном режиме потолок maximumHeight держит
    // список в контенте, в ручном список наливается на всю панель
    favoritesList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // Вертикальный скролл всегда виден: панель не прыгает
    // по ширине при появлении/исчезновении ползунка, место под
    // него резервируется сразу
    favoritesList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // Длинные названия обрезаются краем списка вместо горизонтальной
    // прокрутки: трек скроллбара под одной записью читается как
    // серая полоса-отрыв от дерева, а setTextElideMode в этом Qt
    // многоточия не дает (проверено пробой). Горизонтального
    // скролла нет вообще, полное имя видно в тултипе
    favoritesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    favoritesList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    // Прием записей перетаскиванием из таблицы записей:
    // события обрабатывает сама панель (включая шапку)
    setAcceptDrops(true);
}

void FavoritesPanel::assembly(void)
{
    QHBoxLayout *headerLayout=new QHBoxLayout();
    headerLayout->setContentsMargins(3, 0, 0, 0);
    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(headerLabel, 1);

    QVBoxLayout *panelLayout=new QVBoxLayout();
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->addLayout(headerLayout);

    // Без растяжения списка: высота по содержимому (AdjustToContents
    // плюс потолок), иначе пара звездочек болтается в пустом вьюпорте
    // на полпанели. Хвостовая распорка — второй поглотитель простора:
    // первым забирает сам список (Expanding), так что серой полосы
    // между списком и деревом нет. Без распорки лишек делился бы
    // поровну и пухла сама шапка.
    // Ручной драг переключает режим через setUserResized()
    panelLayout->addWidget(favoritesList);
    panelLayout->addStretch(1);

    favoritesLayout=panelLayout;
    setLayout(panelLayout);
}

// Ручной драг границы: список наливается на всю выставленную
// высоту вместо компактного содержимого. Обратно в компакт
// только перезапуском: пересборки режим не трогают
void FavoritesPanel::setUserResized(void)
{
    // Программная посадка секции (syncSplitterToContent) ручным
    // драгом не считается и компактный режим не убивает
    if(m_userResized || m_syncingSplitter)
        return;

    m_userResized=true;

    // Весь простор панели списку, распорке ничего
    favoritesLayout->setStretch(1, 1);
    favoritesLayout->setStretch(2, 0);

    // Потолок по контенту снят: видно столько строк, сколько влезло.
    // Пин панели тоже снят: высотой управляет сплиттер
    favoritesList->setMaximumHeight(QWIDGETSIZE_MAX);
    setMaximumHeight(QWIDGETSIZE_MAX);
}

void FavoritesPanel::setupSignals(void)
{
    // Одинарный клик прыгает: панель это лаунчер, а не таблица
    connect(favoritesList, &QListWidget::itemClicked,
            this,          &FavoritesPanel::onNoteClicked);

    // Дабл-клик открывает заметку тем же прыжком
    connect(favoritesList, &QListWidget::itemDoubleClicked,
            this,          &FavoritesPanel::onNoteDoubleClicked);

    favoritesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(favoritesList, &QListWidget::customContextMenuRequested,
            this,         &FavoritesPanel::onFavoritesContextMenu);

}

void FavoritesPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshFavorites();
}

// Та же ноша что тащит дерево из таблицы записей.
// Чужие данные (текст, файлы, ветки) панели не нужны
bool FavoritesPanel::canAcceptRecordsDrop(const QMimeData *mimeData) const
{
    if(mimeData==nullptr)
        return false;

    return mimeData->hasFormat(FixedParameters::appTextId+QStringLiteral("/records"));
}

bool FavoritesPanel::eventFilter(QObject *watched, QEvent *event)
{
    // Только дропы списка: остальное вьюхе и панели как обычно
    if(watched!=favoritesList || event->type()!=QEvent::Drop)
        return QWidget::eventFilter(watched, event);

    QDropEvent *dropEvent=static_cast<QDropEvent *>(event);
    QObject *source=dropEvent->source();

    // Внутренний дроп: перестановка строк. Цель - строка под курсором
    // (мимо строк - в хвост). Дефолт вьюхи не вызываем: он двигает
    // только виджет, а канон - хранилище через setOrder + пересборка
    if(source==favoritesList || source==favoritesList->viewport())
    {
        QListWidgetItem *dragged=favoritesList->currentItem();

        if(dragged==nullptr)
            return true;

        const QString id=dragged->data(Qt::UserRole).toString();

        if(id.isEmpty())
            return true;

        const QPoint viewPos=favoritesList->viewport()->mapFromGlobal(QCursor::pos());
        int toRow=favoritesList->indexAt(viewPos).row();

        QStringList order=currentListOrder();
        const int fromRow=order.indexOf(id);

        if(fromRow<0)
            return true;

        order.removeAt(fromRow);

        // Мимо строк - в хвост. Строки ниже уехавшей съехали
        // на одну вверх: цель корректируется
        if(toRow<0)
            toRow=order.size();
        else
        {
            if(fromRow<toRow)
                toRow--;

            toRow=qBound(0, toRow, order.size());
        }
        order.insert(toRow, id);

        m_favorites->setOrder(order);
        dropEvent->acceptProposedAction();

        return true;
    }

    // Внешний дроп из таблицы записей: звездочки как при броске на панель
    if(canAcceptRecordsDrop(dropEvent->mimeData()))
    {
        dropEvent->acceptProposedAction();
        this->dropEvent(dropEvent);

        return true;
    }

    return QWidget::eventFilter(watched, event);
}

QStringList FavoritesPanel::currentListOrder(void) const
{
    QStringList order;

    for(int i=0; i<favoritesList->count(); ++i)
    {
        const QString id=favoritesList->item(i)->data(Qt::UserRole).toString();

        if(!id.isEmpty())
            order << id;
    }

    return order;
}

void FavoritesPanel::moveContextNote(int delta)
{
    if(contextNoteId.isEmpty())
        return;

    QStringList order=currentListOrder();
    const int from=order.indexOf(contextNoteId);

    if(from<0)
        return;

    const int to=qBound(0, from+delta, order.size()-1);

    if(to==from)
        return;

    order.move(from, to);
    m_favorites->setOrder(order);

    contextNoteId.clear();
}

void FavoritesPanel::moveContextNoteToEdge(bool toTop)
{
    if(contextNoteId.isEmpty())
        return;

    QStringList order=currentListOrder();

    if(!order.removeOne(contextNoteId))
        return;

    if(toTop)
        order.prepend(contextNoteId);
    else
        order.append(contextNoteId);

    m_favorites->setOrder(order);

    contextNoteId.clear();
}

void FavoritesPanel::dragEnterEvent(QDragEnterEvent *event)
{
    if(canAcceptRecordsDrop(event->mimeData()))
    {
        // Копия, не перенос: запись из таблицы никуда не девается,
        // только помечается звездой
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void FavoritesPanel::dragMoveEvent(QDragMoveEvent *event)
{
    if(canAcceptRecordsDrop(event->mimeData()))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else
        event->ignore();
}

// Брошенная запись становится избранной. Список пересобирается
// сигналом хранилища, отдельное обновление не нужно
void FavoritesPanel::dropEvent(QDropEvent *event)
{
    if(!canAcceptRecordsDrop(event->mimeData()))
        return;

    const ClipboardRecords *clipboardRecords=qobject_cast<const ClipboardRecords *>(event->mimeData());

    if(clipboardRecords==nullptr)
        return;

    for(int i=0; i<clipboardRecords->getCount(); ++i)
    {
        QString id=clipboardRecords->getRecord(i).getField(QStringLiteral("id"));

        if(!id.isEmpty())
            m_favorites->setFavorite(id, true);
    }

    event->setDropAction(Qt::CopyAction);
    event->acceptProposedAction();
}

// Обновить прямые тексты новым языком. Вызывается из
// MainWindow::retranslateUi
void FavoritesPanel::retranslateUi(void)
{
    refreshFavorites();
}

// Любое сохранение метаданных пересобирает список. Видимость решает
// сам refresh: скрытая пустая панель так сама показывается при
// появлении первой звездочки
void FavoritesPanel::onTreeMetadataSaved(void)
{
    refreshFavorites();
}

// Звездочки из меню записей и настроек: список пересобирается,
// сохранение дерева не требуется
void FavoritesPanel::onFavoritesChanged(void)
{
    refreshFavorites();
}

void FavoritesPanel::onNoteClicked(QListWidgetItem *item)
{
    if(item==nullptr)
        return;

    // По мертвой строке прыгать некуда
    if(item->data(DeadRole).toBool())
        return;

    goToNote(item->data(Qt::UserRole).toString());
}

// Дабл-клик открывает заметку тем же прыжком что одинарный.
// Мертвые и пустые отсекаются внутри onNoteClicked
void FavoritesPanel::onNoteDoubleClicked(QListWidgetItem *item)
{
    onNoteClicked(item);
}

// Прыжок к заметке через готовый механизм позиционирования
void FavoritesPanel::goToNote(const QString &id)
{
    if(id.isEmpty())
        return;

    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());
    if(dataModel==nullptr)
        return;

    const QStringList path=dataModel->getRecordPath(id);
    if(path.isEmpty() || !dataModel->isItemValid(path))
        return;

    MainWindow *mainWindow=find_object<MainWindow>("mainwindow");
    if(mainWindow==nullptr)
        return;

    mainWindow->setTreeAndRecordtablePositions(path, id);
}

void FavoritesPanel::onFavoritesContextMenu(const QPoint &pos)
{
    QListWidgetItem *item=favoritesList->itemAt(pos);
    if(item==nullptr)
        return;

    contextNoteId=item->data(Qt::UserRole).toString();
    if(contextNoteId.isEmpty())
        return;

    // Порядок правится только в сайдкаре: в xml-режиме канон -
    // номера favor в mytetra.xml, их не трогаем
    const bool reorderable=(m_favorites->storage()==Favorites::StorageSidecar);
    const int row=favoritesList->row(item);
    const int lastRow=favoritesList->count()-1;

    QMenu menu(this);

    // Для мертвой строки прыжка нет, только убрать
    QAction *goAction=nullptr;

    if(!item->data(DeadRole).toBool())
        goAction=menu.addAction(tr("Go to note"));

    QAction *upAction=nullptr;
    QAction *downAction=nullptr;
    QAction *topAction=nullptr;
    QAction *bottomAction=nullptr;

    if(reorderable && favoritesList->count()>1)
    {
        menu.addSeparator();
        upAction=menu.addAction(tr("Move up"));
        downAction=menu.addAction(tr("Move down"));
        topAction=menu.addAction(tr("Move to top"));
        bottomAction=menu.addAction(tr("Move to bottom"));

        upAction->setEnabled(row>0);
        downAction->setEnabled(row<lastRow);
        topAction->setEnabled(row>0);
        bottomAction->setEnabled(row<lastRow);
    }

    menu.addSeparator();
    QAction *removeAction=menu.addAction(tr("Remove from favorites"));

    QAction *chosen=menu.exec(favoritesList->viewport()->mapToGlobal(pos));
    if(chosen==nullptr)
    {
        contextNoteId.clear();
        return;
    }

    if(chosen==goAction)
        goToNote(contextNoteId);
    else if(chosen==upAction)
        onMoveFavoriteUp();
    else if(chosen==downAction)
        onMoveFavoriteDown();
    else if(chosen==topAction)
        onMoveFavoriteToTop();
    else if(chosen==bottomAction)
        onMoveFavoriteToBottom();
    else if(chosen==removeAction)
        onRemoveFavorite();
    else
        contextNoteId.clear();
}

void FavoritesPanel::onRemoveFavorite(void)
{
    if(contextNoteId.isEmpty())
        return;

    // Хранилище само сохраняется и дергает пересборку сигналом
    m_favorites->setFavorite(contextNoteId, false);

    contextNoteId.clear();
}

void FavoritesPanel::onMoveFavoriteUp(void)
{
    moveContextNote(-1);
}

void FavoritesPanel::onMoveFavoriteDown(void)
{
    moveContextNote(1);
}

void FavoritesPanel::onMoveFavoriteToTop(void)
{
    moveContextNoteToEdge(true);
}

void FavoritesPanel::onMoveFavoriteToBottom(void)
{
    moveContextNoteToEdge(false);
}

// Высота строки списка. Намеренно от шрифта, а не от живой
// строки: иначе пол прыгает в момент добавления первой записи
// (эстимейт по шрифту и реальная строка почти никогда не равны,
// панель ужимается, а сплиттер за ужимкой не следует и остается
// зазор). Шрифт стабилен весь сеанс, пол не дергается
int FavoritesPanel::favoritesRowHeight(void) const
{
    return favoritesList->fontMetrics().height()+2;
}

// Пол панели: шапка плюс две строки списка. Сплиттер уважает
// minimumHeight детей, поэтому границу выше двух строк поднять
// нельзя. Пол задается и списку тоже: иначе при пустом списке его
// maximumHeight-под-контент схлопывает вьюпорт в ноль, место
// уходит серой распорке и панель выглядит неинициализированной
void FavoritesPanel::updateMinimumHeight(void)
{
    int listMin=favoritesList->frameWidth()*2+favoritesRowHeight()*2;
    favoritesList->setMinimumHeight(listMin);

    int headerHeight=headerLabel->sizeHint().height();
    int spacing=(favoritesLayout!=nullptr) ? favoritesLayout->spacing() : 0;

    setMinimumHeight(headerHeight+spacing+listMin);
}

// Высота списка ровно по строкам с потолком 180: пустого вьюпорта
// не бывает ни при каком размере панели. Пол выступает полом
// и для максимума: maximumHeight ниже minimumHeight это конфликт
// ограничений, от него вьюпорт серый и строки съезжают до первого
// драга. В ручном режиме после драга потолок снят: список наливается
// на всю панель
void FavoritesPanel::updateListHeight(void)
{
    if(m_userResized)
        return;

    int listHeight=favoritesList->frameWidth()*2;

    if(favoritesList->count()>0)
        listHeight+=favoritesRowHeight()*favoritesList->count();

    favoritesList->setMaximumHeight(qMin(qMax(listHeight, favoritesList->minimumHeight()), 180));
}

// Высота панели ровно под контент: шапка плюс высота списка.
// Иначе лишний простор забирает хвостовая распорка и между
// списком и деревом висит серая полоса (видно при добавлении
// первых записей). В ручном режиме после драга пин снят:
// высотой управляет пользователь через сплиттер
void FavoritesPanel::updatePanelHeight(void)
{
    if(m_userResized)
        return;

    int headerHeight=headerLabel->sizeHint().height();
    int spacing=(favoritesLayout!=nullptr) ? favoritesLayout->spacing() : 0;

    setMaximumHeight(headerHeight+spacing+favoritesList->maximumHeight());
}

// Посадка секции сплиттера ровно под панель. Сплиттер сам за
// сменой min/max не следит: после ужимки потолка секция остается
// высокой и висит зазор, после роста — наоборот. Только компактный
// режим: в ручном высотой управляет пользователь
void FavoritesPanel::syncSplitterToContent(void)
{
    if(m_userResized)
        return;

    QSplitter *splitter=qobject_cast<QSplitter *>(parentWidget());

    if(splitter==nullptr)
        return;

    int index=splitter->indexOf(this);

    if(index<0)
        return;

    QList<int> sizes=splitter->sizes();

    if(index>=sizes.size())
        return;

    int target=qBound(minimumHeight(), sizeHint().height(), maximumHeight());

    if(sizes.at(index)==target)
        return;

    int total=0;

    for(int v : sizes)
        total+=v;

    int othersTotal=total-sizes.at(index);
    int othersTarget=total-target;

    if(othersTarget<0)
        othersTarget=0;

    QList<int> adjusted;

    for(int i=0; i<sizes.size(); ++i)
    {
        if(i==index)
        {
            adjusted << target;
            continue;
        }

        int v=0;

        if(othersTotal>0)
            v=sizes.at(i)*othersTarget/othersTotal;

        adjusted << v;
    }

    // Программный сдвиг границы ручным драгом не считается:
    // иначе первая же посадка убила бы компактный режим
    m_syncingSplitter=true;
    splitter->setSizes(adjusted);
    m_syncingSplitter=false;
}

// Пересборка списка: id берутся из хранилища, названия и ветки
// резолвятся вживую (ветку могли переименовать или переместить).
// Мертвые id (запись не найдена: удалена или подменена база)
// показываются красным и хранятся дальше: при возврате базы
// звездочки оживают сами
void FavoritesPanel::refreshFavorites(void)
{
    // Дерево берется из захваченного указателя, а не через
    // parentWidget(): панель живет в сплиттере
    TreeScreen *treeScreen=treeScreenRef;
    if(treeScreen==nullptr)
        return;

    // Перетаскивание для смены порядка - только в сайдкаре:
    // в xml-режиме порядок задают номера favor, их не правим
    favoritesList->setDragEnabled(m_favorites->storage()==Favorites::StorageSidecar);

    if(!treeMetadataConnected)
    {
        treeMetadataConnected=true;

        connect(treeScreen, &TreeScreen::treeMetadataSaved,
                this,       &FavoritesPanel::onTreeMetadataSaved);
    }

    KnowTreeModel *dataModel=treeScreen->knowTreeModel;

    QMap<QString, QString> branches;
    QStringList dead;

    if(dataModel!=nullptr)
        for(const QString &id : m_favorites->ids())
        {
            const QStringList path=dataModel->getRecordPath(id);

            // Заметки нет в текущей базе: удалена или база подменена.
            // Строка останется красной, id из хранилища не выкидывается
            if(path.isEmpty() || !dataModel->isItemValid(path))
            {
                dead << id;
                continue;
            }

            // Зашифрованные ветки без пароля пропускаются как в поиске
            bool locked=false;

            for(const QString &branchId : path)
            {
                TreeItem *branchItem=dataModel->getItemById(branchId);

                if(branchItem!=nullptr &&
                   branchItem->getField(QStringLiteral("crypt"))==QStringLiteral("1") &&
                   AppContext::instance().globalParameters()->getCryptKey().length()==0)
                {
                    locked=true;
                    break;
                }
            }

            if(locked)
                continue;

            QStringList branchNames;

            for(const QString &branchId : path)
            {
                TreeItem *branchItem=dataModel->getItemById(branchId);

                if(branchItem!=nullptr)
                    branchNames << branchItem->getField(QStringLiteral("name"));
            }

            branches[id]=branchNames.join(QStringLiteral(" / "));
        }

    favoritesList->clear();

    if(dataModel!=nullptr)
        for(auto mapIt=branches.constBegin(); mapIt!=branches.constEnd(); ++mapIt)
        {
            Record *record=dataModel->getRecord(mapIt.key());

            if(record==nullptr)
            {
                dead << mapIt.key();
                continue;
            }

            QListWidgetItem *item=new QListWidgetItem(record->getField(QStringLiteral("name")), favoritesList);
            item->setData(Qt::UserRole, mapIt.key());

            // Тултип несет и полное имя (список его эллидирует),
            // и путь ветки
            QString toolTip=record->getField(QStringLiteral("name"));

            if(!mapIt.value().isEmpty())
                toolTip+=QStringLiteral("\n")+mapIt.value();

            item->setToolTip(toolTip);
        }

    // Мертвые строки в хвост красным: id вместо названия (названия
    // больше нет), прыжок для них скрыт, убрать можно как обычно
    for(const QString &id : dead)
    {
        QListWidgetItem *item=new QListWidgetItem(id, favoritesList);
        item->setData(Qt::UserRole, id);
        item->setData(DeadRole, true);
        item->setForeground(QBrush(Qt::red));
        item->setToolTip(tr("Note is missing from the base"));
    }

    // Пол панели: шапка плюс две строки, затем высота списка
    // и пин панели под контент. Порядок важен: updateListHeight
    // берет пол из minimumHeight, updatePanelHeight берет потолок
    // списка из maximumHeight
    updateMinimumHeight();
    updateListHeight();
    updatePanelHeight();
    syncSplitterToContent();

    // Шапка со счетчиком чтобы панель не висела одиноко:
    // звездочка слева уже стоит в assembly, тут слово и число
    headerLabel->setText(tr("Favorites")+QStringLiteral(" (%1)").arg(favoritesList->count()));

    // Панель видна всегда пока фича включена, даже пустая:
    // иначе непонятно куда делась звездочка из меню
    if(!AppContext::instance().appConfig()->uiSection()->get_favoritesEnabled())
        setVisible(false);
    else if(!isVisible())
        setVisible(true);
}

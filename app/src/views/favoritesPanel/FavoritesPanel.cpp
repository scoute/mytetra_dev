#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QIcon>
#include <QBrush>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
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


extern GlobalParameters globalParameters;
extern AppConfig mytetraConfig;


FavoritesPanel::FavoritesPanel(QWidget *parent) : QWidget(parent),
    treeMetadataConnected(false),
    treeScreenRef(qobject_cast<TreeScreen *>(parent))
{
    setupUi();
    assembly();
    setupSignals();

    // Стартуем скрытыми чтобы не ломать дизайн пустой панелью
    headerLabel->setText(tr("Favorites")+QStringLiteral(" (0)"));
    hide();

    // Хранилище-синглтон уже есть: звездочки из меню и настроек
    // сами дергают пересборку
    connect(&Favorites::instance(), &Favorites::favoritesChanged,
            this,                   &FavoritesPanel::onFavoritesChanged);

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
            Favorites::instance().setFavorite(id, true);
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

    QMenu menu(this);

    // Для мертвой строки прыжка нет, только убрать
    QAction *goAction=nullptr;

    if(!item->data(DeadRole).toBool())
        goAction=menu.addAction(tr("Go to note"));

    QAction *removeAction=menu.addAction(tr("Remove from favorites"));

    QAction *chosen=menu.exec(favoritesList->viewport()->mapToGlobal(pos));
    if(chosen==nullptr)
        return;

    if(chosen==goAction)
        goToNote(contextNoteId);
    else if(chosen==removeAction)
        onRemoveFavorite();
}


void FavoritesPanel::onRemoveFavorite(void)
{
    if(contextNoteId.isEmpty())
        return;

    // Хранилище само сохраняется и дергает пересборку сигналом
    Favorites::instance().setFavorite(contextNoteId, false);

    contextNoteId.clear();
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
        for(const QString &id : Favorites::instance().ids())
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
                   globalParameters.getCryptKey().length()==0)
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
    if(!mytetraConfig.get_favoritesEnabled())
        setVisible(false);
    else if(!isVisible())
        setVisible(true);
}

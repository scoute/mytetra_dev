#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QIcon>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QAbstractScrollArea>
#include <QDebug>

#include "FavoritesPanel.h"

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
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
    treeMetadataConnected(false)
{
    setupUi();
    assembly();
    setupSignals();

    // Стартуем скрытыми чтобы не ломать дизайн пустой панелью
    headerLabel->setText(tr("Favorites")+QStringLiteral(" (0)"));
    hide();

    // Подписка сразу через родителя: скрытая панель не получит showEvent,
    // а find_object() в конструкторе завершил бы программу (TreeScreen
    // еще без имени). Родитель уже есть - это и есть TreeScreen
    if(TreeScreen *treeScreen=qobject_cast<TreeScreen *>(parent))
    {
        treeMetadataConnected=true;
        connect(treeScreen, &TreeScreen::treeMetadataSaved,
                this,       &FavoritesPanel::onTreeMetadataSaved);
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

    // Высота по содержимому с потолком: пара звездочек не должна
    // отъедать полдерева
    favoritesList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    favoritesList->setMaximumHeight(180);
}


void FavoritesPanel::assembly(void)
{
    QHBoxLayout *headerLayout=new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(headerLabel, 1);

    QVBoxLayout *panelLayout=new QVBoxLayout();
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->addLayout(headerLayout);
    panelLayout->addWidget(favoritesList, 1);

    setLayout(panelLayout);
}


void FavoritesPanel::setupSignals(void)
{
    // Одинарный клик прыгает: панель это лаунчер, а не таблица
    connect(favoritesList, &QListWidget::itemClicked,
            this,          &FavoritesPanel::onNoteClicked);

    favoritesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(favoritesList, &QListWidget::customContextMenuRequested,
            this,          &FavoritesPanel::onFavoritesContextMenu);

}


void FavoritesPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshFavorites();
}


// Любое сохранение метаданных пересобирает список. Видимость решает
// сам refresh: скрытая пустая панель так сама показывается при
// появлении первой звездочки
void FavoritesPanel::onTreeMetadataSaved(void)
{
    refreshFavorites();
}


void FavoritesPanel::onNoteClicked(QListWidgetItem *item)
{
    if(item==nullptr)
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
    QAction *goAction=menu.addAction(tr("Go to note"));
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

    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());
    if(dataModel==nullptr)
        return;

    const QStringList path=dataModel->getRecordPath(contextNoteId);
    if(path.isEmpty() || !dataModel->isItemValid(path))
        return;

    TreeItem *branchItem=dataModel->getItem(path);
    if(branchItem==nullptr)
        return;

    RecordTableData *recordTable=branchItem->recordtableGetTableData();
    if(recordTable==nullptr)
        return;

    const int pos=recordTable->getPosById(contextNoteId);
    if(pos < 0)
        return;

    recordTable->setField(QStringLiteral("favorite"), QString(), pos);

    TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
    if(treeScreen!=nullptr)
        treeScreen->saveKnowTree();

    contextNoteId.clear();
}


// Пересборка списка: названия и ветки резолвятся вживую одним
// проходом по дереву. Мертвые id отбрасываются
void FavoritesPanel::refreshFavorites(void)
{
    // Панель живет внутри TreeScreen, поэтому дерево берется через
    // родителя без find_object(): в конструкторе TreeScreen еще без
    // имени и find_object() завершил бы программу
    TreeScreen *treeScreen=qobject_cast<TreeScreen *>(parentWidget());
    if(treeScreen==nullptr)
        return;

    if(!treeMetadataConnected)
    {
        treeMetadataConnected=true;

        connect(treeScreen, &TreeScreen::treeMetadataSaved,
                this,       &FavoritesPanel::onTreeMetadataSaved);
    }

    KnowTreeModel *dataModel=treeScreen->knowTreeModel;

    struct NoteInfo
    {
        QString name;
        QString branch;
    };
    QMap<QString, NoteInfo> noteInfo;
    if(dataModel!=nullptr)
    {
        struct WalkState
        {
            TreeItem *item;
            QStringList branchNames;
        };
        QList<WalkState> stack;

        const TreeItem *rootItem=dataModel->getRootItem();
        if(rootItem!=nullptr)
            for(int i=0; i<rootItem->childCount(); ++i)
            {
                TreeItem *child=rootItem->child(i);
                WalkState state;
                state.item=child;
                const QString childName=child->getField(QStringLiteral("name"));
                if(!childName.isEmpty())
                    state.branchNames << childName;
                stack << state;
            }

        while(!stack.isEmpty())
        {
            const WalkState state=stack.takeLast();
            TreeItem *item=state.item;

            if(item->getField(QStringLiteral("crypt"))!=QStringLiteral("1") ||
               globalParameters.getCryptKey().length()>0)
            {
                RecordTableData *recordTable=item->recordtableGetTableData();
                if(recordTable!=nullptr)
                    for(int row=0; row<static_cast<int>(recordTable->size()); ++row)
                    {
                        // Только избранное: флаг едет атрибутом XML,
                        // мертвые id невозможны в принципе
                        if(recordTable->getField(QStringLiteral("favorite"), row)!=QStringLiteral("1"))
                            continue;

                        NoteInfo info;
                        info.name=recordTable->getField(QStringLiteral("name"), row);
                        info.branch=state.branchNames.join(QStringLiteral(" / "));
                        noteInfo[recordTable->getField(QStringLiteral("id"), row)]=info;
                    }
            }

            for(int i=0; i<item->childCount(); ++i)
            {
                TreeItem *child=item->child(i);
                const QString childName=child->getField(QStringLiteral("name"));
                WalkState childState;
                childState.item=child;
                childState.branchNames=state.branchNames;
                if(!childName.isEmpty())
                    childState.branchNames << childName;
                stack << childState;
            }
        }
    }

    favoritesList->clear();

    for(auto mapIt=noteInfo.constBegin(); mapIt!=noteInfo.constEnd(); ++mapIt)
    {
        QListWidgetItem *item=new QListWidgetItem(mapIt->name, favoritesList);
        item->setData(Qt::UserRole, mapIt.key());
        item->setToolTip(mapIt->branch);
    }

    // Шапка со счетчиком чтобы панель не висела одиноко:
    // звездочка слева уже стоит в assembly, тут слово и число
    headerLabel->setText(tr("Favorites")+QStringLiteral(" (%1)").arg(noteInfo.size()));

    // Пустая панель прячется совсем чтобы не ломать дизайн дерева.
    // Показ тоже отсюда: первая звездочка сама выводит панель
    if(!mytetraConfig.get_favoritesEnabled() || noteInfo.isEmpty())
        setVisible(false);
    else if(!isVisible())
        setVisible(true);
}

#include <QListWidget>
#include <QMenu>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QDebug>

#include "FavoritesPanel.h"

#include "models/tree/KnowTreeModel.h"
#include "models/tree/TreeItem.h"
#include "models/recordTable/RecordTableData.h"
#include "views/tree/TreeScreen.h"
#include "views/tree/KnowTreeView.h"
#include "views/mainWindow/MainWindow.h"
#include "libraries/GlobalParameters.h"
#include "libraries/helpers/ObjectHelper.h"


extern GlobalParameters globalParameters;


FavoritesPanel::FavoritesPanel(QWidget *parent) : QWidget(parent),
    treeMetadataConnected(false)
{
    setupUi();
    assembly();
    setupSignals();

    refreshFavorites();
}


FavoritesPanel::~FavoritesPanel(void)
{

}


void FavoritesPanel::setupUi(void)
{
    favoritesList=new QListWidget(this);
    favoritesList->setSelectionMode(QAbstractItemView::SingleSelection);
}


void FavoritesPanel::assembly(void)
{
    QVBoxLayout *panelLayout=new QVBoxLayout();
    panelLayout->setContentsMargins(0, 0, 0, 0);
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


// Удаление и переименование чистят список при следующей пересборке
void FavoritesPanel::onTreeMetadataSaved(void)
{
    if(isVisible())
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
    // Подписка на изменения дерева делается один раз и только когда
    // дерево уже собрано: иначе find_object() на отсутствующем объекте
    // завершает программу
    if(!treeMetadataConnected)
    {
        TreeScreen *treeScreen=find_object<TreeScreen>("treeScreen");
        if(treeScreen==nullptr)
            return;

        treeMetadataConnected=true;

        connect(treeScreen, &TreeScreen::treeMetadataSaved,
                this,       &FavoritesPanel::onTreeMetadataSaved);
    }

    KnowTreeModel *dataModel=static_cast<KnowTreeModel*>(find_object<KnowTreeView>("knowTreeView")->model());

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
}

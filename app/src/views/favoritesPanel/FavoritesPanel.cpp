#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QIcon>
#include <QBrush>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QAbstractScrollArea>
#include <QDebug>

#include "FavoritesPanel.h"

#include "libraries/Favorites.h"
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

    // Высота по содержимому с потолком: пара звездочек не должна
    // отъедать полдерева
    favoritesList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    favoritesList->setMaximumHeight(180);
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
            this,         &FavoritesPanel::onFavoritesContextMenu);

}


void FavoritesPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    refreshFavorites();
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
            item->setToolTip(mapIt.value());
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

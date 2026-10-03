#include <QAbstractButton>
#include <QAction>
#include <QDebug>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "SubscriptionPanel.h"

#include "models/subscription/SubscriptionRegistry.h"
#include "models/teamProfile/TeamProfile.h"
#include "libraries/BranchPublisher.h"
#include "libraries/GitWrapper.h"
#include "libraries/SubscriptionImportEngine.h"
#include "libraries/SharedDirWatcher.h"

extern TeamProfile teamProfile;
extern SubscriptionRegistry subscriptionRegistry;
extern SharedDirWatcher sharedDirWatcher;


// Роли данных в элементах панели
enum SubscriptionItemRole
{
    ItemRoleBranchId=Qt::UserRole+1,        // Машинный ключ публикации (branchId)
    ItemRolePublicationDir=Qt::UserRole+2,  // Каталог публикации
    ItemRoleSubscribed=Qt::UserRole+3,      // Признак подписки (bool)
    ItemRoleOwn=Qt::UserRole+4              // Собственная публикация владельца (bool)
};


SubscriptionPanel::SubscriptionPanel(QWidget *parent) : QWidget(parent)
{
    // Заголовок панели
    QLabel *caption=new QLabel(tr("Subscriptions"), this);
    QFont captionFont=caption->font();
    captionFont.setBold(true);
    caption->setFont(captionFont);

    // Дерево подписок
    subscriptionTree=new QTreeWidget(this);
    subscriptionTree->setObjectName("subscriptionTree");
    subscriptionTree->setHeaderHidden(true);
    subscriptionTree->setContextMenuPolicy(Qt::CustomContextMenu);
    subscriptionTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    subscriptionTree->setMinimumHeight(60);

    connect(subscriptionTree, &QTreeWidget::customContextMenuRequested,
            this,             &SubscriptionPanel::onCustomContextMenuRequested);
    connect(subscriptionTree, &QTreeWidget::itemActivated,
            this,             &SubscriptionPanel::onItemActivated);

    // Обновление панели при изменении реестра подписок из любого места
    connect(&subscriptionRegistry, &SubscriptionRegistry::subscriptionsChanged,
            this,                  &SubscriptionPanel::refresh);

    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->setContentsMargins(2,0,0,0);
    layout->setSpacing(2);
    layout->addWidget(caption);
    layout->addWidget(subscriptionTree);
}


SubscriptionPanel::~SubscriptionPanel()
{
}


void SubscriptionPanel::refresh(void)
{
    qDebug() << "SubscriptionPanel::refresh invoked";
    rebuildContent();
}


// Перестроение дерева панели по реестру подписок и каталогу обмена
void SubscriptionPanel::rebuildContent(void)
{
    subscriptionTree->clear();

    QString sharedDir=teamProfile.getSharedDir();

    // Группа «Подписки»
    QTreeWidgetItem *subscribedGroup=new QTreeWidgetItem(subscriptionTree);
    subscribedGroup->setText(0, tr("Subscriptions"));

    // Группа «Мои публикации» (read-only, без действий)
    QTreeWidgetItem *myGroup=new QTreeWidgetItem(subscriptionTree);
    myGroup->setText(0, tr("My publications"));

    // Группа «Доступно»
    QTreeWidgetItem *availableGroup=new QTreeWidgetItem(subscriptionTree);
    availableGroup->setText(0, tr("Available"));

    if(sharedDir.isEmpty())
    {
        QTreeWidgetItem *hint=new QTreeWidgetItem(subscribedGroup);
        hint->setText(0, tr("(shared directory is not set)"));
        hint->setFlags(Qt::ItemIsEnabled);
        subscriptionTree->expandAll();
        return;
    }

    // Список всех публикаций в общем каталоге
    QList<PublicationMeta> publications=BranchPublisher::listPublications(sharedDir);

    bool isOwnPublic=false;
    bool hasAny=false;
    for(int i=0; i<publications.size(); ++i)
    {
        const PublicationMeta &meta=publications.at(i);
        if(meta.branchId.isEmpty())
            continue;

        bool isOwn=!teamProfile.getTeamId().isEmpty()
                   && meta.ownerId==teamProfile.getTeamId();
        bool isSubscribed=subscriptionRegistry.isSubscribed(meta.branchId);

        if(isOwn)
        {
            // Свои публикации — отдельная группа «Мои публикации» в режиме
            // только чтения: никакого импорта, чтобы не зацикливать обмен.
            // Если на свою публикацию осталась устаревшая подписка —
            // показываем её в группе «Подписки», чтобы можно было отписаться.
            QTreeWidgetItem *ownItem=createPublicationItem(
                                        isSubscribed ? subscribedGroup : myGroup,
                                        meta.branchId,
                                        meta.title,
                                        meta.ownerName,
                                        meta.dirPath,
                                        isSubscribed,
                                        isOwn);
            if(ownItem)
                isOwnPublic=true;
            continue;
        }

        // Чужие публикации в «Доступно» показываются всегда
        createPublicationItem(isSubscribed ? subscribedGroup : availableGroup,
                              meta.branchId,
                              meta.title,
                              meta.ownerName,
                              meta.dirPath,
                              isSubscribed);
        hasAny=true;
    }

    if(!hasAny)
    {
        QTreeWidgetItem *hint=new QTreeWidgetItem(subscribedGroup);
        hint->setText(0, tr("(no publications — check that Syncthing syncs the sync/ folder)"));
        hint->setToolTip(0, tr("On every device, share only the sync/ folder inside the shared "
                               "directory. If publications do not appear, verify the Syncthing "
                               "folder path and that the sync is complete."));
        hint->setFlags(Qt::ItemIsEnabled);
    }

    subscriptionTree->expandAll();

    qDebug() << "SubscriptionPanel::rebuildContent: publications in shared dir" << sharedDir
             << ", subscribed:" << subscriptionRegistry.listSubscriptionKeys().size()
             << ", own publications:" << isOwnPublic;
}


// Элемент публикации для группы
QTreeWidgetItem *SubscriptionPanel::createPublicationItem(QTreeWidgetItem *groupItem,
                                                         const QString &branchId,
                                                         const QString &title,
                                                         const QString &ownerName,
                                                         const QString &publicationDir,
                                                         bool isSubscribed,
                                                         bool isOwn)
{
    QString display=title;
    if(!ownerName.isEmpty())
        display+=tr(" — %1").arg(ownerName);

    // Для собственных публикаций маркер «*» и жирное выделение не нужны:
    // они не имеют базовой точки, с ними нельзя ничего делать
    bool hasUpdate=false;
    bool waitingData=false;
    if(isSubscribed && !isOwn)
    {
        // Детект обновления по диджестам (без git): манифест-снимок не задан
        // (ни разу не просмотрено) либо publishVersion ушёл вперёд.
        // publishVersion меняется только при фактических обновлениях контента
        // (пустые обновления не коммитятся и версию не трогают)
        const SubscriptionRecord record=subscriptionRegistry.getSubscription(branchId);
        const int currentVersion=BranchPublisher::readPublication(publicationDir).publishVersion;

        hasUpdate=record.baselineSnapshot.isEmpty()
                  || currentVersion!=record.baselinePublishVersion;

        if(hasUpdate)
            display+=tr(" *");

        // Данные ещё не доехали полностью: манифест актуальной версии
        // не появился/не совпал (дёшево — по заголовку манифеста, без прохода
        // по файлам; legacy per-branch без манифеста — штатно, не ожидание).
        // Статус обновляется при каждом обновлении панели
        if(hasUpdate && currentVersion>0)
        {
            waitingData=BranchPublisher::isPublicationDataPending(publicationDir,
                                                                 currentVersion);
            if(waitingData)
                display+=tr(" (...");
        }
    }

    QTreeWidgetItem *item=new QTreeWidgetItem(groupItem);
    item->setText(0, display);
    item->setData(0, ItemRoleBranchId, branchId);
    item->setData(0, ItemRolePublicationDir, publicationDir);
    item->setData(0, ItemRoleSubscribed, isSubscribed);
    item->setData(0, ItemRoleOwn, isOwn);
    item->setToolTip(0, publicationDir
                     + (waitingData
                        ? tr("\nОбновление в пути — данные ещё не доехали полностью. "
                             "Проверьте, что Syncthing синхронизирует папку sync/ и сошёлся.")
                        : QString()));

    if(isOwn)
        item->setForeground(0, QBrush(Qt::gray));

    // Подписки с обновлениями выделяются жирным шрифтом
    if(hasUpdate)
    {
        QFont itemFont=item->font(0);
        itemFont.setBold(true);
        item->setFont(0, itemFont);
    }

    return item;
}


// Контекстное меню элемента панели
void SubscriptionPanel::onCustomContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    QTreeWidgetItem *item=subscriptionTree->itemAt(pos);

    // Чуждая подписка (лист не-своего): журнальные операции бессмысленны —
    // local/ содержит только свои публикации, а восстановление чужого
    // из журнала воскресило бы даже отозванное владельцем (борьба
    // с Syncthing). Блок журнала ниже таким элементам не показывается
    const bool isLeaf=item && item->childCount()==0;
    const bool isGroupHeader=item && item->childCount()>0;
    const bool isOwnItem=isLeaf && item->data(0, ItemRoleOwn).toBool();
    const bool isForeignItem=isLeaf && !item->data(0, ItemRoleBranchId).toString().isEmpty()
                             && !isOwnItem;

    if(isLeaf)
    {
        QString branchId=item->data(0, ItemRoleBranchId).toString();
        if(branchId.isEmpty())
            return;

        QString publicationDir=item->data(0, ItemRolePublicationDir).toString();
        bool isSubscribed=item->data(0, ItemRoleSubscribed).toBool();
        bool isOwn=item->data(0, ItemRoleOwn).toBool();

        // Собственные публикации — только режим чтения: никакого импорта,
        // чтобы не зациклить обмен. При устаревшей подписке — только отписка.
        if(isSubscribed && !isOwn)
        {
            menu.addAction(tr("Unsubscribe"), this,
                           [this, branchId](){ unsubscribeFrom(branchId); });
            menu.addAction(tr("Mark as viewed"), this,
                           [this, branchId, publicationDir](){ markAsViewed(branchId, publicationDir); });
            menu.addSeparator();

            // Просмотр среза подписки (только чтение)
            menu.addAction(tr("Open slice..."), this,
                           [this, branchId, publicationDir]()
                           { emit openSliceRequested(branchId, publicationDir); });

            // Что изменилось + выборочный импорт (Фаза 4)
            menu.addAction(tr("Show changes / Import..."), this,
                           [this, branchId, publicationDir]()
                           { emit showChangesRequested(branchId, publicationDir); });

            menu.addSeparator();
        }
        else if(isOwn)
        {
            // Своя публикация: дожать обновление вручную (тот же поток,
            // что пункт дерева Force branch update / recovery).
            // Импорта тут нет и не будет — только чтение.
            menu.addAction(tr("Force branch update / recovery..."), this,
                           [this, branchId]()
                           { emit forceUpdatePublicationRequested(branchId); });

            // Отзыв собственной публикации (тот же поток, что пункт
            // дерева Revoke publication: диалог, корзина, журнал, лог)
            menu.addAction(tr("Revoke publication..."), this,
                           [this, branchId]()
                           { emit revokePublicationRequested(branchId); });
            menu.addSeparator();

            // Устаревшая подписка на собственный каталог: позволяем отписаться
            if(isSubscribed)
            {
                menu.addAction(tr("Unsubscribe"), this,
                               [this, branchId](){ unsubscribeFrom(branchId); });
                menu.addSeparator();
            }
        }
        else if(!isOwn)
        {
            menu.addAction(tr("Subscribe"), this,
                           [this, branchId, publicationDir](){ subscribeTo(branchId, publicationDir); });
            menu.addSeparator();
        }

        menu.addAction(tr("Refresh list"), this, [this](){ refresh(); });
    }

    // Журнальный блок меню строится отдельно для каждого контекста:
    // пустая область (глобальные операции) и свои публикации (восстановление
    // своего). На чужих подписках и заголовках групп его нет вообще —
    // local/ содержит только своё, а восстановление чужого из журнала
    // воскресило бы даже отозванное владельцем (борьба с Syncthing)
    if(!isForeignItem && !isGroupHeader)
        addJournalMenuActions(menu);

    if(item && item->childCount()>0)
        menu.addAction(tr("Refresh list"), this, [this](){ refresh(); });

    if(menu.isEmpty())
        return;

    menu.exec(subscriptionTree->viewport()->mapToGlobal(pos));
}


// Журнальный блок контекстного меню: восстановление (local, journal)
// и обслуживание (archive). Вызывается отдельно для пустой области
// и для своих публикаций; если показать нечего — серая подсказка
// с причиной, чтобы пустое меню не выглядело сломанным
void SubscriptionPanel::addJournalMenuActions(QMenu &menu)
{
    const bool recoveryAvailable=sharedDirWatcher.isRecoveryAvailable();
    const bool localRestoreAvailable=sharedDirWatcher.isLocalRestoreAvailable();
    const int snapshotCount=sharedDirWatcher.journalSnapshotCount();

    menu.addSeparator();

    bool anyAction=false;

    // Восстановление: сначала из local/ (без git), затем из журнала
    if(localRestoreAvailable)
    {
        menu.addAction(tr("Restore sync/ from local..."), this,
                       [this]()
                       {
                           restoreFromLocal();
                       });
        anyAction=true;
    }

    if(recoveryAvailable || snapshotCount>0)
    {
        menu.addAction(tr("Restore sync/ from journal... (%1)").arg(snapshotCount), this,
                       [this, recoveryAvailable, snapshotCount]()
                       {
                           restoreFromJournal(recoveryAvailable, snapshotCount);
                       });
        anyAction=true;
    }

    // Обслуживание отделено разделителем: это не восстановление
    if(snapshotCount>1)
    {
        menu.addSeparator();
        menu.addAction(tr("Archive journal history... (%1)").arg(snapshotCount), this,
                       [this, snapshotCount]()
                       {
                           archiveJournal(snapshotCount);
                       });
        anyAction=true;
    }

    if(!anyAction)
    {
        // Нечего показать — объясняем почему, иначе меню выглядит сломанным.
        // Без git дифф/импорт/детект работают по диджестам, журнал недоступен
        QAction *hint;
        if(!GitWrapper::isGitAvailable())
            hint=menu.addAction(tr("Journal unavailable: git not found in PATH"));
        else
            hint=menu.addAction(tr("(journal is empty — nothing to restore)"));
        hint->setEnabled(false);
    }
}


// Восстановление sync/ из local-авторитета (без git, первично у владельца)
void SubscriptionPanel::restoreFromLocal(void)
{
    const int result=QMessageBox::question(
                this, tr("Restore from local"),
                tr("Зеркало sync/ будет перестроено из авторитета local/ "
                   "(только свои публикации владельца, без git).\n\n"
                   "Продолжить?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if(result!=QMessageBox::Yes)
        return;

    QString error;
    const int restoredCount=sharedDirWatcher.restoreSyncFromLocal(&error);

    if(restoredCount>0)
        QMessageBox::information(this, tr("Restore from local"),
                                 tr("Восстановлено публикаций: %1.").arg(restoredCount));
    else if(error.isEmpty())
        QMessageBox::information(this, tr("Restore from local"),
                                 tr("Восстановление не требуется."));
    else
        QMessageBox::critical(this, tr("Restore from local"),
                              tr("Не удалось восстановить из local/:\n%1").arg(error));
}


// Восстановление sync/ из единого журнала (с подтверждением пользователя)
void SubscriptionPanel::restoreFromJournal(bool recoveryAvailable, int snapshotCount)
{
    QString info=tr("В журнале %1 снимков(а) области sync/.").arg(snapshotCount);

    if(recoveryAvailable)
        info+=tr("\nОбласть sync/ пуста или повреждена — восстановление необходимо.");
    else
        info+=tr("\nОбласть sync/ содержит целостные публикации.");

    QString message=tr("Восстановить из журнала:\n"
                       "  — только недостающие на диске публикации;\n"
                       "  — всю область sync/ (заменить текущее состояние).");

    QMessageBox box;
    box.setWindowTitle(tr("Restore from journal"));
    box.setText(info+"\n\n"+message);
    box.setIcon(QMessageBox::Warning);

    QPushButton *missingButton=box.addButton(tr("Only missing"), QMessageBox::YesRole);
    QPushButton *fullButton=box.addButton(tr("Whole sync/"), QMessageBox::YesRole);
    box.addButton(QMessageBox::Cancel);

    box.exec();

    QAbstractButton *clicked=box.clickedButton();
    if(clicked==missingButton || clicked==fullButton)
    {
        const bool restoreOnlyMissing=(clicked==missingButton);
        QString error;
        if(sharedDirWatcher.restoreSyncFromJournal(restoreOnlyMissing, &error))
            QMessageBox::information(this, tr("Restore from journal"),
                                     tr("Восстановление завершено."));
        else
            QMessageBox::critical(this, tr("Restore from journal"),
                                  tr("Не удалось восстановить из журнала:\n%1").arg(error));
    }
}


// «Архивирование истории» журнала (требует согласия пользователя)
void SubscriptionPanel::archiveJournal(int snapshotCount)
{
    const int result=QMessageBox::question(
                this, tr("Archive journal history"),
                tr("История журнала содержит %1 снимков(а).\n"
                   "После архивирования текущее состояние станет единственным "
                   "снимком, а прежняя история будет удалена (необратимо).\n\n"
                   "Продолжить?").arg(snapshotCount),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if(result!=QMessageBox::Yes)
        return;

    QString error;
    if(sharedDirWatcher.archiveJournalHistory(&error))
    {
        QMessageBox::information(this, tr("Archive journal history"),
                                 tr("История журнала архивирована."));
    }
    else
    {
        QMessageBox::critical(this, tr("Archive journal history"),
                              tr("Не удалось архивировать историю:\n%1").arg(error));
    }
}


// Подписка на публикацию. Базовая точка ставится на текущий HEAD,
// чтобы подписка не считалась «с обновлениями» до появления новых версий
void SubscriptionPanel::subscribeTo(const QString &branchId, const QString &publicationDir)
{
    PublicationMeta meta=BranchPublisher::readPublication(publicationDir);

    // Защита от подписки на собственный каталог: иначе владелец начал бы
    // импортировать собственную публикацию сам себе (зацикливание обмена)
    if(!teamProfile.getTeamId().isEmpty()
       && meta.ownerId==teamProfile.getTeamId())
    {
        QMessageBox::warning(this, tr("Subscribe"),
                             tr("This is your own publication. "
                                "No need to subscribe to it."));
        return;
    }

    SubscriptionRecord record;
    record.branchId=branchId;
    record.sharedPath=publicationDir;

    record.ownerName=meta.ownerName.isEmpty() ? meta.ownerId : meta.ownerName;

    // Базовая точка — манифест-снимок текущего состояния публикации
    // (branch.xml + sha256 текстов записей; git не требуется)
    SubscriptionImportEngine engine;
    engine.setPublicationDir(publicationDir);
    QString loadError;
    if(!engine.loadPublication(&loadError))
    {
        QMessageBox::warning(this, tr("Subscribe"), loadError);
        return;
    }

    // Нельзя брать за базовую точку частично доехавшую публикацию:
    // snapshot был бы неверным, а после завершения передачи обновления
    // не распознались бы
    if(!engine.isPublicationComplete())
    {
        QMessageBox::information(this, tr("Subscribe"),
                                 tr("Публикация ещё не доехала полностью. "
                                    "Подписка станет доступна после завершения "
                                    "синхронизации."));
        return;
    }

    record.baselineSnapshot=engine.buildBaselineSnapshot();
    record.baselinePublishVersion=meta.publishVersion;

    subscriptionRegistry.addOrUpdateSubscription(record);

    refresh();

    // Первый импорт — сразу в момент подписки: срез публикации целиком
    // создаётся в основной БД (imported/<owner>/<title>), подписчик не ждёт
    // ручного «Show changes». Обработчик сам различает первый импорт
    // (нет локальной копии) и просмотр изменений
    emit showChangesRequested(branchId, publicationDir);
}


// Отмена подписки (данные в shared/ не затрагиваются)
void SubscriptionPanel::unsubscribeFrom(const QString &branchId)
{
    subscriptionRegistry.removeSubscription(branchId);

    refresh();
}


// Отметка подписки как просмотренной (baseline подтягивается к текущему
// состоянию публикации через манифест-снимок, без git)
void SubscriptionPanel::markAsViewed(const QString &branchId, const QString &publicationDir)
{
    SubscriptionImportEngine engine;
    engine.setPublicationDir(publicationDir);

    QString loadError;
    if(!engine.loadPublication(&loadError))
    {
        QMessageBox::warning(this, tr("Mark as viewed"), loadError);
        return;
    }

    // Частично доехавшую публикацию «просмотренной» помечать нельзя — иначе
    // неполный снимок станет базовой точкой, и дальнейшие обновления потеряются
    if(!engine.isPublicationComplete())
    {
        QMessageBox::information(this, tr("Mark as viewed"),
                                 tr("Публикация ещё не доехала полностью. "
                                    "Отметить просмотренным можно после завершения "
                                    "синхронизации."));
        return;
    }

    const int currentVersion=BranchPublisher::readPublication(publicationDir).publishVersion;
    subscriptionRegistry.setBaselineState(branchId, engine.buildBaselineSnapshot(),
                                          currentVersion);

    refresh();
}


// Двойной клик или Enter по элементу панели:
//   подписка — открыть срез (только чтение);
//   своя публикация — перейти к исходной ветке в дереве разделов
void SubscriptionPanel::onItemActivated(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)

    if(!item || item->childCount()>0)
        return;

    QString branchId=item->data(0, ItemRoleBranchId).toString();
    if(branchId.isEmpty())
        return;

    // Своя публикация — позиционируем курсор на исходную ветку в базе
    if(item->data(0, ItemRoleOwn).toBool())
    {
        emit focusLocalBranchRequested(branchId);
        return;
    }

    bool isSubscribed=item->data(0, ItemRoleSubscribed).toBool();
    if(!isSubscribed)
        return;

    QString publicationDir=item->data(0, ItemRolePublicationDir).toString();

    emit openSliceRequested(branchId, publicationDir);
}
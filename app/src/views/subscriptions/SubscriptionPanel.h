#ifndef _SUBSCRIPTIONPANEL_H_
#define _SUBSCRIPTIONPANEL_H_

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QMenu;

// Панель подписок в левой колонке окна под деревом разделов.
//
// Содержит три группы:
//   «Подписки»        — публикации, на которые оформлена подписка (subscriptions.ini)
//   «Мои публикации»  — собственные публикации владельца (read-only, без импорта)
//   «Доступно»        — остальные чужие публикации, найденные в общем каталоге shared/
//
// Панель самостоятельно читает реестр подписок (SubscriptionRegistry)
// и сканирует каталог обмена (BranchPublisher::listPublications).
// Признак «есть обновления» у подписки определяется по диджестам:
// наличие манифест-снимка baselineSnapshot и сравнение publishVersion
// публикации с сохранённым (git не требуется, ADR-015, раздел 7.2).
//
// Управление подпиской (подписаться/отписаться/отметить просмотренным)
// делается через контекстное меню прямо в панели. На собственные
// публикации подписаться нельзя (защита от зацикливания обмена) —
// они показываются только для контроля.

class SubscriptionPanel : public QWidget
{
    Q_OBJECT

public:
    SubscriptionPanel(QWidget *parent=nullptr);
    ~SubscriptionPanel();

    // Полное обновление содержимого панели (реестр + сканирование)
    void refresh(void);

    // Восстановление sync/ из журнала (контекстное меню и авто-предложение)
    void restoreFromJournal(bool recoveryAvailable, int snapshotCount);

    // Восстановление sync/ из local-авторитета (без git, первично у владельца)
    void restoreFromLocal(void);

    // «Архивирование истории» журнала (контекстное меню панели,
    // предупреждение о размере из TreeScreen)
    void archiveJournal(int snapshotCount);

signals:
    // Пользователь хочет открыть срез подписки (только чтение, Фаза 3.5)
    void openSliceRequested(const QString &branchId, const QString &publicationDir);

    // Пользователь хочет увидеть изменения и импортировать выбранное (Фаза 4)
    void showChangesRequested(const QString &branchId, const QString &publicationDir);

    // Двойной клик по собственной публикации: перейти к исходной ветке в базе
    void focusLocalBranchRequested(const QString &branchId);

    // Принудительное обновление собственной публикации из панели
    // «Мои публикации» (тот же поток, что пункт дерева Force branch update)
    void forceUpdatePublicationRequested(const QString &branchId);

    // Отзыв собственной публикации из панели «Мои публикации»
    // (тот же поток, что пункт дерева Revoke publication)
    void revokePublicationRequested(const QString &branchId);

private slots:
    void onCustomContextMenuRequested(const QPoint &pos);
    void onItemActivated(QTreeWidgetItem *item, int column);

private:
    // Перестроение дерева панели по реестру и каталогу обмена
    void rebuildContent(void);

    // Элемент публикации для группы
    QTreeWidgetItem *createPublicationItem(QTreeWidgetItem *groupItem,
                                           const QString &branchId,
                                           const QString &title,
                                           const QString &ownerName,
                                           const QString &publicationDir,
                                           bool isSubscribed,
                                           bool isOwn=false);

    // Действия контекстного меню
    void subscribeTo(const QString &branchId, const QString &publicationDir);
    void unsubscribeFrom(const QString &branchId);
    void markAsViewed(const QString &branchId, const QString &publicationDir);

    // Журнальный блок контекстного меню (восстановление + архив).
    // Отдельный вызов для пустой области и своих публикаций;
    // при пустом журнале — серая подсказка с причиной
    void addJournalMenuActions(QMenu &menu);

    QTreeWidget *subscriptionTree;
};

#endif // _SUBSCRIPTIONPANEL_H_
#ifndef _TREESCREEN_H_
#define	_TREESCREEN_H_

#include <QWidget>
#include <QFileInfo>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QModelIndex>
#include <QTimer>


class QItemSelectionModel;
class QVBoxLayout;
class QToolBar;

class KnowTreeModel;
class KnowTreeView;
class ClipboardBranch;
class FavoritesPanel;

class TreeScreen : public QWidget
{
 Q_OBJECT

public:
 TreeScreen(QWidget *parent=nullptr);
 virtual ~TreeScreen();

 KnowTreeModel *knowTreeModel;

 void saveKnowTree(void);
 bool reloadKnowTree(void);

 void updateSelectedBranch(void);

 int getFirstSelectedItemIndex(void);
 QModelIndex getCurrentItemIndex(void);
 
 QItemSelectionModel *getSelectionModel(void);

 void exportBranchToDirectory(QString exportDir);
 void importBranchFromDirectory(QString importDir);
 
 // Установка курсора на указанный элемент
 void setCursorToIndex(QModelIndex index);
 void setCursorToId(QString nodeId);
 
 void updateBranchOnScreen(const QModelIndex &index);

 // Отложенное автообновление собственных публикаций после сохранений
 // (вызывается из MainWindow::saveTextarea)
 void schedulePublicationsAutoUpdate(void);

 void setFocusToBaseWidget(void);

 // Глобальный режим только чтения: гасит мутирующие действия и DnD.
 // Копирование ветки, сворачивание и поиск разрешены
 void setReadOnly(bool state);

signals:

    void treeScreenFindInBaseClicked();

    // Сохранены метаданные дерева: заголовки веток и поля заметок.
    // Подписчики (панель истории, позже словари автодополнения)
    // обновляются по этому сигналу
    void treeMetadataSaved(void);

public slots:

    void setupShortcuts(void);

    // Обновить прямые тексты новым языком. Тексты на шорткатах
    // освежает setupShortcuts по сигналу updateWidgetShortcut
    void retranslateUi(void);

private slots:

 void expandAllSubbranch(void);
 void collapseAllSubbranch(void);
 void expandOrCollapseRecurse(QModelIndex modelIndex, bool mode);
 void insSubbranch(void);
 void insBranch(void);
 void editBranch(void);
 void setIcon(void);

 void delBranch(QString mode="delete");

 void moveUpBranch(void);
 void moveDownBranch(void);
 void cutBranch(void);
 void cancelCutBranch(void);
 bool copyBranch(void);
 void pasteBranch(void);
 void pasteSubbranch(void);

 void encryptBranch(void);
 void decryptBranch(void);

 void publishBranch(void);
 void updatePublication(void);
 void publishCurrentBranch(bool isUpdate);
 void revokePublication(void);
 void openSubscriptionSlice(const QString &branchId, const QString &publicationDir);
 void showSubscriptionChanges(const QString &branchId, const QString &publicationDir);
 void focusLocalBranchInTree(const QString &branchId);
 void forceUpdateOwnPublication(const QString &branchId);
 void revokeOwnPublication(const QString &branchId);
 void refreshPublicationState(void);
 void updatePublicationActionsState(void);
 void onSharedDirChanged(void);
 void updatePublishedBadges(void);
 void autoUpdatePublishedBranches(void);
 void onPublicationAutoUpdateTimeout(void);
 void checkJournalRecovery(void);
 void checkJournalSize(void);

 // Действия при клике на ветку дерева
 void onKnowtreeClicked(const QModelIndex &index);
 // void checkIfOneRootCryptItem(const QModelIndex &index);

 // Открытие контекстного меню
 void onCustomContextMenuRequested(const QPoint &pos);

private:

 void setupUI(void);
 void setupModels(void);
 void setupSignals(void);
 void setupActions(void);
 void assembly(void);
 
 void moveUpDownBranch(int direction);
 bool moveCheckEnable(void);

 void insBranchSmart(bool is_branch);
 void insBranchProcess(QModelIndex index, QString name, bool is_branch);

 void addBranchToClipboard(ClipboardBranch *branch_clipboard_data, QStringList path, bool is_root);

 void pasteBranchSmart(bool is_branch);

 // Вставка вырезанной ветки перемещением с сохранением идентификатора
 void pasteCutBranch(bool is_branch);

 void treeEmptyControl(void);
 void treeCryptControl(void);

 void encryptBranchItem(void);
 void decryptBranchItem(void);

 void updateLastKnowTreeData(QFileInfo fileInfo, bool isFileInfoReal);

 // Реальные действия при клике на ветку дерева
 void processKnowtreeClicked(const QModelIndex &index);

 QMap<QString, QAction *> actionList;

 QToolBar *toolsLine;

 KnowTreeView  *knowTreeView;

 // Избранное между тулбаром веток и деревом. Прячется когда пусто
 FavoritesPanel *favoritesPanel=nullptr;

 // Панель подписок SyncTetra под деревом
 class SubscriptionPanel *subscriptionPanel=nullptr;
 class PublishedBadgeDelegate *publishedBadgeDelegate=nullptr;

 // Ключи опубликованных веток для бейджей дерева
 QSet<QString> publishedBranchKeys;

 // Предложения восстановления/архива журнала — раз за сессию
 bool recoveryPromptShown=false;
 bool journalSizePromptShown=false;

 // Debounce автообновления собственных публикаций после сохранений
 QTimer *publicationAutoUpdateTimer=nullptr;

// Порог снимков журнала, после которого предлагается архивирование истории.
// Тихого автосжатия нет осознанно: удаление истории необратимо и убивает
// глубину восстановления — решение всегда за пользователем
static const int JournalArchiveSuggestSnapshots=1000;

 QVBoxLayout *treeScreenLayout;

 QDateTime lastKnowTreeModifyDateTime;
 qint64    lastKnowTreeSize;

 // bool isKnowtreeClickedWork=false;
};


#endif	// _TREESCREEN_H_


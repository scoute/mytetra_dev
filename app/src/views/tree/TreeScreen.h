#ifndef _TREESCREEN_H_
#define	_TREESCREEN_H_

#include <QWidget>
#include <QFileInfo>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QModelIndex>


class QItemSelectionModel;
class QVBoxLayout;
class QToolBar;
class QTimer;

class KnowTreeModel;
class KnowTreeView;
class ClipboardBranch;
class SubscriptionPanel;
class PublishedBadgeDelegate;

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

  void setFocusToBaseWidget(void);

  // Обновление состояния панели подписок и бейджей публикаций
  void refreshPublicationState(void);

  // Автоматическое обновление опубликованных веток в shared после сохранения
  void autoUpdatePublishedBranches(void);

  // Отложенное автообновление публикаций (debounce): правки текста записей
  // сохраняются без пересохранения дерева, поэтому обновление публикаций
  // по ним планируется таймером, а не выполняется немедленно
  void schedulePublicationsAutoUpdate(void);

 signals:

    void treeScreenFindInBaseClicked();

    // Сохранены метаданные дерева: заголовки веток и поля заметок.
    // Словари автодополнения пересобираются по этому сигналу
    void treeMetadataSaved(void);

public slots:

    void setupShortcuts(void);

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

 // Действия при клике на ветку дерева
 void onKnowtreeClicked(const QModelIndex &index);
 // void checkIfOneRootCryptItem(const QModelIndex &index);

  // Открытие контекстного меню
  void onCustomContextMenuRequested(const QPoint &pos);

  // Изменение общего каталога (появились/исчезли публикации)
  void onSharedDirChanged(void);

  // Открытие среза публикации (только чтение)
  void openSubscriptionSlice(const QString &branchId, const QString &publicationDir);

  // Просмотр изменений публикации и выборочный импорт
  void showSubscriptionChanges(const QString &branchId, const QString &publicationDir);

  // Переход к исходной ветке в базе по собственной публикации
  void focusLocalBranchInTree(const QString &branchId);

  // Принудительное обновление собственной публикации из панели
  void forceUpdateOwnPublication(const QString &branchId);

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

  // Публикация/обновление/отзыв публикации текущей ветки
  void publishBranch(void);
  void updatePublication(void);
  void revokePublication(void);
  void publishCurrentBranch(bool isUpdate);
  void updatePublicationActionsState(void);

  // Обновление набора ключей опубликованных веток и перерисовка бейджей
  void updatePublishedBadges(void);

  // Проверка доступности восстановления sync/ из журнала и предложение
  // пользователю (один раз за сессию). Вызывается при обновлении состояния
  void checkJournalRecovery(void);

  // Предупреждение о разросшемся журнале обмена (один раз за сессию):
  // порог снимков превышен — предложить архивирование истории
  void checkJournalSize(void);

  // Срабатывание отложенного автообновления публикаций
  void onPublicationAutoUpdateTimeout(void);

 void updateLastKnowTreeData(QFileInfo fileInfo, bool isFileInfoReal);

 // Реальные действия при клике на ветку дерева
 void processKnowtreeClicked(const QModelIndex &index);

 QMap<QString, QAction *> actionList;

 QToolBar *toolsLine;

  KnowTreeView  *knowTreeView;

  SubscriptionPanel *subscriptionPanel=nullptr;
  PublishedBadgeDelegate *publishedBadgeDelegate=nullptr;

  // Ключи веток, опубликованных в общий каталог (для бейджей)
  QSet<QString> publishedBranchKeys;

  // Предложение восстановления sync/ из журнала уже показывалось
  bool recoveryPromptShown=false;

  // Предупреждение о размере журнала уже показывалось
  bool journalSizePromptShown=false;

  // Debounce отложенного автообновления публикаций после правок текста
  QTimer *publicationAutoUpdateTimer=nullptr;

  QVBoxLayout *treeScreenLayout;

 QDateTime lastKnowTreeModifyDateTime;
 qint64    lastKnowTreeSize;

 // bool isKnowtreeClickedWork=false;
};


#endif	// _TREESCREEN_H_


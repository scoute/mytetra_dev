#ifndef _TREESCREEN_H_
#define	_TREESCREEN_H_

#include <QWidget>
#include <QFileInfo>
#include <QDateTime>
#include <QMap>
#include <QModelIndex>


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

 void setFocusToBaseWidget(void);

 // Глобальный режим только чтения: гасит мутирующие действия и DnD.
 // Копирование ветки, сворачивание и поиск разрешены
 void setReadOnly(bool state);

 // Порядок кнопок тулбара по умолчанию и режимы кнопок.
 // Нужно диалогу настройки порядка кнопок
 static QStringList defaultToolbarOrder(void);
 static QString toolbarItemMode(const QString &command);

 // Кнопка синхронизации: блокировка на время команды и чтение состояния.
 // MainWindow дёргает напрямую, как раньше дёргал кнопку записей
 void setSyncroEnabled(bool state);
 bool isSyncroEnabled(void) const;

 // Действие тулбара по имени. Нужно диалогу настройки порядка кнопок
 QAction *toolAction(const QString &command);

 // Перестроение тулбара из списка конфига (или дефолта).
 // Вызывается при старте и вживую из диалога настроек
 void rebuildToolbar(void);

signals:

    void treeScreenFindInBaseClicked();

    // Сохранены метаданные дерева: заголовки веток и поля заметок.
    // Подписчики (панель истории, позже словари автодополнения)
    // обновляются по этому сигналу
    void treeMetadataSaved(void);

public slots:

    void setupShortcuts(void);

    // Разблокировка кнопки синхронизации по окончании команды
    void onSyncroCommandFinishWork(void);

    // Обновить прямые тексты новым языком. Тексты на шорткатах
    // освежает setupShortcuts по сигналу updateWidgetShortcut
    void retranslateUi(void);

private slots:

 // Контекстное меню тулбара на пустом месте: единственный пункт -
 // настройка порядка и видимости кнопок
 void onToolbarContextMenu(const QPoint &pos);

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

 // Синхронизация базы (кнопка переехала с тулбара записей)
 void onSyncroClick(void);

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

 // Пружины-распоры прошлого построения: QToolBar::clear() виджеты
 // не удаляет, чистятся вручную
 QList<QWidget *> toolbarSprings;

 KnowTreeView  *knowTreeView;

 // Избранное между тулбаром веток и деревом. Прячется когда пусто
 FavoritesPanel *favoritesPanel=nullptr;

 QVBoxLayout *treeScreenLayout;

 QDateTime lastKnowTreeModifyDateTime;
 qint64    lastKnowTreeSize;

 // bool isKnowtreeClickedWork=false;
};


#endif	// _TREESCREEN_H_


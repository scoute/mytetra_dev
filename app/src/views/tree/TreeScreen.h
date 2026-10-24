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
class QLineEdit;

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

 // Строка фильтра над деревом: показать/скрыть (тумблер в меню Вид).
 // Скрытие сбрасывает текст и возвращает все ветки
 void setTreeFilterVisible(bool visible);
 
 void updateBranchOnScreen(const QModelIndex &index);

 void setFocusToBaseWidget(void);

 // Глобальный режим только чтения: гасит мутирующие действия и DnD.
 // Копирование ветки, сворачивание и поиск разрешены
 void setReadOnly(bool state);

 // Порядок кнопок тулбара по умолчанию и режимы кнопок.
 // Нужно диалогу настройки порядка кнопок
 static QStringList defaultToolbarOrder(void);
 static QString toolbarItemMode(const QString &command);

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

    // Обновить прямые тексты новым языком. Тексты на шорткатах
    // освежает setupShortcuts по сигналу updateWidgetShortcut
    void retranslateUi(void);

private slots:

 // Фильтрация дерева по подстроке имени: совпадения и их цепочки
 // предков видны, остальное прячется. Пустая строка — вернуть все
 void applyTreeFilter(const QString &text);

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

 // Рекурсия фильтра: совпадение или видимый потомок — показать,
 // предков совпадений раскрыть. Возвращает есть ли видимое в поддереве
 bool filterBranches(const QModelIndex &parent, const QString &text);

 // Снять все скрытия строк (фильтр очищен)
 void showAllBranches(const QModelIndex &parent);

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

 // Строка фильтра над деревом. Фильтр режет скрытием строк
 // (setRowHidden), а не прокси-моделью: индексы исходной модели
 // остаются целыми и весь остальной код дерева не трогается
 QLineEdit *treeFilterEdit;

 // Избранное между тулбаром веток и деревом. Прячется когда пусто
 FavoritesPanel *favoritesPanel=nullptr;

 QVBoxLayout *treeScreenLayout;

 QDateTime lastKnowTreeModifyDateTime;
 qint64    lastKnowTreeSize;

 // bool isKnowtreeClickedWork=false;
};


#endif	// _TREESCREEN_H_


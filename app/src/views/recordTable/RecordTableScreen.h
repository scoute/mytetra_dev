#ifndef RECORDTABLESCREEN_H_
#define RECORDTABLESCREEN_H_

#include <QObject>
#include <QWidget>
#include <QString>
#include <QMap>
#include <QSet>


class QHBoxLayout;
class QVBoxLayout;
class QToolBar;
class QLabel;
class QAction;

class RecordTableController;

class RecordTableScreen : public QWidget
{
 Q_OBJECT

public:
 RecordTableScreen(QWidget *parent=nullptr);
 virtual ~RecordTableScreen();

 int     getFirstSelectionPos(void);
 QString getFirstSelectionId(void);
 void    setSelectionToPos(int pos);
 void    setSelectionToId(QString id);

 void setTreePath(QString path);
 QString getTreePath(void);

 void disableAllActions(void);

 void setFocusToBaseWidget();

 // Переприменить read-only редактора (для глобального режима без смены засветки)
 void refreshEditorReadOnly(void);

 // Действия, используемые как на тулбаре, так и в контекстном меню списка записей
 QAction *actionAddNewToEnd;
 QAction *actionAddNewBefore;
 QAction *actionAddNewAfter;
 QAction *actionEditField;
 QAction *actionBlock;
 QAction *actionFavorite;

 // Звездочка текущей записи для меню (текст зависит от состояния)
 bool isCurrentRecordFavorite(void);
 QAction *actionDelete;
 QAction *actionCut;
 QAction *actionCopy;
 QAction *actionPaste;
 QAction *actionSettings;
 QAction *actionBack;
 QAction *actionFindInBase;
 QAction *actionSort;
 QAction *actionPrint;
 QAction *actionPrintPreview;
 QAction *actionCopyRecordReference;
 QAction *actionChangeHistory;
 QAction *actionSwitchSelectionMode;

public slots:

 // Обновление состояния экранных элементов согласно состоянию записи и положении зиписи в таблице
 void toolsUpdate(void);

 // Добавить кнопку во вторую линию инструментов (для внешних действий
 // вроде переключателя панели тегов)
 void addExtraToolAction(QAction *action);

 void setupShortcuts(void);

 // Обновить прямые тексты новым языком. Тексты на шорткатах
 // освежает setupShortcuts по сигналу updateWidgetShortcut
 void retranslateUi(void);

private slots:
 
 void onWalkHistoryPreviousClick(void);
 void onWalkHistoryNextClick(void);
 void onBackClick(void);
 void onCopyRecordReference(void);

private:
 QToolBar *toolsLine;
 QToolBar *extraToolsLine;

 // Внешние кнопки второй линии (переключатели панелей из MainWindow).
 // Хранятся чтобы переживать перестроение тулбара из настроек
 QList<QAction *> extraToolActions;

 // Пружины-распоры прошлого построения: QToolBar::clear() виджеты
 // не удаляет, чистятся вручную
 QList<QWidget *> toolbarSprings;

 QLabel *treePathLabel;
 QString treePath;
 
 RecordTableController *recordTableController;

 QHBoxLayout *recordTableToolsLayout;
 QVBoxLayout *recordTableScreenLayout;

 QAction *actionMoveUp;
 QAction *actionMoveDn;
 QAction *actionWalkHistoryPrevious;
 QAction *actionWalkHistoryNext;
  
 void setupUI(void);
 void setupSignals(void);
 void setupActions(void);
 void assembly(void);

 // Порядок кнопок тулбаров по умолчанию (объединение десктопного
 // и мобильного наборов; лишнее режется режимом в rebuildToolbars)
 static QStringList defaultToolbarOrder(void);
 static QStringList defaultExtraToolbarOrder(void);

 // Режим кнопки: desktop, mobile или обе (пусто - обе).
 // Скрытые shortcut-only кнопки в списки не входят, регистрируются
 // всегда. Неизвестные имена из конфига пропускаются с предупреждением
 static QString toolbarItemMode(const QString &command);
 static QSet<QString> hiddenToolbarItems(void);

 // Перестроение обоих тулбаров из списков конфига (или дефолта).
 // Вызывается при старте и вживую из диалога настроек
 void rebuildToolbars(void);

 // Действие тулбаров по имени для построения из списков
 QAction *toolActionByName(const QString &command);

 // Построение одной линии тулбара из списка
 void buildToolbarLine(QToolBar *line,
                       const QStringList &order,
                       bool isMobile);

 void toolsWidgetsUpdate();
 void editorModesUpdate();

};

#endif /* RECORDTABLESCREEN_H_ */

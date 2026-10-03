#ifndef BRANCHSLICEDIALOG_H
#define BRANCHSLICEDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>
#include <QString>

#include "libraries/BranchPublisher.h"
#include "libraries/BranchDiffEngine.h"

class QDomElement;
class QLabel;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

// Диалог просмотра среза публикации (только чтение).
//
// Показывает записи ветки из общего каталога shared/ без импорта в личную БД:
//   слева — дерево веток и записей, построенное по branch.xml;
//   справа — текст выбранной записи, читаемый напрямую из records/<dir>/<file>.
//
// Вся информация берётся только из каталога публикации. Возможность
// создания/редактирования/удаления отсутствует: просмотр строго read-only.
//
// Если передан список изменений с базовой точки подписки — записи и ветки,
// затронутые изменениями, подсвечиваются (новые — зелёным, изменённые —
// синим), а удалённые владельцем выносятся в красную секцию «Удалено
// владельцем» (их нет в дереве среза): diff «at a glance» без импорта.
// Полный выборочный импорт — в диалоге «Что изменилось».

class BranchSliceDialog : public QDialog
{
  Q_OBJECT

public:
  explicit BranchSliceDialog(const QString &publicationDir, QWidget *parent=nullptr,
                             const QList<DiffChange> &changes=QList<DiffChange>());

private slots:
  void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
  void setupUi(void);

  // Построение дерева записей по <node>-элементу branch.xml
  void buildTree(const QDomElement &nodeElement,
                 QTreeWidgetItem *parentBranchItem,
                 const QString &branchPath);

  // Подсветка элементов по списку изменений + сводка в заголовке
  void applyChangesHighlight(void);

  // Показ текста записи, на которую указывает элемент дерева
  void showRecordText(QTreeWidgetItem *item);

  QString publicationDir;
  PublicationMeta meta;
  QList<DiffChange> changes;

  // Индексы элементов дерева по id (для подсветки изменений)
  QMap<QString, QTreeWidgetItem*> recordItems;
  QMap<QString, QTreeWidgetItem*> branchItems;

  QLabel *infoLabel;
  QTreeWidget *recordTree;
  QTextEdit *textView;
};

#endif // BRANCHSLICEDIALOG_H
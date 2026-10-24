#ifndef CLIPBRANCHDIALOG_H
#define CLIPBRANCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>
#include <QSortFilterProxyModel>

class KnowTreeModel;
class QGridLayout;
class QRadioButton;
class QLineEdit;
class QScrollArea;
class QStackedWidget;
class QTreeView;
class QDialogButtonBox;
class QModelIndex;


// Фильтр дерева: совпадение по имени оставляет ветку, рекурсивный
// режим Qt сам показывает всю цепочку предков (полная глубина видна)
class ClipBranchFilter : public QSortFilterProxyModel
{
 public:
  explicit ClipBranchFilter(QObject *parent=nullptr);

  void setFilterText(const QString &text);

 protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

 private:
  QString m_text;
};


// Модальный вопрос клиппера "куда вставить": два вида на выбор —
// плитка недавних веток и живое дерево. Плитка — недавние по истории
// вставок (пусто — полный список как раньше), дерево — вся иерархия
// на той же модели. Фильтр режет оба вида, глубину дерева не трогает.
// Клик по плитке или двойной клик в дереве сразу вставляет.
// Отмена — пустая строка

class ClipBranchDialog : public QDialog
{
  Q_OBJECT

 public:
  explicit ClipBranchDialog(QWidget *parent=nullptr);
  virtual ~ClipBranchDialog(void);

  // Общий листинг для плитки и настроек: пары (id ветки, путь " / ").
  // Корневые + два уровня вложений. Пустой список — дерево не готово
  static QList< QPair<QString, QString> > collectBranches(KnowTreeModel *model);

  // Модальный опрос: id выбранной ветки, пустая строка при отмене.
  // Окно поднимается поверх всех (браузер в фокусе в момент хоткея)
  static QString askBranch(QWidget *parent=nullptr);

  QString selectedBranchId(void) const;

 private slots:
  void onTileClicked(void);
  void onViewModeChanged(void);
  void onFilterChanged(const QString &text);
  void onTreeDoubleClicked(const QModelIndex &index);
  void onTreeSelectionChanged(void);
  void onOkClicked(void);

 private:
  void setupUi(void);
  void setupTiles(void);
  void setupTree(KnowTreeModel *model);

  QString m_selectedId;
  QGridLayout *m_grid;
  QRadioButton *m_tilesRadio;
  QRadioButton *m_treeRadio;
  QLineEdit *m_searchEdit;
  QStackedWidget *m_stack;
  QScrollArea *m_tilesScroll;
  QTreeView *m_treeView;
  ClipBranchFilter *m_treeFilter;
  QDialogButtonBox *m_buttonBox;
};

#endif // CLIPBRANCHDIALOG_H

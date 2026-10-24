#ifndef CLIPBRANCHDIALOG_H
#define CLIPBRANCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>
#include <QSortFilterProxyModel>

class KnowTreeModel;
class TreeItem;
class QGridLayout;
class QLineEdit;
class QScrollArea;
class QTreeView;
class QDialogButtonBox;
class QLabel;
class QModelIndex;


// Фильтр дерева: совпадение по имени оставляет ветку, рекурсивный
// режим Qt сам показывает всю цепочку предков (глубина дерева не режется)
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


// Модальный вопрос клиппера "куда вставить": единый экран —
// дерево с фильтром сверху, 16 плиток снизу (последние или частые
// по настройке, пусто — полный список). Клик по плитке или двойной
// клик в дереве сразу вставляет. Отмена — пустая строка

class ClipBranchDialog : public QDialog
{
  Q_OBJECT

 public:
  explicit ClipBranchDialog(QWidget *parent=nullptr);
  virtual ~ClipBranchDialog(void);

  // Общий листинг для настроек: пары (id ветки, путь " / ").
  // Корневые + два уровня вложений. Пустой список — дерево не готово.
  // Зашифрованные без введенного пароля выкинуты: вставить туда нельзя
  static QList< QPair<QString, QString> > collectBranches(KnowTreeModel *model);

  // Ветка закрыта: шифрована сама или любой предок, а пароль не введен.
  // Проверка только для показа (списки, плитки), хранение не трогает:
  // пароль введут — ветка снова доступна
  static bool isLocked(const TreeItem *item);

  // Сколько плиток внизу опроса: 4 строки по 4
  static const int maxTiles=16;

  // Модальный опрос: id выбранной ветки, пустая строка при отмене.
  // Окно поднимается поверх всех (браузер в фокусе в момент хоткея)
  static QString askBranch(QWidget *parent=nullptr);

  QString selectedBranchId(void) const;

 private slots:
  void onTileClicked(void);
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
  QLineEdit *m_searchEdit;
  QTreeView *m_treeView;
  ClipBranchFilter *m_treeFilter;
  QLabel *m_tilesLabel;
  QScrollArea *m_tilesScroll;
  QDialogButtonBox *m_buttonBox;
};

#endif // CLIPBRANCHDIALOG_H

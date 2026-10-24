#ifndef CLIPBRANCHDIALOG_H
#define CLIPBRANCHDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class KnowTreeModel;
class QGridLayout;
class QRadioButton;
class QScrollArea;
class QStackedWidget;
class QTreeView;
class QDialogButtonBox;
class QModelIndex;


// Модальный вопрос клиппера "куда вставить": два вида на выбор —
// плитка кнопок и живое дерево. Плитка — корневые ветки с двумя
// уровнями вложений, дерево — вся иерархия на той же модели.
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
  QStackedWidget *m_stack;
  QScrollArea *m_tilesScroll;
  QTreeView *m_treeView;
  QDialogButtonBox *m_buttonBox;
};

#endif // CLIPBRANCHDIALOG_H

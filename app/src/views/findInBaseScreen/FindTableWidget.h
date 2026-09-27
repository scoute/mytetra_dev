#ifndef _FINDTABLEWIDGET_H_
#define	_FINDTABLEWIDGET_H_

#include <QWidget>
#include <QTextDocument>


class QModelIndex;
class QTableView;
class QStandardItemModel;


class FindTableWidget : public QWidget
{
  Q_OBJECT

public:

  FindTableWidget(QWidget *parent=nullptr);
  virtual ~FindTableWidget(void);

  void clearAll(void);
  void addRow(QString title, QString branchName, QString tags, QStringList path, QString recordId, int matchCount, bool isRecord);
  int  getRowCount();
  void updateColumnsWidth(void);
  void setOverdrawMessage(const QString iOverdrawMessage); // Установка надписи, которая появляется поверх виджета

  // Запрос последнего поиска и его флаги. Нужны чтобы при переходе
  // в запись сразу запускать поиск по заметке с тем же запросом
  void setLastSearch(const QString &query, QTextDocument::FindFlags flags);

private slots:

  // void selectCell(int row, int column);
  void selectCell(const QModelIndex & index);

private:

  QTableView *findTableView;
  QStandardItemModel *findTableModel;
  QString overdrawMessage;

  // Запрос и флаги последнего поиска для моста в поиск по заметке
  QString lastSearchQuery;
  QTextDocument::FindFlags lastSearchFlags;

  void setupUI(void);
  void setupModels(void);
  void setupSignals(void);
  void assembly(void);

  void paintEvent(QPaintEvent *event);
};

#endif	/* _FINDTABLEWIDGET_H_ */


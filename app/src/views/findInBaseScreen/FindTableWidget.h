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

  // Агрегация счетчиков строк веток: каждая ветка показывает суммарные
  // совпадения по всему своему поддереву (свое имя + прямые записи +
  // итоги дочерних веток). Вызывается один раз после конца поиска
  void aggregateBranchCounts(void);

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

  // Есть ли запросу совпадения в тексте записи. Проверка тем же движком
  // что подсветка полоски (QTextDocument::find с теми же флагами),
  // поэтому результат один в один совпадает с будущим "Нет совпадений".
  // Нужна чтобы не открывать мост-бар бессмысленно, когда совпало поле
  // author/url/tags, а не текст
  static bool noteTextContains(const QString &branchId,
                               const QString &recordId,
                               const QString &query,
                               QTextDocument::FindFlags flags);

  void paintEvent(QPaintEvent *event);
};

#endif	/* _FINDTABLEWIDGET_H_ */


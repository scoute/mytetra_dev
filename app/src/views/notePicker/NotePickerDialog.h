#ifndef NOTEPICKERDIALOG_H
#define NOTEPICKERDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class QLabel;
class QTimer;


// Быстрый выбор заметки для вставки ссылки.
// Живой фильтр по именам, веткам и тегам (мгновенно, без чтения файлов).
// Выдача ужата до 50 строк со счетчиком "Показано 50 из 55".
// Текущая запись исключается: ссылка на себя бессмысленна.

class NotePickerDialog : public QDialog
{
  Q_OBJECT

public:

  // excludeId не показывается в выдаче
  explicit NotePickerDialog(QWidget *parent=nullptr,
                            const QString &excludeId=QString());

  QString selectedRecordId(void) const;
  QString selectedRecordName(void) const;

  // Для стенда: синхронно применить фильтр
  void applyFilter(const QString &text);
  void setCurrentRow(int row);
  int resultCount(void) const;
  QString counterText(void) const;

private slots:

  void scheduleFilter(void);
  void rebuildResults(void);
  void onItemActivated(QTableWidgetItem *item);

private:

  struct Hit
  {
    QString id;
    QString name;
    QString branch;
    QString branchPath;
    QString tags;
  };

  QList<Hit> searchHits(const QString &query) const;
  bool isRecordSkippable(const QString &recordId) const;

  // Столбцы выдачи: имя заметки, ветка, метки
  enum Columns
  {
    ColumnName=0,
    ColumnBranch=1,
    ColumnTags=2,
    ColumnCount=3
  };

  QLineEdit *filterEdit;
  QTableWidget *resultsTable;
  QLabel *counterLabel;
  QTimer *filterTimer;

  QString excludeRecordId;

  static const int DISPLAY_LIMIT=50;
};

#endif // NOTEPICKERDIALOG_H

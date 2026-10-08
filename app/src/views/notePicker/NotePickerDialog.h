#ifndef NOTEPICKERDIALOG_H
#define NOTEPICKERDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QTimer;


// Быстрый выбор заметки для вставки ссылки.
// Живой фильтр только по именам и тегам (мгновенно, без чтения файлов).
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
  void onItemActivated(QListWidgetItem *item);

private:

  struct Hit
  {
    QString id;
    QString name;
    QString branchPath;
  };

  QList<Hit> searchHits(const QString &query) const;
  bool isRecordSkippable(const QString &recordId) const;

  QLineEdit *filterEdit;
  QListWidget *resultsList;
  QLabel *counterLabel;
  QTimer *filterTimer;

  QString excludeRecordId;

  static const int DISPLAY_LIMIT=50;
};

#endif // NOTEPICKERDIALOG_H

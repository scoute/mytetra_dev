#ifndef MCPHISTORYDIALOG_H
#define MCPHISTORYDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QList>

// Одна строка истории действий MCP. Хранится только в памяти:
// на диск не пишется, при дисконнекте список чистится
struct McpHistoryEntry
{
  qint64 msecs=0;
  QString kind;    // "read", "write" или "other"
  QString tool;
  QString summary;
};

class QComboBox;
class QListWidget;

// Н модальное окно истории действий активной MCP-сессии.
// Показывается на старт сессии, скрывается и чистится на дисконнекте
class McpHistoryDialog : public QDialog
{
  Q_OBJECT

public:
  explicit McpHistoryDialog(QWidget *parent=nullptr);
  virtual ~McpHistoryDialog();

  void retranslateUi(void);

  // Полная замена списка (фильтр применяется заново)
  void setHistory(const QList<McpHistoryEntry> &entries);

  // Сколько строк сейчас видно (для стендов)
  int visibleCount(void) const;

private slots:
  void onFilterChanged(int index);

private:
  void render(void);
  QString renderEntry(const McpHistoryEntry &entry) const;

  QList<McpHistoryEntry> m_entries;
  QString m_filter="all";

  QComboBox *m_filterBox=nullptr;
  QListWidget *m_list=nullptr;
};

#endif // MCPHISTORYDIALOG_H

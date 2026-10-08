#ifndef BACKLINKSPANEL_H
#define BACKLINKSPANEL_H

#include <QWidget>
#include <QSet>

class QLabel;
class QListWidget;
class QListWidgetItem;


// Док-панель входящих ссылок mytetra://note/ на текущую запись.
// Только входящие. Читает готовый обратный индекс за O(1),
// сама ничего не сканирует. Несуществующие источники красит
// красным, чистятся пунктом меню.

class BacklinksPanel : public QWidget
{
  Q_OBJECT

public:

  BacklinksPanel(QWidget *parent=nullptr);
  virtual ~BacklinksPanel(void);

public slots:

  void refreshBacklinks(void);

protected:

  void showEvent(QShowEvent *event) override;

private slots:

  void onItemActivated(QListWidgetItem *item);
  void onCleanupStale(void);
  void onRebuildAll(void);
  void refreshDeferred(void);

private:

  QString currentRecordId(void) const;
  void setupLazySignals(void);
  void fillList(QListWidget *list, const QSet<QString> &ids);

  QLabel *incomingHeaderLabel;
  QListWidget *incomingList;

  QLabel *outgoingHeaderLabel;
  QListWidget *outgoingList;

  bool signalsConnected=false;
};

#endif // BACKLINKSPANEL_H

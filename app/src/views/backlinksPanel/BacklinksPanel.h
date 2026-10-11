#ifndef BACKLINKSPANEL_H
#define BACKLINKSPANEL_H

#include <QWidget>
#include <QSet>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QTextDocument;


// Док-панель входящих и исходящих ссылок mytetra://note/.
// Входящие читаются из сохраненного обратного индекса за O(1).
// Исходящие текущей записи берутся из живого документа редактора,
// чтобы вставка ссылки была видна сразу, до сейва. Несуществующие
// записи красятся красным, чистятся пунктом меню.

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
  void onLiveDocumentChanged(void);
  void refreshOutgoingLive(void);

private:

  QString currentRecordId(void) const;

  // Исходящие текущей записи из живого документа (до сейва).
  // Пусто если редактор недоступен: тогда caller берет индекс
  QSet<QString> liveOutgoing(bool *ok) const;

  void setupLazySignals(void);
  void setupLiveDocument(void);
  void fillList(QListWidget *list, const QSet<QString> &ids);

  QLabel *incomingHeaderLabel;
  QListWidget *incomingList;

  QLabel *outgoingHeaderLabel;
  QListWidget *outgoingList;

  bool signalsConnected=false;

  // Живой документ редактора для исходящих до сейва.
  // Указатель стабилен: loadTextarea делает setHtml в тот же документ
  QTextDocument *liveDoc=nullptr;
  bool outgoingPending=false;
};

#endif // BACKLINKSPANEL_H

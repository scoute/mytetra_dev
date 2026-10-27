#ifndef __ATTACHTABLECONTROLLER_H__
#define __ATTACHTABLECONTROLLER_H__

#include <QObject>
#include <QModelIndexList>
#include <QItemSelectionModel>


class AttachTableView;
class AttachTableTileView;
class AttachTableModel;
class AttachTableData;
class MetaEditor;

class AttachTableController : public QObject
{
  Q_OBJECT

public:

  AttachTableController(QObject *parent, QObject *iMetaEditor );
  virtual ~AttachTableController();

  AttachTableView *getView(void);

  // Плитка и текущий видимый вид (для родителей диалогов и ширины)
  AttachTableTileView *getTileView(void);
  QWidget *currentView(void) const;

  void setAttachTableData(AttachTableData *attachTableData);
  AttachTableData *getAttachTableData();

  QList<QString> getSelectedId(void);

public slots:

  void onAddAttach(void);
  void onAddAttachFromUrl(void);
  void onAddLink(void);
  void onEditFileName(void);
  void onDeleteAttach(void);
  void onOpenAttach(void);
  void onShowAttachInfo(void);
  void onSaveAsAttach(void);

  void onSwitchToEditor(void);

  // Файлы из drag'n'drop (вид дергает напрямую, без диалога)
  void addDroppedFiles(const QStringList &files);

protected:

  void addSmart(QString attachType);
  void addFileList(QString attachType, const QStringList &files);
  bool addAttach(QString attachType, QString currFullFileName, QString currShortFileName, const QString &source=QString());
  void saveState();
  QStringList selectFilesForAdding(QString attachType);
  void saveAttachToUserPlace(QString fromFullFileName, QString toFullFileName, QString attachType, bool isAttachCrypt);
  void updateAttachListInEditor(void);

  AttachTableView *view;
  AttachTableModel *model;

  // Плитка с тем же model; выделение общее через selectionModel —
  // действия контроллера не зависят от активного вида
  AttachTableTileView *tileView;
  QItemSelectionModel *selectionModel;

  MetaEditor *metaEditor;
};

#endif // __ATTACHTABLECONTROLLER_H__

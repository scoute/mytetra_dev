#ifndef __ATTACHTABLECONTROLLER_H__
#define __ATTACHTABLECONTROLLER_H__

#include <QObject>
#include <QModelIndexList>


class AttachTableView;
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

  // Один файл из дропа в текст: тот же движок что addFileList,
  // но возвращает имена для вставки кликабельной метки.
  // Пусто в innerFileName если файл не добавился
  bool addSingleFileAsAttach(const QString &filePath,
                             QString &innerFileName,
                             QString &displayFileName);

  // Удаление аттача по внутреннему имени для движка переноса метки:
  // находит строку (прямое имя или любая схема), удаляет файл
  // и запись, сохраняет дерево и обновляет иконку/статистику
  bool removeAttachByInnerName(const QString &innerName);

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

  MetaEditor *metaEditor;
};

#endif // __ATTACHTABLECONTROLLER_H__

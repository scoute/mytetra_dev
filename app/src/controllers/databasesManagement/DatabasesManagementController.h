#ifndef DATABASESMANAGEMENTCONTROLLER_H
#define DATABASESMANAGEMENTCONTROLLER_H

#include <QObject>

class DatabasesManagementTable;
class DatabasesManagementModel;
class BacklinkIndex;

class DatabasesManagementController : public QObject
{
  Q_OBJECT

public:

  // P3: индекс внедряется (был BacklinkIndex::instance())
  DatabasesManagementController(BacklinkIndex *backlinks, QObject *parent = nullptr);
  virtual ~DatabasesManagementController();

  DatabasesManagementTable *getView(void);

public slots:

  void onSelectClicked();
  void onCreateClicked();
  void onAddClicked();
  void onEditClicked();
  void onDeleteClicked();
  void onCopyClicked();

  void onDoubleClicked(const QModelIndex &index);

protected:

  void addDatabase(const QString &dbPath,
                   const QString &trashPath);

  bool switchToDatabase(const QString &dbPath,
                        const QString &trashPath);

  DatabasesManagementTable *view;
  DatabasesManagementModel *model;

  // P3: внедренный индекс (DI вместо синглтона)
  BacklinkIndex *m_backlinks=nullptr;

};

#endif // DATABASESMANAGEMENTCONTROLLER_H

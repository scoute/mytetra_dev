#ifndef SYNCDEVICESDIALOG_H
#define SYNCDEVICESDIALOG_H

#include <QDialog>

#include "models/appConfig/AppConfig.h"

class KnowTreeModel;
class QTableWidget;
class QLabel;
class QPushButton;

// Окно Sync devices: известные пиры (кто стучался в hello / к кому ходили).
// Кнопки действий напротив каждого: Sync now, Pair, Unpair, Remove.
// Отдельное меню Sync (не Tools): синк — подсистема, а не утилита
class SyncDevicesDialog : public QDialog
{
    Q_OBJECT

public:

    explicit SyncDevicesDialog(KnowTreeModel *model, AppConfig *config,
                               const QString &tetradir,
                               QWidget *parent=nullptr);

private slots:

    void refreshList(void);
    void onSelectionChanged(void);
    void onSyncNow(void);
    void onPair(void);
    void onUnpair(void);
    void onRemove(void);

private:

    // Строка считается спаренной: секрет есть и выведен именно с ней
    bool isPaired(const AppConfig::SyncDeviceInfo &info) const;
    int currentRowId(QString &id) const;

    KnowTreeModel *model=nullptr;
    AppConfig *config=nullptr;
    QString tetradir;

    QTableWidget *table=nullptr;
    QLabel *fingerprintLabel=nullptr;
    QPushButton *syncButton=nullptr;
    QPushButton *pairButton=nullptr;
    QPushButton *unpairButton=nullptr;
    QPushButton *removeButton=nullptr;
};

#endif // SYNCDEVICESDIALOG_H

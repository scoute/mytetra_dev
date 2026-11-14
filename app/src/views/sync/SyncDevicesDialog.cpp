#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QDateTime>

#include "SyncDevicesDialog.h"
#include "libraries/sync/SyncIdentity.h"
#include "libraries/sync/SyncSession.h"
#include "libraries/sync/SyncClient.h"
#include "models/tree/KnowTreeModel.h"
#include "models/appConfig/AppConfig.h"


SyncDevicesDialog::SyncDevicesDialog(KnowTreeModel *model, AppConfig *config,
                                     const QString &tetradir, QWidget *parent)
    : QDialog(parent),
      model(model),
      config(config),
      tetradir(tetradir)
{
    setWindowTitle(tr("Sync devices"));
    resize(640, 380);

    QVBoxLayout *layout=new QVBoxLayout(this);

    table=new QTableWidget(this);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(QStringList()
        << tr("Name") << tr("Address") << tr("Last seen")
        << tr("Paired") << tr("Device ID"));
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnHidden(4, true); // id держим в строке, не показываем
    layout->addWidget(table);

    fingerprintLabel=new QLabel(this);
    fingerprintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fingerprintLabel->setWordWrap(true);
    layout->addWidget(fingerprintLabel);

    QHBoxLayout *buttons=new QHBoxLayout();
    syncButton=new QPushButton(tr("Sync now"), this);
    pairButton=new QPushButton(tr("Pair..."), this);
    unpairButton=new QPushButton(tr("Unpair"), this);
    removeButton=new QPushButton(tr("Remove"), this);
    QPushButton *closeButton=new QPushButton(tr("Close"), this);
    buttons->addWidget(syncButton);
    buttons->addWidget(pairButton);
    buttons->addWidget(unpairButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(table, &QTableWidget::itemSelectionChanged,
            this, &SyncDevicesDialog::onSelectionChanged);
    connect(syncButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onSyncNow);
    connect(pairButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onPair);
    connect(unpairButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onUnpair);
    connect(removeButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onRemove);
    connect(closeButton, &QPushButton::clicked,
            this, &QDialog::accept);

    refreshList();
}


bool SyncDevicesDialog::isPaired(const AppConfig::SyncDeviceInfo &info) const
{
    return !config->get_syncPairingSecret().isEmpty()
        && config->get_syncPairedDeviceId()==info.id;
}


int SyncDevicesDialog::currentRowId(QString &id) const
{
    const QList<QTableWidgetSelectionRange> ranges=table->selectedRanges();

    if(ranges.isEmpty())
        return -1;

    const int row=ranges.first().topRow();
    id=table->item(row, 4)->text();
    return row;
}


void SyncDevicesDialog::refreshList(void)
{
    const QList<AppConfig::SyncDeviceInfo> devices=config->get_syncDevices();

    table->setRowCount(devices.size());

    for(int row=0; row<devices.size(); ++row)
    {
        const AppConfig::SyncDeviceInfo &info=devices.at(row);

        QTableWidgetItem *nameItem=new QTableWidgetItem(
            info.name.isEmpty() ? info.id.left(12) : info.name);
        QTableWidgetItem *addrItem=new QTableWidgetItem(
            info.ip.isEmpty() ? tr("(unknown address)")
                              : QStringLiteral("%1:%2").arg(info.ip).arg(info.port));
        QTableWidgetItem *seenItem=new QTableWidgetItem(
            info.lastSeen>0
            ? QDateTime::fromMSecsSinceEpoch(info.lastSeen)
                  .toString(QStringLiteral("yyyy-MM-dd hh:mm"))
            : QString());
        QTableWidgetItem *pairedItem=new QTableWidgetItem(
            isPaired(info) ? tr("yes") : tr("no"));
        QTableWidgetItem *idItem=new QTableWidgetItem(info.id);

        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, addrItem);
        table->setItem(row, 2, seenItem);
        table->setItem(row, 3, pairedItem);
        table->setItem(row, 4, idItem);
    }

    onSelectionChanged();
}


void SyncDevicesDialog::onSelectionChanged(void)
{
    QString id;

    if(currentRowId(id)<0)
    {
        fingerprintLabel->setText(tr("Select a device to see its fingerprint."));
        syncButton->setEnabled(false);
        pairButton->setEnabled(false);
        unpairButton->setEnabled(false);
        removeButton->setEnabled(false);
        return;
    }

    fingerprintLabel->setText(tr("Fingerprint:\n%1")
                              .arg(SyncIdentity::fingerprint(id)));

    bool hasAddress=false;
    bool paired=false;

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
    {
        if(info.id!=id)
            continue;

        hasAddress=!info.ip.isEmpty();
        paired=isPaired(info);
        break;
    }

    // Синк инициирует только спок (клиент тянет с хаба). Адрес известен —
    // кнопка жива; иначе подсказка где его взять
    syncButton->setEnabled(hasAddress);
    syncButton->setToolTip(hasAddress ? QString()
        : tr("Address unknown: this device never synced to it. "
             "Sync from the other side first (it records our address)."));
    pairButton->setEnabled(!paired);
    unpairButton->setEnabled(paired);
    removeButton->setEnabled(true);
}


void SyncDevicesDialog::onSyncNow(void)
{
    QString id;

    if(currentRowId(id)<0)
        return;

    AppConfig::SyncDeviceInfo target;

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
    {
        if(info.id==id)
        {
            target=info;
            break;
        }
    }

    if(target.ip.isEmpty())
        return;

    // Спаривание здесь же, если секрета нет: без него hello не пройдет.
    // Фингерпринт уже показан выше — сверка глазами до ввода PIN
    if(!isPaired(target))
    {
        bool ok=false;
        const QString pin=QInputDialog::getText(this,
                                                tr("Sync pairing"),
                                                tr("Enter the pairing PIN (the same PIN "
                                                   "must be entered on %1). Empty = cancel.")
                                                .arg(target.name.isEmpty() ? target.id
                                                                           : target.name),
                                                QLineEdit::Password,
                                                QString(),
                                                &ok);

        if(!ok || pin.isEmpty())
            return;

        if(!SyncIdentity::adoptPairing(config, id, pin))
        {
            QMessageBox::warning(this, tr("Sync"),
                                 tr("Empty PIN is not allowed."));
            return;
        }

        if(config->get_syncGroupId().isEmpty())
            config->touch_syncDevice(id, target.name);
    }

    SyncSession session(model, config, tetradir);

    // PIN в сессию не передаем: секрет уже есть. Починка при расхождении —
    // через Pair заново (осознанно, с показом фингерпринта выше)
    const SyncReport report=session.run(target.ip,
                                        static_cast<quint16>(target.port));

    if(!report.ok)
    {
        QMessageBox::warning(this, tr("Sync"),
                             tr("Sync failed: %1").arg(report.error));
        refreshList();
        return;
    }

    QString summary=tr("Sync complete. Uploaded: notes %1, branches %2. "
                       "Downloaded: notes %3, branches %4. "
                       "Applied deletes: %5.")
        .arg(report.uploadedNotes).arg(report.uploadedBranches)
        .arg(report.downloadedNotes).arg(report.downloadedBranches)
        .arg(report.appliedDeletes);

    if(!report.conflicts.isEmpty())
        summary+=tr("\n\nConflicts (kept this side, other side in backup):\n%1")
                 .arg(report.conflicts.join(QStringLiteral(", ")));

    QMessageBox::information(this, tr("Sync"), summary);
    refreshList();
}


void SyncDevicesDialog::onPair(void)
{
    QString id;

    if(currentRowId(id)<0)
        return;

    QString name;

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
    {
        if(info.id==id)
        {
            name=info.name;
            break;
        }
    }

    bool ok=false;
    const QString pin=QInputDialog::getText(this,
                                            tr("Sync pairing"),
                                            tr("Pair with %1?\nFingerprint above. "
                                               "Enter the SAME pairing PIN as on that device. "
                                               "Empty = cancel.")
                                            .arg(name.isEmpty() ? id : name),
                                            QLineEdit::Password,
                                            QString(),
                                            &ok);

    if(!ok || pin.isEmpty())
        return;

    if(!SyncIdentity::adoptPairing(config, id, pin))
    {
        QMessageBox::warning(this, tr("Sync"),
                             tr("Empty PIN is not allowed."));
        return;
    }

    QMessageBox::information(this, tr("Sync pairing"),
                             tr("Paired. Next sync will be authenticated."));
    refreshList();
}


void SyncDevicesDialog::onUnpair(void)
{
    QString id;

    if(currentRowId(id)<0)
        return;

    if(config->get_syncPairedDeviceId()!=id)
        return;

    if(QMessageBox::question(this, tr("Sync"),
                             tr("Break pairing with this device? "
                                "Next sync will require pairing again."))
       !=QMessageBox::Yes)
        return;

    config->set_syncPairingSecret(QString());
    config->set_syncPairedDeviceId(QString());
    refreshList();
}


void SyncDevicesDialog::onRemove(void)
{
    QString id;

    if(currentRowId(id)<0)
        return;

    if(QMessageBox::question(this, tr("Sync"),
                             tr("Forget this device (remove from list)?"))
       !=QMessageBox::Yes)
        return;

    // Удаляем спаренное — заодно рвем пару, иначе висячий секрет
    if(config->get_syncPairedDeviceId()==id)
    {
        config->set_syncPairingSecret(QString());
        config->set_syncPairedDeviceId(QString());
    }

    config->remove_syncDevice(id);
    refreshList();
}

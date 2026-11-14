#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDateTime>

#include "SyncDevicesDialog.h"
#include "views/mainWindow/MainWindow.h"
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

    // Ручной синк с произвольным хостом — здесь же, а не пунктом меню:
    // весь синк в одном окне. Последний хост подставляется сам
    QHBoxLayout *manual=new QHBoxLayout();
    manual->addWidget(new QLabel(tr("Host:"), this));
    hostEdit=new QLineEdit(this);
    hostEdit->setText(config->get_syncLastHost());
    hostEdit->setPlaceholderText(tr("192.168.1.2"));
    manual->addWidget(hostEdit);
    manual->addWidget(new QLabel(tr("Port:"), this));
    portSpin=new QSpinBox(this);
    portSpin->setRange(1, 65535);
    portSpin->setValue(47777);
    manual->addWidget(portSpin);
    QPushButton *manualSyncButton=new QPushButton(tr("Sync now"), this);
    manual->addWidget(manualSyncButton);
    layout->addLayout(manual);

    QHBoxLayout *buttons=new QHBoxLayout();
    syncButton=new QPushButton(tr("Sync"), this);
    syncButton->setToolTip(tr("Sync with the selected device"));
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
    connect(manualSyncButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onManualSync);
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
    // через Pair заново (осознанно, с показом фингерпринта выше).
    // Исключение — чужая сеть: там PIN нужен чтобы взять группу хаба
    SyncReport report=session.run(target.ip,
                                  static_cast<quint16>(target.port));

    if(!report.ok
       && report.errorCode==QStringLiteral("foreign-network"))
    {
        if(QMessageBox::question(
               this, tr("Sync"),
               tr("This device is in another sync network.\n"
                  "Switch to its network? The secret will be re-derived "
                  "from PIN."))!=QMessageBox::Yes)
        {
            refreshList();
            return;
        }

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
        {
            refreshList();
            return;
        }

        // Тот же путь что headless с PIN: сервер 078+ примет с warning,
        // клиент возьмет группу хаба внутри сессии.
        // Старый сервер так и откажет — тогда входим свежо (сброс пары):
        // пустая группа втягивается сервером любой версии
        report=session.run(target.ip,
                           static_cast<quint16>(target.port),
                           pin);

        if(!report.ok
           && report.errorCode==QStringLiteral("foreign-network"))
        {
            joinFresh();
            report=session.run(target.ip,
                               static_cast<quint16>(target.port),
                               pin);
        }
    }

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


// Ручной синк с произвольным хостом (бывший пункт меню Sync now):
// пробуем hello, при нужде PIN с показом фингерпринта сервера,
// дальше общий путь через MainWindow::runSyncToHeadless
void SyncDevicesDialog::onManualSync(void)
{
    const QString host=hostEdit->text().trimmed();
    const quint16 port=static_cast<quint16>(portSpin->value());

    if(host.isEmpty())
    {
        QMessageBox::warning(this, tr("Sync"),
                             tr("Enter the server address (IP)."));
        return;
    }

    config->set_syncLastHost(host);

    MainWindow *win=qobject_cast<MainWindow *>(parentWidget());

    if(win==nullptr)
        return;

    QString pin;
    QString errorCode;
    bool syncOk=false;
    QString result;

    // Секрета нет: сначала пробуем hello чтобы показать фингерпринт
    // сервера до ввода PIN. Секрет есть: идем сразу в runSyncToHeadless,
    // проба при чужой сети упала бы раньше вопроса о переходе
    if(config->get_syncPairingSecret().isEmpty())
    {
        // Проба: кто отвечает, чтобы показать фингерпринт до ввода PIN
        SyncClient probe(config);
        SyncHelloResult hello=probe.hello(host, port);

        if(!hello.ok)
        {
            // Чужая сеть уже на пробе: сразу предлагаем войти свежо
            // (сервер любой версии: пустая группа втягивается всегда)
            if(hello.errorCode==QStringLiteral("foreign-network")
               && !hello.remoteGroupId.isEmpty())
            {
                pin=askJoinPin(hello.remoteDeviceName.isEmpty()
                               ? hello.remoteDeviceId : hello.remoteDeviceName,
                               hello.remoteGroupId);

                if(pin.isEmpty())
                {
                    refreshList();
                    return;
                }

                joinFresh();
                result=win->runSyncToHeadless(host, pin, port, syncOk);
                showManualResult(result, syncOk);
                return;
            }

            QMessageBox::warning(this, tr("Sync"),
                                 tr("Handshake failed: %1").arg(hello.errorText));
            refreshList();
            return;
        }

        bool ok=false;
        pin=QInputDialog::getText(
            this,
            tr("Sync pairing"),
            tr("Unknown sync server.\n\nName: %1\nFingerprint:\n%2\n\n"
               "Enter the pairing PIN (the same PIN must be entered "
               "on the server). Empty = cancel.")
            .arg(hello.remoteDeviceName.isEmpty()
                 ? hello.remoteDeviceId : hello.remoteDeviceName,
                 SyncIdentity::fingerprint(hello.remoteDeviceId)),
            QLineEdit::Password,
            QString(),
            &ok);

        if(!ok || pin.isEmpty())
            return;
    }

    result=win->runSyncToHeadless(host, pin, port,
                                  syncOk, &errorCode);

    // Чужая сеть: сервер 078+ с PIN сам примет с warning.
    // Старый либо отказ без PIN — предлагаем войти свежо (сброс локальной
    // пары + PIN): пустая группа втягивается сервером любой версии
    if(!syncOk && errorCode==QStringLiteral("foreign-network"))
    {
        pin=askJoinPin(QString());

        if(pin.isEmpty())
        {
            refreshList();
            return;
        }

        joinFresh();
        result=win->runSyncToHeadless(host, pin, port, syncOk);
    }

    showManualResult(result, syncOk);
}


// PIN для входа в чужую сеть. Честно предупреждаем: локальная пара
// (секрет, группа, привязка) будет пересоздана, старая сеть забыта.
// Пусто — отмена пользователем
QString SyncDevicesDialog::askJoinPin(const QString &peerName,
                                      const QString &peerGroup)
{
    QString text=tr("This server is in another sync network.\n"
                    "Join it? Local pairing (secret, group) will be "
                    "re-created from PIN, current network forgotten.");

    if(!peerName.isEmpty())
        text+=tr("\n\nServer: %1").arg(peerName);

    if(!peerGroup.isEmpty())
        text+=tr("\nIts group: %1").arg(peerGroup);

    if(QMessageBox::question(this, tr("Sync"), text)!=QMessageBox::Yes)
        return QString();

    bool ok=false;
    const QString pin=QInputDialog::getText(this,
                                            tr("Sync pairing"),
                                            tr("Enter the pairing PIN (the same PIN "
                                               "must be entered on the server). "
                                               "Empty = cancel."),
                                            QLineEdit::Password,
                                            QString(),
                                            &ok);

    if(!ok || pin.isEmpty())
        return QString();

    return pin;
}


// Сброс локальной пары перед входом в чужую сеть: секрет, группа,
// привязка. Дальше runSyncToHeadless с PIN спарит с нуля на любом сервере
void SyncDevicesDialog::joinFresh(void)
{
    config->set_syncPairingSecret(QString());
    config->set_syncGroupId(QString());
    config->set_syncPairedDeviceId(QString());
}


void SyncDevicesDialog::showManualResult(const QString &result, bool syncOk)
{
    if(!syncOk)
        QMessageBox::warning(this, tr("Sync"), result);
    else
        QMessageBox::information(this, tr("Sync"), result);

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
                                "You will leave the sync network as well. "
                                "Next sync will require pairing again."))
       !=QMessageBox::Yes)
        return;

    config->set_syncPairingSecret(QString());
    config->set_syncPairedDeviceId(QString());
    config->set_syncGroupId(QString());
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

    // Устройств не осталось — статус сбрасывается автоматически:
    // секрет, группа и пара чистятся, устройство девственно чистое.
    // Отдельного подтверждения не спрашиваем: удаление уже подтвердили
    if(config->get_syncDevices().isEmpty())
    {
        config->set_syncPairingSecret(QString());
        config->set_syncGroupId(QString());
        config->set_syncPairedDeviceId(QString());
    }

    refreshList();
}

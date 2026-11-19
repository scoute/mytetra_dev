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
#include <QMap>
#include <QSet>
#include <QTimer>

#include "SyncDevicesDialog.h"
#include "SyncPreviewDialog.h"
#include "views/mainWindow/MainWindow.h"
#include "libraries/sync/SyncIdentity.h"
#include "libraries/sync/SyncSession.h"
#include "libraries/sync/SyncClient.h"
#include "libraries/sync/SyncNoteIo.h"
#include "libraries/sync/SyncDebug.h"
#include "libraries/sync/SyncDiscovery.h"
#include "libraries/sync/SyncSidecar.h"
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
    resize(640, 420);

    QVBoxLayout *layout=new QVBoxLayout(this);

    // Свое состояние пары — тем же окном, а не отдельной командой.
    // Иначе повторяется путаница "список пуст, а секрет где-то блокирует"
    selfLabel=new QLabel(this);
    selfLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    selfLabel->setWordWrap(true);
    layout->addWidget(selfLabel);

    table=new QTableWidget(this);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(QStringList()
        << tr("Name") << tr("Last address") << tr("Last seen")
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
    scanButton=new QPushButton(tr("Scan"), this);
    scanButton->setToolTip(tr("Find sync hubs in the local network "
                              "and refresh their addresses"));
    manual->addWidget(scanButton);
    layout->addLayout(manual);

    QHBoxLayout *buttons=new QHBoxLayout();
    syncButton=new QPushButton(tr("Sync"), this);
    syncButton->setToolTip(tr("Sync with the selected device"));
    pairButton=new QPushButton(tr("Pair..."), this);
    acceptButton=new QPushButton(tr("Accept"), this);
    acceptButton->setToolTip(tr("Accept the incoming pair request "
                               "from the selected device"));
    unpairButton=new QPushButton(tr("Unpair"), this);
    removeButton=new QPushButton(tr("Remove"), this);
    QPushButton *closeButton=new QPushButton(tr("Close"), this);
    QPushButton *resetButton=new QPushButton(tr("Reset all"), this);
    resetButton->setToolTip(tr("Forget everything: pairing secret, group, "
                               "paired device and the whole list. "
                               "Same as --sync-forget plus list cleanup."));
    buttons->addWidget(syncButton);
    buttons->addWidget(pairButton);
    buttons->addWidget(acceptButton);
    buttons->addWidget(unpairButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    buttons->addWidget(resetButton);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(table, &QTableWidget::itemSelectionChanged,
            this, &SyncDevicesDialog::onSelectionChanged);
    connect(syncButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onSyncNow);
    connect(pairButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onPair);
    connect(acceptButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onAccept);
    connect(unpairButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onUnpair);
    connect(removeButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onRemove);
    connect(resetButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onResetAll);
    connect(manualSyncButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onManualSync);
    connect(scanButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onScan);
    connect(closeButton, &QPushButton::clicked,
            this, &QDialog::accept);

    // Живое обновление без опроса: список sync-устройств меняется снаружи
    // (входящий hello, pair-request, Accept на другой стороне) — конфиг
    // сигналит, окно перечитывает. Выделение строки сохраняется по id
    connect(config, &AppConfig::syncDevicesChanged,
            this, &SyncDevicesDialog::refreshList);

    // Добивка исходящего запроса: споку некуда принимать пуш от сервера,
    // поэтому пока висит свой неотвеченный pair-request — перепроверяем
    // коротким hello (пришел ли Accept). Останавливается сам, как только
    // взаимность наступила или запрос снят. Это единственный тикающий
    // кусок, и он узкий: только свой запрос, только пока он висит
    recheckTimer=new QTimer(this);
    recheckTimer->setSingleShot(true);
    connect(recheckTimer, &QTimer::timeout,
            this, &SyncDevicesDialog::onRecheckPair);

    refreshList();
}


bool SyncDevicesDialog::isPaired(const AppConfig::SyncDeviceInfo &info) const
{
    // Кольцо: у каждого спаренного пира свой секрет
    return !SyncIdentity::secretForPeer(config, info.id).isEmpty();
}


bool SyncDevicesDialog::isMutual(const AppConfig::SyncDeviceInfo &info) const
{
    if(!isPaired(info))
        return false;

    return info.lastMutual>0;
}


bool SyncDevicesDialog::ownRequestPending(QString *ip,
                                          quint16 *port) const
{
    const QMap<QString, QString> ring=config->get_syncSecretsRing();

    if(ring.isEmpty())
        return false;

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
    {
        if(!ring.contains(info.id) || info.id==config->get_syncDeviceId())
            continue;

        if(info.lastMutual>0)
            continue;

        if(ip!=nullptr)
            *ip=info.lastIp;

        if(port!=nullptr)
            *port=static_cast<quint16>(info.port);

        return true;
    }

    // Привязка есть, а строки нет (призрак): запрос висит, но перепроверить
    // некуда — ждем ручного Pair со вводом адреса
    return false;
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
    refreshSelf();

    // Автотик живого обновления не должен сбрасывать выделение:
    // запоминаем id, после перестройки выделяем ту же строку
    QString selectedId;
    currentRowId(selectedId);

    QList<AppConfig::SyncDeviceInfo> devices=config->get_syncDevices();

    // Каждый спаренный пир висит строкой всегда, даже если ни разу
    // не виден в сети (секрет есть, записи нет). Иначе его нельзя
    // точечно разорвать: Reset all при нескольких коннектах — не выход.
    const QMap<QString, QString> ring=config->get_syncSecretsRing();
    QSet<QString> haveRows;

    for(const AppConfig::SyncDeviceInfo &info : devices)
        haveRows.insert(info.id);

    QList<QString> ghostIds;

    for(auto it=ring.constBegin(); it!=ring.constEnd(); ++it)
        if(!haveRows.contains(it.key()))
            ghostIds << it.key();

    bool syntheticPaired=false;

    for(const QString &ghostId : ghostIds)
    {
        if(ghostId==config->get_syncDeviceId())
            continue;

        AppConfig::SyncDeviceInfo ghost;
        ghost.id=ghostId;
        devices << ghost;
        syntheticPaired=true;
    }

    table->setRowCount(devices.size());

    for(int row=0; row<devices.size(); ++row)
    {
        const AppConfig::SyncDeviceInfo &info=devices.at(row);
        const bool isGhost=syntheticPaired && row==devices.size()-1;

        QTableWidgetItem *nameItem=new QTableWidgetItem(
            info.name.isEmpty()
            ? (isGhost ? tr("(unknown device)") : info.id.left(12))
            : info.name);

        if(isGhost)
        {
            nameItem->setToolTip(
                tr("Paired peer, never seen on network. "
                   "Unpair or Remove to break pairing selectively."));
            QFont ghostFont=nameItem->font();
            ghostFont.setItalic(true);
            nameItem->setFont(ghostFont);
        }

        QTableWidgetItem *addrItem=new QTableWidgetItem(
            info.lastIp.isEmpty() ? tr("(unknown address)")
                              : QStringLiteral("%1:%2").arg(info.lastIp).arg(info.port));
        QTableWidgetItem *seenItem=new QTableWidgetItem(
            info.lastSeen>0
            ? QDateTime::fromMSecsSinceEpoch(info.lastSeen)
                  .toString(QStringLiteral("yyyy-MM-dd hh:mm"))
            : QString());
        // Paired колонка статусов accept-flow, а не да/нет:
        // yes — взаимно спарены (обмен с проверкой был), Sync можно.
        // pair-request — мы отправили запрос (секрет выведен, но взаимности
        //   еще нет): ждем Accept на той стороне. Повтор Pair перепроверяет.
        // wait-accept — входящий запрос: нужен Accept с вводом общего PIN.
        // no — чужая строка или пары нет. unknown — секрет есть, но с кем —
        //   неизвестно (наследие без привязки). error — пара сломана
        //   (спарен сам с собой).
        // Кольцо: спаренность проверяется по секрету КОНКРЕТНОЙ строки
        const bool haveSecret=!ring.isEmpty();
        const bool selfPairing=haveSecret
            && ring.contains(config->get_syncDeviceId());

        QString pairedText;
        QString pairedTip;

        if(!haveSecret)
        {
            pairedText=tr("no");
        }
        else if(selfPairing)
        {
            pairedText=tr("error");
            pairedTip=tr("Broken pairing: paired with self. "
                         "Reset all and pair again.");
        }
        else if(!config->get_syncPairingSecret().isEmpty()
                && config->get_syncPairedDeviceId().isEmpty())
        {
            pairedText=tr("unknown");
            pairedTip=tr("Paired with unknown device (legacy state, "
                         "no peer binding). Re-pair via Pair or Reset all.");
        }
        else if(ring.contains(info.id))
        {
            // Призрак (спаренный, но ни разу не виденный) — флагов нет,
            // взаимность неизвестна: оставляем legacy yes
            if(isGhost || info.lastMutual>0)
            {
                pairedText=tr("yes");
            }
            else
            {
                pairedText=tr("pair-request");
                pairedTip=tr("Pair request sent, waiting for Accept "
                             "on that device. Press Pair again to re-check "
                             "whether it was accepted.");
            }
        }
        else if(info.pairOffer)
        {
            pairedText=tr("wait-accept");
            pairedTip=tr("Incoming pair request from this device. "
                         "Press Accept and enter the same PIN to pair.");
        }
        else
        {
            pairedText=tr("no");
        }

        QTableWidgetItem *pairedItem=new QTableWidgetItem(pairedText);

        if(!pairedTip.isEmpty())
            pairedItem->setToolTip(pairedTip);
        QTableWidgetItem *idItem=new QTableWidgetItem(info.id);

        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, addrItem);
        table->setItem(row, 2, seenItem);
        table->setItem(row, 3, pairedItem);
        table->setItem(row, 4, idItem);

        // Возвращаем выделение той же строке (обновление по событию)
        if(!selectedId.isEmpty() && info.id==selectedId)
            table->selectRow(row);
    }

    // Добивку исходящего запроса взводим только пока он висит;
    // иначе стоп (окно открыто, делать нечего)
    if(ownRequestPending())
    {
        if(!recheckTimer->isActive())
            recheckTimer->start(5000);
    }
    else
        recheckTimer->stop();

    onSelectionChanged();
}


// Свое состояние пары в том же окне: устройство, спаренность, группа.
// Именно его отсутствие порождало "список пуст, а синк заблокирован"
void SyncDevicesDialog::refreshSelf(void)
{
    const QString ownId=config->get_syncDeviceId();
    const QMap<QString, QString> ring=config->get_syncSecretsRing();
    const bool paired=!ring.isEmpty();
    const QString group=config->get_syncGroupId();

    QString peerText=tr("no");

    if(paired)
    {
        // Сломана в принципе: спарен сам с собой. Такой secret ни с кем
        // не сойдется — только Reset all и спаривание заново
        if(ring.contains(ownId))
        {
            peerText=tr("ERROR: paired with self!");
        }
        else
        {
            // Кольцо: перечисляем всех спаренных пиров (4-5 устройств),
            // по каждому — взаимность отдельным флагом
            QStringList peerTexts;

            for(auto it=ring.constBegin(); it!=ring.constEnd(); ++it)
            {
                const QString &pid=it.key();
                QString peerName;
                qint64 peerMutual=-1;

                for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
                {
                    if(info.id==pid)
                    {
                        peerName=info.name;
                        peerMutual=info.lastMutual;
                        break;
                    }
                }

                if(peerName.isEmpty())
                    peerName=pid+tr(" (not in list!)");

                // Предложили, взаимности нет — честный статус вместо yes:
                // запрос ушёл, ждем Accept на той стороне
                if(peerMutual==0)
                    peerTexts << tr("%1 (pair requested, waiting for accept)")
                        .arg(peerName);
                else
                    peerTexts << peerName;
            }

            peerText=tr("yes, with %1").arg(peerTexts.join(QStringLiteral("; ")));
        }
    }

    selfLabel->setText(
        tr("This device: %1\nPaired: %2\nGroup: %3")
        .arg(config->get_syncDeviceName().isEmpty()
             ? ownId : config->get_syncDeviceName(),
             peerText,
             group.isEmpty() ? tr("(none)") : group));
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
    bool mutual=false;
    bool offer=false;

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
    {
        if(info.id!=id)
            continue;

        hasAddress=!info.lastIp.isEmpty();
        paired=isPaired(info);
        mutual=isMutual(info);
        offer=info.pairOffer;
        break;
    }

    // Sync — только по взаимно спаренной строке (шаг 5 accept-flow):
    // без Accept на той стороне данных нет (там же гейт 089/сервер).
    // Pair — отправить запрос или перепроверить отправленный.
    // Входящий запрос спаривается кнопкой Accept, а не Pair
    syncButton->setEnabled(hasAddress && mutual);
    syncButton->setToolTip(!hasAddress
        ? tr("Address unknown: this device never synced to it. "
             "Sync from the other side first (it records our address).")
        : (!mutual
           ? tr("Not mutually paired: send a Pair request first "
                "(or Accept the incoming one), sync unlocks after Accept.")
           : QString()));
    pairButton->setEnabled(!mutual && !offer);
    pairButton->setToolTip(offer
        ? tr("This device asks to pair: use Accept, not Pair.")
        : QString());
    acceptButton->setEnabled(offer);
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

    if(target.lastIp.isEmpty())
        return;

    // Sync только по взаимной паре (шаг 5 accept-flow): без Accept
    // на той стороне кнопка и так выключена, это вторая линия.
    // Запрос в пару — кнопкой Pair, входящий — кнопкой Accept
    if(!isMutual(target))
    {
        QMessageBox::warning(this, tr("Sync"),
                             tr("Not mutually paired with this device. "
                                "Send a Pair request first (or Accept "
                                "the incoming one); sync unlocks after "
                                "Accept on that side."));
        refreshList();
        return;
    }

    SyncSession session(model, config, tetradir);

    // Стратегия первого синка — тот же диалог, что и в headless-пути
    session.setStrategyResolver(
        [this](int localItems, int remoteItems) -> SyncStrategy {
            return MainWindow::askSyncStrategy(this, localItems, remoteItems);
        });

    // Опасный синк (удаления) — явный выбор: продолжить / отменить /
    // восстановиться с сервера. Безопасное едет молча
    session.setDeleteGuard(
        [this](int remoteDeletes, int localDeletes)
            -> SyncSession::DeleteDecision {
            QMessageBox box(this);
            box.setWindowTitle(tr("Sync"));
            box.setIcon(QMessageBox::Warning);
            box.setText(tr("This sync will DELETE data:\n"
                           "%1 item(s) on the other side, "
                           "%2 on this device.\n"
                           "Deleted items go to trash.\n\n"
                           "What to do?")
                        .arg(remoteDeletes).arg(localDeletes));

            QPushButton *continueButton=box.addButton(
                tr("Continue delete"), QMessageBox::YesRole);
            QPushButton *restoreButton=box.addButton(
                tr("Restore from the server instead "
                   "(my data goes to trash)"), QMessageBox::YesRole);
            QPushButton *cancelButton=box.addButton(
                tr("Cancel"), QMessageBox::RejectRole);
            box.setDefaultButton(cancelButton);
            box.exec();

            if(box.clickedButton()==continueButton)
                return SyncSession::DeleteDecision::Proceed;

            if(box.clickedButton()==restoreButton)
                return SyncSession::DeleteDecision::Restore;

            return SyncSession::DeleteDecision::Cancel;
        });

    // Предпросмотр: окно выбора по категориям (создание едет молча,
    // остальное — явным выбором). Без него встречные правки тихо
    // решались по mtime и пользователь не видел никакого выбора
    session.setPreviewResolver(
        [this, &target](const SyncPreview::Preview &preview,
                        SyncPreview::Decisions &decisions) -> bool {
            const auto ownText=[this](const SyncPreview::Item &item) -> QString {
                if(item.isBranch)
                    return QString();

                const QJsonObject note=SyncNoteIo::serializeNote(
                    model, tetradir, item.id);

                if(note.contains(QStringLiteral("error")))
                    return QString();

                return QString::fromUtf8(QByteArray::fromBase64(
                    note.value(QStringLiteral("text")).toString().toLatin1()));
            };
            const auto otherText=[&target, this](const SyncPreview::Item &item) -> QString {
                if(item.isBranch)
                    return QString();

                SyncClient client(config);
                const QJsonObject note=client.pullNote(target.lastIp,
                    static_cast<quint16>(target.port), item.id);

                if(note.contains(QStringLiteral("error")))
                    return QString();

                return QString::fromUtf8(QByteArray::fromBase64(
                    note.value(QStringLiteral("text")).toString().toLatin1()));
            };

            SyncPreviewDialog dialog(preview,
                SyncPreview::Decisions::defaults(),
                ownText, otherText, this);

            if(dialog.exec()!=QDialog::Accepted)
                return false;

            decisions=dialog.decisions();

            return true;
        });

    // PIN в сессию не передаем: секрет уже есть. Починка при расхождении —
    // через Pair заново (осознанно, с показом фингерпринта выше).
    // Исключение — чужая сеть: там PIN нужен чтобы взять группу хаба
    SyncReport report=session.run(target.lastIp,
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
        report=session.run(target.lastIp,
                           static_cast<quint16>(target.port),
                           pin);

        if(!report.ok
           && report.errorCode==QStringLiteral("foreign-network"))
        {
            joinFresh();
            report=session.run(target.lastIp,
                               static_cast<quint16>(target.port),
                               pin);
        }
    }

    if(!report.ok)
    {
        QString fail=tr("Sync failed: %1").arg(report.error);
        const QString steps=session.log().join(QStringLiteral("\n"));

        if(!steps.isEmpty())
            fail+=QStringLiteral("\n-- steps --\n")+steps;

        QMessageBox::warning(this, tr("Sync"),
                             MainWindow::trimSyncReportForDialog(fail));
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

    if(!report.errors.isEmpty())
        summary+=tr("\n\nItem errors (skipped items):\n%1")
                 .arg(report.errors.join(QStringLiteral("\n")));

    QMessageBox::information(this, tr("Sync"),
                               MainWindow::trimSyncReportForDialog(summary));
    refreshList();
}


// Скан сети: хабы отвечают на UDP-проб своей идентичностью,
// адреса в списке обновляются сами (DHCP-терпимо)
void SyncDevicesDialog::onScan(void)
{
    const quint16 port=static_cast<quint16>(portSpin->value());

    const QList<SyncDiscoveredDevice> found=
        SyncDiscovery::scan(config->get_syncDeviceId(), port);

    for(const SyncDiscoveredDevice &device : found)
        config->touch_syncDevice(device.deviceId, device.deviceName,
                                 device.address, device.port);

    refreshList();

    QMessageBox::information(this, tr("Scan"), found.isEmpty()
        ? tr("No devices found. Check that the sync server is running "
             "in the same network on port %1.").arg(port)
        : tr("Found %1 device(s) in the local network.").arg(found.size()));
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
// привязка. Дальше runSyncToHeadless с PIN спарит с нуля на любом сервере.
// Флаги запросов/взаимности по всем строкам тоже гаснут: старая сеть забыта
void SyncDevicesDialog::joinFresh(void)
{
    // Кольцо целиком: вход в чужую сеть забывает все пары
    config->clear_syncSecretsRing();
    config->set_syncGroupId(QString());

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
    {
        config->clear_syncDeviceOffer(info.id);
        config->clear_syncDeviceMutual(info.id);
    }
}


void SyncDevicesDialog::showManualResult(const QString &result, bool syncOk)
{
    if(!syncOk)
        QMessageBox::warning(this, tr("Sync"),
                             MainWindow::trimSyncReportForDialog(result));
    else
        QMessageBox::information(this, tr("Sync"),
                               MainWindow::trimSyncReportForDialog(result));

    refreshList();
}


void SyncDevicesDialog::onPair(void)
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

    if(target.lastIp.isEmpty())
    {
        QMessageBox::warning(this, tr("Sync pairing"),
                             tr("Address unknown: cannot reach this device. "
                                "Enter its IP below and use Sync now "
                                "(it sends the pair request too)."));
        refreshList();
        return;
    }

    qDebug() << "SyncPair: Pair pressed for"
             << (target.name.isEmpty() ? target.id : target.name)
             << "at" << target.lastIp << target.port;

    // Уже предлагали, взаимности нет: повтор Pair = проверка статуса
    // (пришел ли Accept). Секрет уже выведен — hello пойдет с proof
    if(isPaired(target) && !isMutual(target))
    {
        qDebug() << "SyncPair: re-checking whether our request was accepted";
        SyncClient probe(config);
        const SyncHelloResult hello=probe.hello(target.lastIp,
            static_cast<quint16>(target.port));

        if(!hello.ok)
        {
            qDebug() << "SyncPair: re-check failed:" << hello.errorCode;
            QMessageBox::warning(this, tr("Sync pairing"),
                                 tr("Handshake failed: %1")
                                 .arg(hello.errorText));
        }
        else if(hello.paired)
        {
            qDebug() << "SyncPair: our request was accepted, sync unlocked";
            SyncHandshake::noteMutual(config, id, "pair re-check");
            QMessageBox::information(this, tr("Sync pairing"),
                                     tr("Accepted on that side, "
                                        "sync unlocked."));
        }
        else
        {
            qDebug() << "SyncPair: still waiting for Accept on that side";
            QMessageBox::information(this, tr("Sync pairing"),
                                     tr("Still waiting for Accept "
                                        "on that device."));
        }

        refreshList();
        return;
    }

    bool ok=false;
    const QString pin=QInputDialog::getText(this,
                                            tr("Sync pairing"),
                                            tr("Pair with %1?\nFingerprint above. "
                                               "Enter the SAME pairing PIN as on that device. "
                                               "Empty = cancel.")
                                            .arg(target.name.isEmpty() ? id : target.name),
                                            QLineEdit::Password,
                                            QString(),
                                            &ok);

    if(!ok || pin.isEmpty())
        return;

    MainWindow *win=qobject_cast<MainWindow *>(parentWidget());

    if(win==nullptr)
        return;

    // Общий путь: pre-hello, adopt предложения, group, offering-hello
    // с pairRequest, стоп на pair-established (шаг 1 accept-flow).
    // Сервер запишет входящий запрос для своего Accept
    bool syncOk=false;
    QString errorCode;
    const QString result=win->runSyncToHeadless(target.lastIp, pin,
                                               static_cast<quint16>(target.port),
                                               syncOk, &errorCode);
    showManualResult(result, syncOk);
}


void SyncDevicesDialog::onRecheckPair(void)
{
    QString ip;
    quint16 port=0;

    if(!ownRequestPending(&ip, &port) || ip.isEmpty())
        return; // сняли/приняли пока ждали — refreshList уже пересчитал

    qDebug() << "SyncPair: auto re-check of outstanding request to"
             << ip << port;

    SyncClient probe(config);
    probe.setTimeoutMs(4000);

    const SyncHelloResult hello=probe.hello(ip, port);

    if(hello.ok && hello.paired)
    {
        qDebug() << "SyncPair: request accepted, sync unlocked";
        SyncHandshake::noteMutual(config,
                                 config->get_syncPairedDeviceId(),
                                 "auto re-check");
        // refreshList придет по syncDevicesChanged
        return;
    }

    if(syncDebugEnabled())
        qDebug() << "SyncPair: still waiting"
                 << (hello.ok ? QString() : hello.errorCode);

    // Не приняли (или недоступен) — повторим, пока запрос висит
    // и окно открыто. Снятие запроса остановит через refreshList
    recheckTimer->start(5000);
}


void SyncDevicesDialog::onAccept(void)
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

    if(!target.pairOffer)
        return;

    qDebug() << "SyncPair: Accept pressed for"
             << (target.name.isEmpty() ? target.id : target.name);

    bool ok=false;
    const QString pin=QInputDialog::getText(this,
                                            tr("Sync pairing"),
                                            tr("Accept pair request from %1?\n"
                                               "Fingerprint above. "
                                               "Enter the SAME pairing PIN as on that device. "
                                               "Empty = cancel.")
                                            .arg(target.name.isEmpty() ? id : target.name),
                                            QLineEdit::Password,
                                            QString(),
                                            &ok);

    if(!ok || pin.isEmpty())
        return;

    // adopt гасит запрос (stash 087); взаимность — решением оператора:
    // PIN введен здесь осознанно, дальше докажет первый обмен
    if(!SyncIdentity::adoptPairing(config, id, pin))
    {
        QMessageBox::warning(this, tr("Sync"),
                             tr("Empty PIN is not allowed."));
        return;
    }

    SyncHandshake::noteMutual(config, id, "operator Accept");
    qDebug() << "SyncPair: accepted" << id << ", sync unlocked both sides";

    QMessageBox::information(this, tr("Sync pairing"),
                             tr("Paired with %1. Sync unlocked.")
                             .arg(target.name.isEmpty() ? id : target.name));
    refreshList();
}


void SyncDevicesDialog::onUnpair(void)
{
    QString id;

    if(currentRowId(id)<0)
        return;

    // Кольцо: рвем пару точечно, остальные пиры не трогаем
    if(!config->get_syncSecretsRing().contains(id))
        return;

    const bool wasPrimary=(config->get_syncPairedDeviceId()==id);

    const QString askText=tr("Break pairing with this device? %1"
                                "Next sync will require pairing again.")
        .arg(wasPrimary
             ? tr("You will leave the sync network as well. ")
             : tr("Other pairings stay intact. "));

    if(QMessageBox::question(this, tr("Sync"), askText)!=QMessageBox::Yes)
        return;

    config->clear_syncDeviceSecret(id);

    if(wasPrimary)
    {
        config->set_syncPairingSecret(QString());
        config->set_syncPairedDeviceId(QString());
        config->set_syncGroupId(QString());

        // Журнал синка — в новую эпоху (см. SyncIdentity::adoptPairing)
        SyncSidecar::reset();
    }

    // Флаги пары по строке тоже гаснут: запрос принят/отклонен разрывом
    qDebug() << "SyncPair: unpaired" << id << ", offer/mutual cleared";
    config->clear_syncDeviceOffer(id);
    config->clear_syncDeviceMutual(id);
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

    // Удаляем спаренное — заодно рвем пару, иначе висячий секрет.
    // Кольцо: снимаем секрет строки; первичная пара рвется целиком
    if(config->get_syncSecretsRing().contains(id))
    {
        config->clear_syncDeviceSecret(id);

        if(config->get_syncPairedDeviceId()==id)
        {
            config->set_syncPairingSecret(QString());
            config->set_syncPairedDeviceId(QString());
            SyncSidecar::reset();
        }
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


// Полный сброс — видимый аналог --sync-forget плюс чистка списка.
// Нужен именно кнопкой здесь: состояние обязано управляться из UI,
// а не отдельной командой про которую надо знать
void SyncDevicesDialog::onResetAll(void)
{
    if(QMessageBox::question(this, tr("Sync"),
                             tr("Forget everything? Pairing secret, sync group, "
                                "paired device and the whole device list "
                                "will be cleared. This cannot be undone."))
       !=QMessageBox::Yes)
        return;

    // Кольцо целиком: легаси-пара, секреты устройств, группа, список
    config->clear_syncSecretsRing();
    config->set_syncGroupId(QString());

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
        config->remove_syncDevice(info.id);

    // Журнал синка — в новую эпоху
    SyncSidecar::reset();

    refreshList();
}

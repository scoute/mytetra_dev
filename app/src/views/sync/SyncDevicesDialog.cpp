#include <QSignalBlocker>
#include <QTableWidget>
#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHostInfo>
#include <QSslCertificate>
#include <QDialog>
#include <QDialogButtonBox>
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
#include <QBrush>
#include <QColor>
#include <QTimer>

#include "SyncDevicesDialog.h"
#include "SyncPairDialogs.h"
#include "libraries/sync/SyncPairCrypto.h"
#include "libraries/sync/SyncPairing.h"
#include "libraries/sync/SyncHandshake.h"
#include "libraries/sync/SyncTls.h"
#include "SyncPreviewDialog.h"
#include "SyncProgressWindow.h"
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
                                     const QString &tetradir,
                                     quint16 serverPort, QWidget *parent)
    : QDialog(parent),
      model(model),
      config(config),
      tetradir(tetradir),
      serverPort(serverPort)
{
    setWindowTitle(tr("Sync devices"));
    resize(720, 640);

    QVBoxLayout *layout=new QVBoxLayout(this);

    // Свое состояние пары — тем же окном, а не отдельной командой.
    // Иначе повторяется путаница "список пуст, а секрет где-то блокирует"
    selfLabel=new QLabel(this);
    selfLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    selfLabel->setWordWrap(true);
    layout->addWidget(selfLabel);

    table=new QTableWidget(this);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(QStringList()
        << tr("Name") << tr("Last address") << tr("Last seen")
        << tr("Paired") << tr("Last sync") << tr("Device ID"));
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->setColumnHidden(5, true); // id держим в строке, не показываем
    layout->addWidget(table);

    // Пустой список — не тупик: три шага первого знакомства
    emptyHint=new QLabel(this);
    emptyHint->setWordWrap(true);
    emptyHint->setText(tr("Чтобы синхронизироваться с другим устройством:\n"
                          "1) включите «Sync server» на нём (меню Sync);\n"
                          "2) нажмите Scan — хаб появится в списке;\n"
                          "3) выберите его и нажмите Sync selected (код покажет компьютер)."));
    emptyHint->setVisible(false);
    layout->addWidget(emptyHint);

    fingerprintLabel=new QLabel(this);
    fingerprintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fingerprintLabel->setWordWrap(true);
    layout->addWidget(fingerprintLabel);

    // --- настройки серверной стороны ---
    QHBoxLayout *serverOpts=new QHBoxLayout();
    blockPairingCheck=new QCheckBox(tr("Block new pairings"), this);
    blockPairingCheck->setToolTip(
        tr("Refuse new devices. Existing pairs keep syncing. "
           "Turning it on cancels pairing in progress (codes and "
           "pending secrets)."));
    bindIpCheck=new QCheckBox(tr("Bind code to client IP"), this);
    bindIpCheck->setToolTip(
        tr("The one-time code works only from the IP that requested it. "
           "Useful on a LAN where each device has its own IP. "
           "Do NOT enable behind NAT/reverse proxy (shared IP) or if "
           "the client's IP can change between requesting the code "
           "and pair-begin."));
    serverOpts->addWidget(blockPairingCheck);
    serverOpts->addWidget(bindIpCheck);
    serverOpts->addStretch();
    layout->addLayout(serverOpts);

    QLabel *serverHint=new QLabel(this);
    serverHint->setWordWrap(true);
    serverHint->setStyleSheet(QStringLiteral("color: #9E9E9E;"));
    serverHint->setText(tr("Block — сервер не принимает новые устройства; "
                           "старые пары синкаются, незавершённая пара "
                           "отменяется.\n"
                           "Bind IP — код работает только с того IP, "
                           "который его запросил (LAN с уникальными IP; "
                           "не включайте за NAT/прокси и при смене IP)."));
    layout->addWidget(serverHint);

    connect(blockPairingCheck, &QCheckBox::toggled,
            this, &SyncDevicesDialog::onBlockPairingToggled);
    connect(bindIpCheck, &QCheckBox::toggled, this, [this](bool checked) {
        this->config->set_syncPairBindIp(checked);
    });

    // --- TLS и адрес для QR ---
    QGroupBox *tlsBox=new QGroupBox(tr("TLS / advertised address"), this);
    QGridLayout *tlsGrid=new QGridLayout(tlsBox);

    tlsServerCheck=new QCheckBox(tr("Use TLS (HTTPS) for sync server"), tlsBox);
    tlsServerCheck->setToolTip(
        tr("The port speaks TLS only. For a public server use a trusted "
           "certificate (Let's Encrypt fullchain.pem/privkey.pem). "
           "For LAN you can generate a self-signed one and confirm its "
           "fingerprint on the client."));
    tlsGrid->addWidget(tlsServerCheck, 0, 0, 1, 3);

    tlsGrid->addWidget(new QLabel(tr("Advertised host (QR/TLS):"), tlsBox),
                       1, 0);
    advertiseHostEdit=new QLineEdit(tlsBox);
    advertiseHostEdit->setPlaceholderText(
        tr("empty = primary IPv4; public server: domain from the cert"));
    tlsGrid->addWidget(advertiseHostEdit, 1, 1, 1, 2);

    tlsCertButton=new QPushButton(tr("Certificate..."), tlsBox);
    tlsKeyButton=new QPushButton(tr("Private key..."), tlsBox);
    tlsGenerateButton=new QPushButton(tr("Generate self-signed..."), tlsBox);
    tlsGrid->addWidget(tlsCertButton, 2, 0);
    tlsGrid->addWidget(tlsKeyButton, 2, 1);
    tlsGrid->addWidget(tlsGenerateButton, 2, 2);

    tlsCertLabel=new QLabel(tlsBox);
    tlsCertLabel->setWordWrap(true);
    tlsCertLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tlsKeyLabel=new QLabel(tlsBox);
    tlsKeyLabel->setWordWrap(true);
    tlsKeyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tlsFingerprintLabel=new QLabel(tlsBox);
    tlsFingerprintLabel->setWordWrap(true);
    tlsFingerprintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tlsGrid->addWidget(tlsCertLabel, 3, 0, 1, 3);
    tlsGrid->addWidget(tlsKeyLabel, 4, 0, 1, 3);
    tlsGrid->addWidget(tlsFingerprintLabel, 5, 0, 1, 3);

    tlsGrid->addWidget(new QLabel(tr("Outgoing sync:"), tlsBox), 6, 0);
    tlsClientModeCombo=new QComboBox(tlsBox);
    tlsClientModeCombo->addItem(tr("HTTP"));
    tlsClientModeCombo->addItem(tr("HTTPS (prefer)"));
    tlsClientModeCombo->addItem(tr("HTTPS only"));
    tlsClientModeCombo->setToolTip(
        tr("For a public server choose «HTTPS only». A paired device "
           "overrides this with its own transport."));
    tlsGrid->addWidget(tlsClientModeCombo, 6, 1, 1, 2);

    QLabel *tlsRestartHint=new QLabel(
        tr("Restart the Sync server to apply TLS changes."), tlsBox);
    tlsRestartHint->setStyleSheet(QStringLiteral("color: #9E9E9E;"));
    tlsGrid->addWidget(tlsRestartHint, 7, 0, 1, 3);

    layout->addWidget(tlsBox);

    connect(tlsServerCheck, &QCheckBox::toggled, this, [this](bool checked) {
        this->config->set_syncTlsEnabled(checked);
    });
    connect(advertiseHostEdit, &QLineEdit::editingFinished, this, [this]() {
        this->config->set_syncAdvertiseHost(advertiseHostEdit->text());
    });
    connect(tlsClientModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        this->config->set_syncTlsClientMode(index);
    });
    connect(tlsCertButton, &QPushButton::clicked, this, [this]() {
        const QString path=QFileDialog::getOpenFileName(
            this, tr("TLS certificate"), this->config->get_syncTlsCertPath(),
            tr("PEM certificates (*.pem *.crt);;All files (*)"));
        if(path.isEmpty())
            return;
        this->config->set_syncTlsCertPath(path);
        this->config->set_syncTlsFingerprint(
            SyncTls::certificateFingerprint(path));
        refreshList();
    });
    connect(tlsKeyButton, &QPushButton::clicked, this, [this]() {
        const QString path=QFileDialog::getOpenFileName(
            this, tr("TLS private key"), this->config->get_syncTlsKeyPath(),
            tr("PEM keys (*.pem *.key);;All files (*)"));
        if(!path.isEmpty())
            this->config->set_syncTlsKeyPath(path);
    });
    connect(tlsGenerateButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onGenerateSelfSigned);

    // Адрес ведёт отдельный диалог «Подключиться по адресу…»: держать
    // Host/Port в главном окне рядом с кнопкой Sync было двусмысленно
    // (две почти одинаковые кнопки синка).
    hostEdit=new QLineEdit(this);
    hostEdit->setText(config->get_syncLastHost());
    hostEdit->setPlaceholderText(tr("192.168.1.2"));
    hostEdit->setVisible(false);

    portSpin=new QSpinBox(this);
    portSpin->setRange(1, 65535);
    portSpin->setValue(47777);
    portSpin->setVisible(false);

    scanButton=new QPushButton(tr("Scan"), this);
    scanButton->setToolTip(tr("Find sync hubs in the local network "
                              "and refresh their addresses"));

    QHBoxLayout *buttons=new QHBoxLayout();
    syncButton=new QPushButton(tr("Sync selected"), this);
    syncButton->setToolTip(tr("Sync with the selected device"));
    QPushButton *manualSyncButton=new QPushButton(
        tr("Connect by address..."), this);
    manualSyncButton->setToolTip(tr("Connect to a hub by IP/port "
                                    "(also sends the pair request)"));
    pairButton=new QPushButton(tr("Pair as client..."), this);
    pairButton->setToolTip(tr("Connect to a device that runs a sync server "
                              "(normally a desktop). For a phone that cannot "
                              "accept connections use Show code."));
    acceptButton=new QPushButton(tr("Accept"), this);
    acceptButton->setToolTip(tr("Accept the incoming pair request "
                               "from the selected device"));
    showCodeButton=new QPushButton(tr("Show code..."), this);
    showCodeButton->setToolTip(tr("Generate a one-time code/QR for the "
                                  "selected device (for a phone that cannot "
                                  "accept incoming connections)."));
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
    buttons->addWidget(showCodeButton);
    buttons->addWidget(unpairButton);
    buttons->addWidget(removeButton);
    buttons->addWidget(manualSyncButton);
    buttons->addWidget(scanButton);
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
    connect(showCodeButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onShowCode);
    connect(unpairButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onUnpair);
    connect(removeButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onRemove);
    connect(resetButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onResetAll);
    connect(manualSyncButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onConnectByAddress);
    connect(scanButton, &QPushButton::clicked,
            this, &SyncDevicesDialog::onScan);
    connect(closeButton, &QPushButton::clicked,
            this, &QDialog::accept);

    // Живое обновление без опроса: список sync-устройств меняется снаружи
    // (входящий hello, pair-request, Accept на другой стороне) — конфиг
    // сигналит, окно перечитывает. Выделение строки сохраняется по id
    connect(config, &AppConfig::syncDevicesChanged,
            this, &SyncDevicesDialog::refreshList);
    connect(config, &AppConfig::syncServerSettingsChanged,
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


void SyncDevicesDialog::configureTls(SyncClient &client, const QString &host,
                                     quint16 port, bool tls)
{
    const int mode=tls ? 2 : config->get_syncTlsClientMode();
    client.setTlsMode(static_cast<SyncClient::TlsMode>(mode));
    client.setTlsPin(config->get_syncTlsPin(host, port));
    client.setTlsTrustCallback(
        [this](const QString &h, quint16 p, const QSslCertificate &,
               const QString &fingerprint) {
        return SyncPairDialogs::askCertTrust(this, h, p, fingerprint);
    });
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
    id=table->item(row, 5)->text();
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

    QSet<QString> ghostSet;

    for(const QString &ghostId : ghostIds)
    {
        if(ghostId==config->get_syncDeviceId())
            continue;

        AppConfig::SyncDeviceInfo ghost;
        ghost.id=ghostId;
        devices << ghost;
        ghostSet.insert(ghostId);
    }

    table->setRowCount(devices.size());
    emptyHint->setVisible(devices.isEmpty());

    for(int row=0; row<devices.size(); ++row)
    {
        const AppConfig::SyncDeviceInfo &info=devices.at(row);
        const bool isGhost=ghostSet.contains(info.id);

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
        // wait-accept — входящий запрос: Accept покажет одноразовый код.
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
                pairedTip=tr("Pair request sent. If that device shows a "
                             "one-time code, enter it there (legacy PIN path: "
                             "press Accept on that side). Press Pair again "
                             "to re-check whether it was accepted.");
            }
        }
        else if(info.pairOffer)
        {
            pairedText=tr("wait-accept");
            pairedTip=tr("Incoming pair request from this device. "
                         "Press Accept to show the one-time code, "
                         "then enter it on that device.");
        }
        else
        {
            pairedText=tr("no");
        }

        QTableWidgetItem *pairedItem=new QTableWidgetItem(pairedText);

        if(!pairedTip.isEmpty())
            pairedItem->setToolTip(pairedTip);
        // Последний синк: время + ok/error; полная сводка — в подсказке
        QString lastSyncText;

        if(info.lastSync>0)
        {
            lastSyncText=QDateTime::fromMSecsSinceEpoch(info.lastSync)
                             .toString(QStringLiteral("dd.MM hh:mm"))
                         +(info.lastSyncOk ? QStringLiteral(" ✓")
                                           : QStringLiteral(" ✗"));
        }
        else
            lastSyncText=QStringLiteral("—");

        QTableWidgetItem *lastSyncItem=new QTableWidgetItem(lastSyncText);

        if(!info.lastSyncSummary.isEmpty())
            lastSyncItem->setToolTip(info.lastSyncSummary);

        if(info.lastSync>0 && !info.lastSyncOk)
            lastSyncItem->setForeground(QBrush(QColor(0xEF, 0x53, 0x50)));

        QTableWidgetItem *idItem=new QTableWidgetItem(info.id);

        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, addrItem);
        table->setItem(row, 2, seenItem);
        table->setItem(row, 3, pairedItem);
        table->setItem(row, 4, lastSyncItem);
        table->setItem(row, 5, idItem);

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

    // TLS-настройки — из конфига (без рекурсии сигналов)
    if(tlsServerCheck!=nullptr)
    {
        const QSignalBlocker b1(tlsServerCheck);
        const QSignalBlocker b2(advertiseHostEdit);
        const QSignalBlocker b3(tlsClientModeCombo);

        tlsServerCheck->setChecked(config->get_syncTlsEnabled());
        advertiseHostEdit->setText(config->get_syncAdvertiseHost());
        tlsClientModeCombo->setCurrentIndex(config->get_syncTlsClientMode());

        const QString cert=config->get_syncTlsCertPath();
        const QString key=config->get_syncTlsKeyPath();
        QString fp=config->get_syncTlsFingerprint();

        if(fp.isEmpty() && !cert.isEmpty())
            fp=SyncTls::certificateFingerprint(cert);

        tlsCertLabel->setText(cert.isEmpty()
            ? tr("Certificate: not set") : tr("Certificate: %1").arg(cert));
        tlsKeyLabel->setText(key.isEmpty()
            ? tr("Private key: not set") : tr("Private key: %1").arg(key));
        tlsFingerprintLabel->setText(fp.isEmpty()
            ? tr("Fingerprint: not available")
            : tr("Fingerprint: %1").arg(SyncTls::formatFingerprint(fp)));
    }

    // Настройки серверной стороны — из конфига (без рекурсии сигналов)
    if(blockPairingCheck!=nullptr)
    {
        const QSignalBlocker block(blockPairingCheck);
        blockPairingCheck->setChecked(config->get_syncPairingBlocked());
    }

    if(bindIpCheck!=nullptr)
    {
        const QSignalBlocker block(bindIpCheck);
        bindIpCheck->setChecked(config->get_syncPairBindIp());
    }
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
        acceptButton->setEnabled(false);
        showCodeButton->setEnabled(false);
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
    showCodeButton->setEnabled(true);
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

    // Транспорт выбираем по устройству: спарено по TLS — только HTTPS,
    // известно как plain — HTTP. Ручной адрес задаёт режим в диалоге
    config->set_syncTlsClientMode(target.tls ? 2 : 0);

    if(target.tls && !target.tlsFingerprint.isEmpty())
        config->set_syncTlsPin(target.lastIp, target.port,
                               target.tlsFingerprint);

    SyncSession session(model, config, tetradir);
    session.setTlsTrustResolver(
        [this](const QString &host, quint16 port, const QSslCertificate &,
               const QString &fingerprint) {
        return SyncPairDialogs::askCertTrust(this, host, port, fingerprint);
    });

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
        [this, &target, &session](const SyncPreview::Preview &preview,
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

            SyncPreview::Decisions initial=SyncPreview::Decisions::defaults();
            const QString saved=config->get_syncPreviewActions();

            if(!saved.isEmpty())
                initial=SyncPreview::defaultsFromString(saved);

            SyncPreviewDialog dialog(preview, initial, ownText, otherText, this);

            // Та же семантика «Применить», что и из тулбара: неразобранное
            // не применяется молча, а остаётся на следующий заход.
            dialog.setPartialSyncer(
                [&session](const QMap<QString, SyncPreview::Action> &actions) -> bool {
                    return session.syncPartialNow(actions);
                });
            dialog.setSaveDefaults(
                [this](const SyncPreview::Decisions &d) {
                    config->set_syncPreviewActions(
                        SyncPreview::serializeDefaults(d));
                });

            if(dialog.exec()!=QDialog::Accepted)
                return false;

            decisions=dialog.decisions();

            return true;
        });

    // В сессию код не передаем: секрет уже есть. Починка при расхождении —
    // через Pair заново (осознанно, с показом фингерпринта выше).
    // Исключение — чужая сеть: там нужен новый код, чтобы войти
    MainWindow *mw=qobject_cast<MainWindow *>(parentWidget());

    // Перед сессией сохраняем редактируемую запись (нативный путь не
    // делает reloadSaveStage), после — обновляем откреплённые окна:
    // сессия уже перечитала модель.
    if(mw!=nullptr)
        mw->saveTextarea();

    const auto runSession=[&](const QString &code) -> SyncReport {
        // Фоновый таймер проверяет этот флаг: без него processEvents
        // прогресса мог запустить второй синк поверх первого.
        struct BusyGuard {
            MainWindow *w;
            ~BusyGuard() { if(w!=nullptr) w->setNativeSyncBusy(false); }
        } busyGuard{mw};

        if(mw!=nullptr)
            mw->setNativeSyncBusy(true);

        SyncProgressWindow progress(session, this);
        const SyncReport r=session.run(target.lastIp,
                                       static_cast<quint16>(target.port),
                                       code);

        // Последний результат для таблицы устройств
        config->set_syncDeviceLastResult(
            target.id, r.ok,
            r.ok ? tr("↑ notes %1, branches %2; ↓ notes %3, branches %4; deletes %5")
                       .arg(r.uploadedNotes).arg(r.uploadedBranches)
                       .arg(r.downloadedNotes).arg(r.downloadedBranches)
                       .arg(r.appliedDeletes)
                 : r.error);

        if(mw!=nullptr)
            mw->finishNativeSyncUi();
        return r;
    };

    SyncReport report=runSession(QString());

    if(!report.ok
       && report.errorCode==QStringLiteral("foreign-network"))
    {
        const QString peerName=target.name.isEmpty() ? target.id : target.name;

        if(!confirmJoin(peerName))
        {
            refreshList();
            return;
        }

        // Вход в чужую сеть: забываем локальную группу/пары, просим
        // новый одноразовый код и спариваемся заново
        joinFresh();

        SyncClient probe(config);
        probe.setTimeoutMs(6000);
        configureTls(probe, target.lastIp, static_cast<quint16>(target.port),
                     target.tls);
        SyncHelloResult hello=probe.hello(target.lastIp,
            static_cast<quint16>(target.port), true);

        if(!hello.ok && !hello.pairCodeRequired
           && hello.errorCode!=QStringLiteral("auth-failed"))
        {
            QMessageBox::warning(
                this, tr("Sync"),
                tr("Handshake failed: %1")
                .arg(hello.errorText.isEmpty() ? hello.errorCode
                                               : hello.errorText));
            refreshList();
            return;
        }

        if(!hello.pair2)
        {
            QMessageBox::warning(
                this, tr("Sync pairing"),
                tr("The other device does not support one-time code pairing. "
                   "Update MyTetra on it and try again."));
            refreshList();
            return;
        }

        if(hello.pairCodeLocked)
        {
            SyncPairDialogs::showLocked(this, hello.pairCodeUntil);
            refreshList();
            return;
        }

        const QString code=SyncPairDialogs::askCode(this, peerName);

        if(code.isEmpty())
        {
            refreshList();
            return;
        }

        report=runSession(code);
    }

    if(!report.ok)
    {
        QString fail=tr("Sync failed: %1").arg(report.error);
        const QString steps=session.log().join(QStringLiteral("\n"));

        if(!steps.isEmpty())
            fail+=QStringLiteral("\n-- steps --\n")+steps;

        MainWindow::showSyncReport(this, tr("Sync"),
                                   MainWindow::trimSyncReportForDialog(fail),
                                   true);
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

    MainWindow::showSyncReport(this, tr("Sync"),
                               MainWindow::trimSyncReportForDialog(summary),
                               false);
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
    {
        config->touch_syncDevice(device.deviceId, device.deviceName,
                                 device.address, device.port);
        config->set_syncDeviceTls(device.deviceId, device.tls,
                                  device.tlsFingerprint);
    }

    refreshList();

    QMessageBox::information(this, tr("Scan"), found.isEmpty()
        ? tr("No devices found. Check that the sync server is running "
             "in the same network on port %1.").arg(port)
        : tr("Found %1 device(s) in the local network.").arg(found.size()));
}


// «Подключиться по адресу…»: адрес/порт в отдельном диалоге, дальше —
// прежний ручной путь (hello, фингерпринт, PIN, runSyncToHeadless).
void SyncDevicesDialog::onConnectByAddress(void)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Connect by address"));

    QVBoxLayout *layout=new QVBoxLayout(&dialog);

    QLineEdit *host=new QLineEdit(hostEdit->text(), &dialog);
    host->setPlaceholderText(QStringLiteral("192.168.1.2"));
    QSpinBox *port=new QSpinBox(&dialog);
    port->setRange(1, 65535);
    port->setValue(portSpin->value());

    layout->addWidget(new QLabel(tr("Hub address (IP):"), &dialog));
    layout->addWidget(host);
    layout->addWidget(new QLabel(tr("Port:"), &dialog));
    layout->addWidget(port);

    QCheckBox *useTls=new QCheckBox(tr("Use TLS (HTTPS)"), &dialog);
    useTls->setChecked(config->get_syncTlsClientMode()!=0);
    useTls->setToolTip(tr("For a public server use HTTPS with a trusted "
                          "certificate (Let's Encrypt) or confirm the "
                          "self-signed fingerprint."));
    layout->addWidget(useTls);

    QDialogButtonBox *box=new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(box);

    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if(dialog.exec()!=QDialog::Accepted)
        return;

    hostEdit->setText(host->text().trimmed());
    portSpin->setValue(port->value());
    config->set_syncTlsClientMode(useTls->isChecked()?2:0);

    onManualSync();
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

    QString code;
    QString errorCode;
    bool syncOk=false;
    QString result;

    // Секрета нет: сначала просим сервер показать код (pairRequest).
    // Секрет есть: идем сразу в runSyncToHeadless, проба при чужой
    // сети упала бы раньше вопроса о переходе
    if(!config->hasSyncPairing())
    {
        SyncClient probe(config);
        probe.setTimeoutMs(6000);
        configureTls(probe, host, port,
                     config->get_syncTlsClientMode()!=0);
        SyncHelloResult hello=probe.hello(host, port, true);

        // Чужая сеть: чтобы войти, забываем локальную группу/пары.
        // Пустая группа втягивается сервером любой версии
        if(hello.errorCode==QStringLiteral("foreign-network"))
        {
            if(!confirmJoin(hello.remoteDeviceName.isEmpty()
                                ? hello.remoteDeviceId
                                : hello.remoteDeviceName,
                            hello.remoteGroupId))
            {
                refreshList();
                return;
            }

            joinFresh();
            hello=probe.hello(host, port, true);
        }

        if(hello.errorCode==QStringLiteral("network"))
        {
            QMessageBox::warning(this, tr("Sync"),
                                 tr("Handshake failed: %1").arg(hello.errorText));
            refreshList();
            return;
        }

        if(!hello.ok && !hello.pairCodeRequired
           && hello.errorCode!=QStringLiteral("auth-failed"))
        {
            QMessageBox::warning(
                this, tr("Sync"),
                tr("Handshake failed: %1")
                .arg(hello.errorText.isEmpty() ? hello.errorCode
                                               : hello.errorText));
            refreshList();
            return;
        }

        if(!hello.pair2)
        {
            QMessageBox::warning(
                this, tr("Sync pairing"),
                tr("The other device does not support one-time code pairing. "
                   "Update MyTetra on it and try again."));
            refreshList();
            return;
        }

        if(hello.pairCodeLocked)
        {
            SyncPairDialogs::showLocked(this, hello.pairCodeUntil);
            refreshList();
            return;
        }

        code=SyncPairDialogs::askCode(
            this,
            hello.remoteDeviceName.isEmpty() ? hello.remoteDeviceId
                                             : hello.remoteDeviceName);

        if(code.isEmpty())
        {
            refreshList();
            return;
        }
    }

    result=win->runSyncToHeadless(host, code, port,
                                  syncOk, &errorCode);

    // Чужая сеть: сервер с парами не пускает чужую группу. Спрашиваем,
    // забываем локальную сеть, просим новый код и входим
    if(!syncOk && errorCode==QStringLiteral("foreign-network"))
    {
        if(!confirmJoin(QString(), QString()))
        {
            refreshList();
            return;
        }

        joinFresh();

        SyncClient probe(config);
        probe.setTimeoutMs(6000);
        configureTls(probe, host, port,
                     config->get_syncTlsClientMode()!=0);
        SyncHelloResult hello=probe.hello(host, port, true);

        if(!hello.ok && !hello.pairCodeRequired
           && hello.errorCode!=QStringLiteral("auth-failed"))
        {
            QMessageBox::warning(
                this, tr("Sync"),
                tr("Handshake failed: %1")
                .arg(hello.errorText.isEmpty() ? hello.errorCode
                                               : hello.errorText));
            refreshList();
            return;
        }

        if(!hello.pair2)
        {
            QMessageBox::warning(
                this, tr("Sync pairing"),
                tr("The other device does not support one-time code pairing. "
                   "Update MyTetra on it and try again."));
            refreshList();
            return;
        }

        if(hello.pairCodeLocked)
        {
            SyncPairDialogs::showLocked(this, hello.pairCodeUntil);
            refreshList();
            return;
        }

        code=SyncPairDialogs::askCode(
            this,
            hello.remoteDeviceName.isEmpty() ? hello.remoteDeviceId
                                             : hello.remoteDeviceName);

        if(code.isEmpty())
        {
            refreshList();
            return;
        }

        result=win->runSyncToHeadless(host, code, port, syncOk);
    }

    showManualResult(result, syncOk);
}


// Подтверждение входа в чужую сеть. Локальная пара (секрет, группа,
// привязка) будет забыта; дальше нужен одноразовый код сервера
bool SyncDevicesDialog::confirmJoin(const QString &peerName,
                                    const QString &peerGroup)
{
    QString text=tr("This server is in another sync network.\n"
                    "Join it? Local pairing (secret, group) will be "
                    "forgotten, then you will need its one-time pairing code.");

    if(!peerName.isEmpty())
        text+=tr("\n\nServer: %1").arg(peerName);

    if(!peerGroup.isEmpty())
        text+=tr("\nIts group: %1").arg(peerGroup);

    return QMessageBox::question(this, tr("Sync"), text)==QMessageBox::Yes;
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
    MainWindow::showSyncReport(this, tr("Sync"),
                               MainWindow::trimSyncReportForDialog(result),
                               !syncOk);

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
                                "Use «Connect by address...» and enter its IP "
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
        configureTls(probe, target.lastIp,
                     static_cast<quint16>(target.port), target.tls);
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

    MainWindow *win=qobject_cast<MainWindow *>(parentWidget());

    if(win==nullptr)
        return;

    const quint16 port=static_cast<quint16>(target.port);
    const QString peerName=target.name.isEmpty() ? id : target.name;

    // 1. Запрос в пару: сервер сгенерирует код и покажет его оператору
    SyncClient probe(config);
    probe.setTimeoutMs(6000);
    configureTls(probe, target.lastIp, port, target.tls);
    SyncHelloResult hello=probe.hello(target.lastIp, port, true);

    // Чужая сеть: чтобы войти, сначала забываем локальную группу/пары.
    // Пустая группа втягивается сервером любой версии
    if(hello.errorCode==QStringLiteral("foreign-network"))
    {
        if(!confirmJoin(peerName, hello.remoteGroupId))
            return;

        joinFresh();
        hello=probe.hello(target.lastIp, port, true);
    }

    if(hello.errorCode==QStringLiteral("network"))
    {
        QMessageBox::warning(this, tr("Sync pairing"),
                             tr("Cannot reach %1: %2")
                             .arg(peerName, hello.errorText));
        return;
    }

    // Прочие отказы (версии/часы/self) — показываем как есть
    if(!hello.ok && !hello.pairCodeRequired
       && hello.errorCode!=QStringLiteral("auth-failed"))
    {
        QMessageBox::warning(
            this, tr("Sync pairing"),
            tr("Handshake failed: %1")
            .arg(hello.errorText.isEmpty() ? hello.errorCode
                                           : hello.errorText));
        return;
    }

    // Сервер без sync/2: PIN-путь намеренно не предлагаем — обновите его
    if(!hello.pair2)
    {
        QMessageBox::warning(
            this, tr("Sync pairing"),
            tr("The other device does not support one-time code pairing. "
               "Update MyTetra on it and try again."));
        return;
    }

    if(hello.pairCodeLocked)
    {
        SyncPairDialogs::showLocked(this, hello.pairCodeUntil);
        return;
    }

    // 2. Код показан на сервере — вводим его здесь
    const QString code=SyncPairDialogs::askCode(this, peerName);

    if(code.isEmpty())
        return;

    config->set_syncTlsClientMode(target.tls ? 2 : 0);
    config->set_syncTlsPin(target.lastIp, port,
                           config->get_syncTlsPin(target.lastIp, port));

    bool syncOk=false;
    QString errorCode;
    const QString result=win->runSyncToHeadless(target.lastIp, code, port,
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
    configureTls(probe, ip, port, false);

    const SyncHelloResult hello=probe.hello(ip, port);

    if(hello.ok && hello.paired)
    {
        qDebug() << "SyncPair: request accepted, sync unlocked";
        // Взаимность — ИМЕННО тому, кто ответил, а не первой паре кольца
        // (иначе второй пир навсегда остаётся невзаимным, таймер крутится).
        const QString acceptedId=hello.remoteDeviceId.isEmpty()
            ? config->get_syncPairedDeviceId() : hello.remoteDeviceId;
        SyncHandshake::noteMutual(config, acceptedId, "auto re-check");
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

    // Сервер: показать одноразовый код. Клиент вводит его у себя,
    // после чего первый hello с новым секретом подтвердит пару
    bool rejected=false;
    QString shownName=target.name.isEmpty() ? id : target.name;

    if(!target.lastIp.isEmpty())
        shownName+=QStringLiteral(" (%1)").arg(target.lastIp);

    QString qrHost=config->get_syncAdvertiseHost();

    if(qrHost.isEmpty())
        qrHost=SyncHandshake::lastLocalAddress();

    if(qrHost.isEmpty())
        qrHost=SyncTls::primaryAddress();

    if(qrHost.isEmpty())
        qrHost=QHostInfo::localHostName();

    QString qrFingerprint=config->get_syncTlsFingerprint();

    if(qrFingerprint.isEmpty() && !config->get_syncTlsCertPath().isEmpty())
        qrFingerprint=SyncTls::certificateFingerprint(
            config->get_syncTlsCertPath());

    SyncPairDialogs::showCode(this, id, shownName,
                              QDateTime::currentMSecsSinceEpoch(),
                              &rejected, target.lastIp,
                              config->get_syncPairBindIp(),
                              qrHost, serverPort,
                              config->get_syncTlsEnabled(),
                              qrFingerprint,
                              config->get_syncDeviceName());

    if(rejected)
    {
        config->clear_syncDeviceOffer(id);
        config->clear_syncDevicePendingSecret(id);
    }

    refreshList();
}


// Серверная сторона: показать код/QR для выбранного устройства.
// Нужно телефону, который не слушает порт и не может быть «хабом».
void SyncDevicesDialog::onShowCode(void)
{
    QString id;

    if(currentRowId(id)<0)
        return;

    AppConfig::SyncDeviceInfo target;

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
        if(info.id==id)
        {
            target=info;
            break;
        }

    const QString shownName=target.name.isEmpty() ? id : target.name;

    // Помечаем входящий запрос, но без второго попапа: этот диалог
    // и есть серверный экран кода/QR. Повторный pairRequest от
    // телефона не должен открыть ещё одно окно
    config->set_syncDeviceOffer(id, false);

    QString qrHost=config->get_syncAdvertiseHost();

    if(qrHost.isEmpty())
        qrHost=SyncHandshake::lastLocalAddress();

    if(qrHost.isEmpty())
        qrHost=SyncTls::primaryAddress();

    if(qrHost.isEmpty())
        qrHost=QHostInfo::localHostName();

    QString qrFingerprint=config->get_syncTlsFingerprint();

    if(qrFingerprint.isEmpty() && !config->get_syncTlsCertPath().isEmpty())
        qrFingerprint=SyncTls::certificateFingerprint(
            config->get_syncTlsCertPath());

    bool rejected=false;
    SyncPairDialogs::showCode(this, id, shownName,
                              QDateTime::currentMSecsSinceEpoch(),
                              &rejected, target.lastIp,
                              config->get_syncPairBindIp(),
                              qrHost, serverPort,
                              config->get_syncTlsEnabled(),
                              qrFingerprint,
                              config->get_syncDeviceName());

    if(rejected)
    {
        config->clear_syncDeviceOffer(id);
        config->clear_syncDevicePendingSecret(id);
    }

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
    SyncPairing::clearCode(id);

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
        SyncPairing::clearCode(id);

        if(config->get_syncPairedDeviceId()==id)
        {
            config->set_syncPairingSecret(QString());
            config->set_syncPairedDeviceId(QString());
            SyncSidecar::reset();
        }
    }

    // Недоигранная пара по коду: pending/код тоже не должны пережить
    // удаление строки, иначе клиент может допариться уже после Remove
    config->clear_syncDevicePendingSecret(id);
    SyncPairing::clearCode(id);

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


// Сгенерировать self-signed cert/key через openssl CLI
void SyncDevicesDialog::onGenerateSelfSigned(void)
{
    const QString dir=QFileInfo(config->getConfigFileName()).absolutePath()
                      +QStringLiteral("/sync-tls");
    QString host=advertiseHostEdit->text().trimmed();

    if(host.isEmpty())
        host=SyncTls::primaryAddress();

    if(host.isEmpty())
        host=QHostInfo::localHostName();

    const QString cert=dir+QStringLiteral("/server-cert.pem");
    const bool exists=QFile::exists(cert);

    if(exists && QMessageBox::question(
           this, tr("TLS"),
           tr("A self-signed certificate already exists. Regenerate it? "
              "Existing clients will need to trust the new fingerprint."))
       !=QMessageBox::Yes)
        return;

    QString certPath;
    QString keyPath;
    QString error;

    if(!SyncTls::ensureSelfSigned(dir, host, &certPath, &keyPath, &error,
                                  exists))
    {
        QMessageBox::warning(this, tr("TLS"), error);
        return;
    }

    config->set_syncTlsCertPath(certPath);
    config->set_syncTlsKeyPath(keyPath);
    config->set_syncTlsFingerprint(
        SyncTls::certificateFingerprint(certPath));
    config->set_syncTlsEnabled(true);

    if(advertiseHostEdit->text().trimmed().isEmpty())
        config->set_syncAdvertiseHost(host);

    refreshList();

    QMessageBox::information(
        this, tr("TLS"),
        tr("Self-signed certificate created.\n\n"
           "Fingerprint:\n%1\n\n"
           "Restart the Sync server. Clients will be asked to trust this "
           "fingerprint (or scan the QR).")
        .arg(SyncTls::formatFingerprint(
            config->get_syncTlsFingerprint())));
}


// Запрет новых пар: подтверждаем, отменяем незавершённые пары
void SyncDevicesDialog::onBlockPairingToggled(bool checked)
{
    if(checked)
    {
        if(QMessageBox::question(
               this, tr("Sync"),
               tr("Block new pairings?\n\n"
                  "Existing devices keep syncing. Any pairing in progress "
                  "will be cancelled (codes and pending secrets)."))
           !=QMessageBox::Yes)
        {
            const QSignalBlocker block(blockPairingCheck);
            blockPairingCheck->setChecked(false);
            return;
        }

        SyncPairing::clearAll();

        for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
            config->clear_syncDevicePendingSecret(info.id);
    }

    config->set_syncPairingBlocked(checked);
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
    SyncPairing::clearAll();

    for(const AppConfig::SyncDeviceInfo &info : config->get_syncDevices())
        config->remove_syncDevice(info.id);

    // Журнал синка — в новую эпоху
    SyncSidecar::reset();

    refreshList();
}

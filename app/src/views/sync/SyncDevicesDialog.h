#ifndef SYNCDEVICESDIALOG_H
#define SYNCDEVICESDIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>

#include "models/appConfig/AppConfig.h"

class KnowTreeModel;
class QTableWidget;
class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QTimer;
class SyncClient;

// Окно Sync devices: известные пиры (кто стучался в hello / к кому ходили).
// Кнопки действий напротив каждого: Sync now, Pair, Unpair, Remove.
// Отдельное меню Sync (не Tools): синк — подсистема, а не утилита
class SyncDevicesDialog : public QDialog
{
    Q_OBJECT

public:

    explicit SyncDevicesDialog(KnowTreeModel *model, AppConfig *config,
                               const QString &tetradir,
                               quint16 serverPort=47777,
                               QWidget *parent=nullptr);

private slots:

    void refreshList(void);
    void onSelectionChanged(void);
    void onSyncNow(void);
    void onManualSync(void);
    void onConnectByAddress(void);
    void onScan(void);
    void onPair(void);
    void onRecheckPair(void);
    void onAccept(void);
    void onUnpair(void);
    void onRemove(void);
    void onResetAll(void);
    void onBlockPairingToggled(bool checked);
    void onGenerateSelfSigned(void);

private:

    // Подтверждение входа в чужую сеть. Честно предупреждает, что
    // локальная пара/группа будут забыты, дальше нужен код сервера
    bool confirmJoin(const QString &peerName,
                     const QString &peerGroup=QString());

    // Настроить исходящий SyncClient: режим TLS, пин сертификата,
    // диалог доверия (self-signed/private CA)
    void configureTls(SyncClient &client, const QString &host,
                      quint16 port, bool tls);

    // Свое состояние пары в том же окне (без отдельных команд)
    void refreshSelf(void);

    // Сброс локальной пары перед входом в чужую сеть
    void joinFresh(void);

    // Показ итога ручного синка
    void showManualResult(const QString &result, bool syncOk);

    // Строка считается спаренной: секрет есть и выведен именно с ней.
    // Взаимность (isMutual) строже: обмен с проверкой уже был, Sync можно
    bool isPaired(const AppConfig::SyncDeviceInfo &info) const;
    bool isMutual(const AppConfig::SyncDeviceInfo &info) const;

    // Свой неотвеченный pair-request: секрет и привязка есть, взаимности
    // нет. В ip/port (если даны) — куда перепроверять; пустой ip значит
    // некуда стучаться (призрак без адреса)
    bool ownRequestPending(QString *ip=nullptr,
                           quint16 *port=nullptr) const;
    int currentRowId(QString &id) const;

    KnowTreeModel *model=nullptr;
    AppConfig *config=nullptr;
    QString tetradir;
    quint16 serverPort=47777;

    QTableWidget *table=nullptr;
    QLabel *fingerprintLabel=nullptr;
    QLabel *selfLabel=nullptr;
    QLabel *emptyHint=nullptr;
    QLineEdit *hostEdit=nullptr;
    QSpinBox *portSpin=nullptr;
    QPushButton *scanButton=nullptr;
    QPushButton *syncButton=nullptr;
    QCheckBox *blockPairingCheck=nullptr;
    QCheckBox *bindIpCheck=nullptr;
    QCheckBox *tlsServerCheck=nullptr;
    QLineEdit *advertiseHostEdit=nullptr;
    QPushButton *tlsCertButton=nullptr;
    QPushButton *tlsKeyButton=nullptr;
    QPushButton *tlsGenerateButton=nullptr;
    QLabel *tlsCertLabel=nullptr;
    QLabel *tlsKeyLabel=nullptr;
    QLabel *tlsFingerprintLabel=nullptr;
    QComboBox *tlsClientModeCombo=nullptr;
    QPushButton *pairButton=nullptr;
    QPushButton *acceptButton=nullptr;
    QPushButton *unpairButton=nullptr;
    QPushButton *removeButton=nullptr;

    // Узкий добивочный таймер исходящего pair-request (см. ctor)
    QTimer *recheckTimer=nullptr;
};

#endif // SYNCDEVICESDIALOG_H

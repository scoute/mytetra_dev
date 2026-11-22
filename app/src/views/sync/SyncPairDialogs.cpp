#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "SyncPairDialogs.h"
#include "libraries/sync/SyncPairCrypto.h"
#include "libraries/sync/SyncPairing.h"
#include "libraries/sync/SyncTls.h"
#include "libraries/sync/SyncPairQr.h"


namespace SyncPairDialogs
{

QString askCode(QWidget *parent, const QString &peerName,
                const QString &detail)
{
    QString normalized;

    while(true)
    {
        QInputDialog dialog(parent);
        dialog.setWindowTitle(QStringLiteral("Код спаривания"));
        dialog.setLabelText(
            QStringLiteral("Введите 16-символьный код, показанный на %1.\n"
                           "Код одноразовый и действует 5 минут.%2")
                .arg(peerName.isEmpty() ? QStringLiteral("другом устройстве")
                                        : peerName,
                     detail.isEmpty() ? QString()
                                      : QStringLiteral("\n")+detail));
        dialog.setInputMode(QInputDialog::TextInput);
        dialog.setTextEchoMode(QLineEdit::Normal);
        dialog.setOkButtonText(QStringLiteral("Спарить"));

        QLineEdit *edit=dialog.findChild<QLineEdit *>();

        if(edit!=nullptr)
        {
            edit->setPlaceholderText(QStringLiteral("ABCD-EFGH-IJKL-MNOP"));
            edit->setFont(QFontDatabase::systemFont(
                QFontDatabase::FixedFont));
            edit->setMaxLength(24);
            edit->setAlignment(Qt::AlignCenter);
        }

        if(dialog.exec()!=QDialog::Accepted)
            return QString();

        normalized=SyncPairCrypto::normalizeCode(dialog.textValue());

        if(SyncPairCrypto::isValidCode(normalized))
            return normalized;

        QMessageBox::warning(
            parent, QStringLiteral("Код спаривания"),
            QStringLiteral("Неверный формат кода. Нужно 16 символов "
                           "A-Z и 2-7 (можно с дефисами)."));
    }
}


QString showCode(QWidget *parent, const QString &peerId,
                 const QString &peerName, qint64 now, bool *rejected,
                 const QString &peerIp, bool bindIp,
                 const QString &qrHost, quint16 qrPort, bool qrTls,
                 const QString &qrFingerprint, const QString &serverName)
{
    if(rejected!=nullptr)
        *rejected=false;

    qint64 expiresAt=0;
    qint64 lockedUntil=0;
    QString code=SyncPairing::ensureCode(peerId, now, peerIp, bindIp,
                                         &expiresAt, &lockedUntil);

    if(code.isEmpty())
    {
        if(lockedUntil>0)
            showLocked(parent, lockedUntil);
        else
            QMessageBox::warning(
                parent, QStringLiteral("Спаривание"),
                QStringLiteral("Не удалось создать код. Повторите попытку."));

        return QString();
    }

    while(true)
    {
        const QString shown=SyncPairCrypto::formatCode(code);
        const QString until=QDateTime::fromMSecsSinceEpoch(expiresAt)
                                 .toString(QStringLiteral("HH:mm"));
        const QString uri=(!qrHost.isEmpty() && qrPort>0)
            ? SyncPairQr::buildUri(qrHost, qrPort, code, qrTls,
                                   qrFingerprint, serverName)
            : QString();
        const QPixmap qr=SyncPairQr::render(uri);

        QDialog dialog(parent);
        dialog.setWindowTitle(QStringLiteral("Спаривание"));
        dialog.setMinimumWidth(360);

        QVBoxLayout *layout=new QVBoxLayout(&dialog);

        QLabel *title=new QLabel(QStringLiteral(
            "Устройство %1 хочет спариться.")
            .arg(peerName.isEmpty() ? peerId.left(8) : peerName), &dialog);
        title->setWordWrap(true);
        layout->addWidget(title);

        QLabel *codeLabel=new QLabel(
            QStringLiteral("Код: %1").arg(shown), &dialog);
        QFont mono=QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPointSize(16);
        mono.setBold(true);
        codeLabel->setFont(mono);
        codeLabel->setAlignment(Qt::AlignCenter);
        codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse
                                           | Qt::TextSelectableByKeyboard);
        layout->addWidget(codeLabel);

        if(!qr.isNull())
        {
            QLabel *qrLabel=new QLabel(&dialog);
            qrLabel->setPixmap(qr);
            qrLabel->setAlignment(Qt::AlignCenter);
            qrLabel->setStyleSheet(QStringLiteral(
                "background: white; padding: 8px;"));
            layout->addWidget(qrLabel, 0, Qt::AlignCenter);
        }

        QLabel *hint=new QLabel(QStringLiteral(
            "Отсканируйте QR на телефоне или введите код вручную. "
            "Код одноразовый, действует до %1.").arg(until), &dialog);
        hint->setWordWrap(true);
        layout->addWidget(hint);

        QHBoxLayout *buttons=new QHBoxLayout();
        QPushButton *copy=new QPushButton(QStringLiteral("Копировать код"),
                                          &dialog);
        QPushButton *renew=new QPushButton(QStringLiteral("Новый код"),
                                           &dialog);
        QPushButton *reject=new QPushButton(QStringLiteral("Отклонить"),
                                            &dialog);
        QPushButton *close=new QPushButton(QStringLiteral("Закрыть"),
                                           &dialog);
        buttons->addWidget(copy);
        buttons->addWidget(renew);
        buttons->addWidget(reject);
        buttons->addStretch();
        buttons->addWidget(close);
        layout->addLayout(buttons);

        QAbstractButton *clicked=nullptr;
        const auto bind=[&](QAbstractButton *button) {
            QObject::connect(button, &QAbstractButton::clicked, &dialog,
                             [&, button]() {
                clicked=button;
                dialog.accept();
            });
        };
        bind(copy);
        bind(renew);
        bind(reject);
        bind(close);
        close->setDefault(true);

        dialog.exec();

        if(clicked==copy)
        {
            QGuiApplication::clipboard()->setText(shown);
            continue;
        }

        if(clicked==renew)
        {
            SyncPairing::clearCode(peerId);
            code=SyncPairing::ensureCode(peerId,
                                         QDateTime::currentMSecsSinceEpoch(),
                                         peerIp, bindIp, &expiresAt,
                                         &lockedUntil);

            if(code.isEmpty())
            {
                if(lockedUntil>0)
                    showLocked(parent, lockedUntil);

                return QString();
            }

            continue;
        }

        if(clicked==reject)
        {
            SyncPairing::clearCode(peerId);

            if(rejected!=nullptr)
                *rejected=true;

            return QString();
        }

        return code;
    }
}


bool askCertTrust(QWidget *parent, const QString &host, quint16 port,
                  const QString &fingerprint)
{
    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("TLS certificate"));
    box.setIcon(QMessageBox::Warning);
    box.setText(QStringLiteral(
        "The TLS certificate of %1:%2 is self-signed or not trusted.\n\n"
        "Fingerprint (SHA-256):\n%3\n\n"
        "Compare it with the fingerprint shown on the server "
        "(QR / devices window). Trust this certificate?")
        .arg(host).arg(port)
        .arg(SyncTls::formatFingerprint(fingerprint)));
    box.setTextInteractionFlags(Qt::TextSelectableByMouse
                                | Qt::TextSelectableByKeyboard);

    QPushButton *trust=box.addButton(QStringLiteral("Trust"),
                                     QMessageBox::AcceptRole);
    QPushButton *cancel=box.addButton(QStringLiteral("Cancel"),
                                      QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.exec();

    return box.clickedButton()==trust;
}


void showLocked(QWidget *parent, qint64 until)
{
    const QString when=until>0
        ? QDateTime::fromMSecsSinceEpoch(until)
              .toString(QStringLiteral("HH:mm"))
        : QStringLiteral("несколько минут");

    QMessageBox::warning(
        parent, QStringLiteral("Спаривание"),
        QStringLiteral("Слишком много неверных кодов. "
                       "Повторите после %1 и запросите новый код.")
            .arg(when));
}

} // namespace SyncPairDialogs

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "SyncPairDialogs.h"
#include "libraries/sync/SyncPairCrypto.h"
#include "libraries/sync/SyncPairing.h"


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
                 const QString &peerName, qint64 now, bool *rejected)
{
    if(rejected!=nullptr)
        *rejected=false;

    qint64 expiresAt=0;
    qint64 lockedUntil=0;
    QString code=SyncPairing::ensureCode(peerId, now, &expiresAt,
                                         &lockedUntil);

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

        QMessageBox box(parent);
        box.setWindowTitle(QStringLiteral("Спаривание"));
        box.setIcon(QMessageBox::Information);
        box.setText(QStringLiteral("Устройство %1 хочет спариться.\n\n"
                                   "Код: %2\n\n"
                                   "Введите этот код на другом устройстве. "
                                   "Он действует до %3 и одноразовый.")
                        .arg(peerName.isEmpty() ? peerId.left(8) : peerName,
                             shown, until));
        box.setTextInteractionFlags(Qt::TextSelectableByMouse
                                    | Qt::TextSelectableByKeyboard);

        QPushButton *copy=box.addButton(QStringLiteral("Копировать код"),
                                        QMessageBox::ActionRole);
        QPushButton *renew=box.addButton(QStringLiteral("Новый код"),
                                         QMessageBox::ActionRole);
        QPushButton *reject=box.addButton(QStringLiteral("Отклонить"),
                                          QMessageBox::DestructiveRole);
        box.addButton(QStringLiteral("Закрыть"), QMessageBox::RejectRole);
        box.setDefaultButton(copy);
        box.exec();

        if(box.clickedButton()==copy)
        {
            QGuiApplication::clipboard()->setText(shown);
            continue;
        }

        if(box.clickedButton()==reject)
        {
            SyncPairing::clearCode(peerId);

            if(rejected!=nullptr)
                *rejected=true;

            return QString();
        }

        if(box.clickedButton()==renew)
        {
            SyncPairing::clearCode(peerId);
            code=SyncPairing::ensureCode(peerId, QDateTime::currentMSecsSinceEpoch(),
                                         &expiresAt, &lockedUntil);

            if(code.isEmpty())
            {
                if(lockedUntil>0)
                    showLocked(parent, lockedUntil);

                return QString();
            }

            continue;
        }

        return code;
    }
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

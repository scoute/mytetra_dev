#ifndef SYNCPAIRDIALOGS_H
#define SYNCPAIRDIALOGS_H

#include <QString>

class QWidget;

// Диалоги спаривания sync/2. Вынесены из MainWindow/SyncDevicesDialog,
// чтобы сервер и клиент показывали одно и то же.
namespace SyncPairDialogs
{

// Клиент: ввести 16-символьный код с экрана другого устройства.
// Пусто — отмена или невалидный код (после предупреждения).
QString askCode(QWidget *parent, const QString &peerName,
                const QString &detail=QString());

// Сервер: показать/обновить код для входящего запроса.
// Возвращает показанный код (пусто — заблокировано/ошибка).
QString showCode(QWidget *parent, const QString &peerId,
                 const QString &peerName, qint64 now,
                 bool *rejected=nullptr);

// Сервер: сообщить о блокировке после серии неверных кодов
void showLocked(QWidget *parent, qint64 until);

} // namespace SyncPairDialogs

#endif // SYNCPAIRDIALOGS_H

#ifndef SYNCNOTEIO_H
#define SYNCNOTEIO_H

#include <QString>
#include <QJsonObject>
#include <QHash>
#include <QMap>

class KnowTreeModel;

// Общая сериализация/применение заметок и веток для обеих сторон:
// хаб дергает через маршруты, клиент — напрямую при загрузках.
// Правила одинаковые: id сохраняются, mtime/hash принимаются чужие
// (adopt, не "сейчас"), шифртекст идет как есть, ключ не нужен.
namespace SyncNoteIo
{

// branchId — подсказка из манифеста: искать запись сразу в её ветке,
// а не обходить всё дерево. Пусто — старый полный поиск
QJsonObject serializeNote(KnowTreeModel *model, const QString &tetradir,
                          const QString &id,
                          const QString &branchId=QString());

QJsonObject serializeBranch(KnowTreeModel *model, const QString &id);

// Возвращают "" при успехе, текст ошибки иначе.
// applyDelete: deletedAt — удаленное время tombstone
// localBranches — id записи -> её ветка в локальном манифесте.
// Позволяет не сканировать всё дерево на каждую новую заметку:
// нет id в индексе = новая, есть = знаем старую ветку для переезда.
// nullptr — старый полный поиск (серверный приём пушей и т.п.)
QString applyNote(KnowTreeModel *model, const QString &tetradir,
                  const QJsonObject &request,
                  const QHash<QString, QString> *localBranches=nullptr);
QString applyBranch(KnowTreeModel *model, const QJsonObject &request);
QString applyDelete(KnowTreeModel *model, const QString &id,
                    qint64 deletedAt);

// Валидация имён/путей от пира — см. SyncValidation.h

// Стереть всю базу локально (стратегии "начать заново"): все ветки
// уходят в корзину штатным удалением, журнал синка стирается целиком
// БЕЗ tombstone'ов: это не удаление данных для пира, а замена
// содержимого (пустое место под полный залив с другой стороны)
QString wipeAll(KnowTreeModel *model);

// Слияние веток по именам на хабе (стратегия "объединение"): ветки из
// clientBranchIds (созданные клиентом, их у хаба не было) вливаются
// в одноименные ветки хаба — id хаба побеждает, записи не сливаются
// (одноименные остаются дублями), пустые оболочки удаляются.
// Рекурсивно по уровням. "" при успехе
QString mergeByName(KnowTreeModel *model,
                    const QStringList &clientBranchIds);

} // namespace SyncNoteIo

#endif // SYNCNOTEIO_H

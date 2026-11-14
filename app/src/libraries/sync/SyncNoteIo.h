#ifndef SYNCNOTEIO_H
#define SYNCNOTEIO_H

#include <QString>
#include <QJsonObject>
#include <QMap>

class KnowTreeModel;

// Общая сериализация/применение заметок и веток для обеих сторон:
// хаб дергает через маршруты, клиент — напрямую при загрузках.
// Правила одинаковые: id сохраняются, mtime/hash принимаются чужие
// (adopt, не "сейчас"), шифртекст идет как есть, ключ не нужен.
namespace SyncNoteIo
{

QJsonObject serializeNote(KnowTreeModel *model, const QString &tetradir,
                          const QString &id);

QJsonObject serializeBranch(KnowTreeModel *model, const QString &id);

// Возвращают "" при успехе, текст ошибки иначе.
// applyDelete: deletedAt — удаленное время tombstone
QString applyNote(KnowTreeModel *model, const QString &tetradir,
                  const QJsonObject &request);
QString applyBranch(KnowTreeModel *model, const QJsonObject &request);
QString applyDelete(KnowTreeModel *model, const QString &id,
                    qint64 deletedAt);

} // namespace SyncNoteIo

#endif // SYNCNOTEIO_H

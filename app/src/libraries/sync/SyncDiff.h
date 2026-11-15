#ifndef SYNCDIFF_H
#define SYNCDIFF_H

#include <QList>
#include <QString>

// Построчный diff двух текстов для предпросмотра правок синка.
// Чистая логика без Qt-виджетов: тот же алгоритм портируется в Kotlin
// для MyTetroid (единая семантика на обеих платформах).
//
// Алгоритм: общие префикс/суффикс отрезаются, на середине — LCS по
// строкам (О(n*m) по памяти, с потолком: гигантские тексты дают
// грубый diff "все строки удалены + все добавлены").
namespace SyncDiff
{

struct Line
{
    enum Kind { Equal, Removed, Added };

    Kind kind=Equal;
    QString text;
};

// Diff "было" -> "стало": строки с пометками Equal/Removed/Added
QList<Line> diff(const QString &before, const QString &after);

// Человекочитаемая сводка: "+5 -3" (добавлено/удалено строк)
QString summary(const QList<Line> &lines);

} // namespace SyncDiff

#endif // SYNCDIFF_H

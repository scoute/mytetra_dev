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

// Diff "было" -> "стало": строки с пометками Equal/Removed/Added.
// Диффит видимое (без HTML-мусора редакторов): см. visibleLines.
// Картинки — строками "[картинка: имя]"
QList<Line> diff(const QString &before, const QString &after);

// Diff готовых списков строк (после visibleLines)
QList<Line> diffLines(const QStringList &a, const QStringList &b);

// Видимые строки заметки: редакторный мусор (qrichtext-шапки Qt,
// стили, DOCTYPE) выкидывается, картинки — заглушками "[картинка: имя]".
// Остальное — как видит пользователь. 1-в-1 с MyTetroid SyncDiff.kt
QStringList visibleLines(const QString &text);

// Человекочитаемая сводка: "+5 -3" (добавлено/удалено строк)
QString summary(const QList<Line> &lines);

} // namespace SyncDiff

#endif // SYNCDIFF_H

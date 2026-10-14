#ifndef _UNIFIEDDIFF_H_
#define _UNIFIEDDIFF_H_

#include <QString>
#include <QStringList>

// Unified diff через внешний git и его раскраска в HTML.
// Без git возвращается пусто, вызыватель показывает причину

class UnifiedDiff
{
public:

    // Компактное plain-превращение HTML заметки в строки
    static QStringList htmlToLines(const QString &html);

    // Unified diff двух наборов строк через git diff --no-index.
    // Временные пути подменяются названием чтобы patch читался
    static QString diffLines(const QStringList &before,
                             const QStringList &after,
                             const QString &title);

    // Раскраска unified diff в HTML для QTextBrowser.
    // Заголовки серым, добавления зеленым, удаления красным
    static QString toColoredHtml(const QString &diff);
};

#endif /* _UNIFIEDDIFF_H_ */

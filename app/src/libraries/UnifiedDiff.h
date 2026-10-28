#ifndef _UNIFIEDDIFF_H_
#define _UNIFIEDDIFF_H_

#include <QString>
#include <QStringList>

// Unified diff через внешний git и его раскраска в HTML.
// Без git возвращается пусто, вызыватель показывает причину

class UnifiedDiff
{
public:

    // Компактное plain-превращение HTML заметки в строки.
    // Картинки - маркером [image: имя], прочие объекты - [object]
    static QStringList htmlToLines(const QString &html);

    // Unified diff двух наборов строк через git diff --no-index.
    // Временные пути подменяются названием чтобы patch читался
    static QString diffLines(const QStringList &before,
                             const QStringList &after,
                             const QString &title);

    // Раскраска unified diff в HTML для QTextBrowser.
    // Заголовки серым, добавления зеленым, удаления красным.
    // dark=true — палитра под темную тему: черный текст
    // на темном фоне иначе вообще не читается
    static QString toColoredHtml(const QString &diff, bool dark=false);
};

#endif /* _UNIFIEDDIFF_H_ */

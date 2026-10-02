#ifndef _TEXTDIFF_H_
#define _TEXTDIFF_H_

#include <QList>
#include <QString>
#include <QStringList>

// Построчное сравнение текстов заметок (Qt-only, без внешних библиотек).
//
// Используется в диалоге «Что изменилось»: показывает, какие строки текста
// записи изменились у владельца относительно локальной копии подписчика.
// HTML-разметка в сравнение не входит: теги счищаются, картинки-вложения
// показываются строкой-плейсхолдером [изображение: имя]. Содержимое
// бинарных вложений не сравнивается никогда — только списки +/- (см. диалог).

// Одна строка результата сравнения
struct TextDiffLine
{
    enum Kind
    {
        Same,   // Строка без изменений (контекст)
        Del,    // Строка была в старом тексте
        Add     // Строка появилась в новом тексте
    };

    Kind kind=Same;
    QString text;
};

class TextDiff
{
public:
    TextDiff() = delete;

    // HTML заметки -> текстовые строки для сравнения.
    // Блочные элементы и <br/> начинают новую строку, <img> даёт строку
    // [изображение: src] (добавление/удаление картинки видно в diff).
    // Сначала пробуется строгий XML-разбор, при ошибке — regex-фолбэк
    static QStringList htmlToTextLines(const QString &html);

    // Построение списка различий (Myers O(ND)).
    // Пустой список = тексты совпадают. При превышении лимитов
    // (слишком большой diff) возвращается список с одной строкой-заглушкой:
    // проверять через isTruncated()
    static QList<TextDiffLine> diffLines(const QStringList &oldLines,
                                         const QStringList &newLines);

    // Признак усечённого diff (см. diffLines)
    static bool isTruncated(const QList<TextDiffLine> &diff);

    // Рендер diff в HTML для QTextEdit: контекст — серым, удаления — красным,
    // добавления — зелёным, hunks с заголовками номеров строк
    static QString diffToHtml(const QList<TextDiffLine> &diff, int context=3);

    // Удобный метод: HTML diff старого и нового HTML-текстов целиком.
    // Пустая строка = тексты совпадают
    static QString diffHtml(const QString &oldHtml, const QString &newHtml);

private:
    // Лимиты: суммарно строк и максимальная цена diff (защита от OOM:
    // след Myers хранит копию V на каждый D)
    static const int MaxTotalLines=6000;
    static const int MaxDiffCost=2000;

    static QStringList regexToTextLines(const QString &html);
};

#endif // _TEXTDIFF_H_

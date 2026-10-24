#ifndef TYPEFACEFORMATTER_H
#define TYPEFACEFORMATTER_H

#include <QColor>

#include "Formatter.h"
#include "qtextedit.h"

// Класс форматирования для начертания текста

class QDomNode;
class QTextCharFormat;
class QTextDocument;
class QTextFrame;
class QTextTable;


class TypefaceFormatter : public Formatter
{
    Q_OBJECT

public:
    TypefaceFormatter();

    enum EasyFormatType {Bold, Italic, Underline, StrikeOut, SuperScript, SubScript};

    // Язык подсветки блока кода. Порядок совпадает с пунктами меню
    // кнопки кода в EditorToolBar (пункт 0 - старый обычный код,
    // дальше языки по алфавиту)
    enum CodeLanguage {CodePlain=0,
                       CodeAngular, CodeBash, CodeC, CodeCxx, CodeCSharp,
                       CodeGlsl, CodeGo, CodeHtml, CodeJava, CodeJs,
                       CodeJson, CodeLua, CodePython, CodeReact, CodeRust,
                       CodeXml};

signals:

    void updateOutlineButtonHiglight();

    void changeFontselectOnDisplay(QString fontFamily);
    void changeFontsizeOnDisplay(int n);
    void changeFontFamily(QString fontFamily);
    void changeFontPointSize(int n);
    void changeFontcolor(const QColor &color);
    void changeBackgroundcolor(const QColor &color);

public slots:

    // Действия в области редактирования
    void onBoldClicked(void);
    void onItalicClicked(void);
    void onUnderlineClicked(void);
    void onStrikeOutClicked(void); // Зачеркивание текста
    void onSuperScriptClicked(void);
    void onSubScriptClicked(void);
    void onMonospaceClicked(void);
    void onCodeLanguageClicked(int language);
    void onHeading1Clicked(void);
    void onHeading2Clicked(void);
    void onClearClicked(void);
    void onTextOnlyClicked(void);
    void onFixBreakSymbolClicked(void);
    void onTabsToSpacesClicked(void);

    void onFontselectChanged(const QFont &font);
    void onFontsizeChanged(int n);
    void onFontcolorClicked(int n);
    void onBackgroundcolorClicked(int n);
    void onFontcolorApplyLast(void);
    void onBackgroundcolorApplyLast(void);
    void onSpecialCharClicked(void);

    // Вставка одного символа из таблицы спецсимволов
    // (диалог присылает по сигналу, сам остается открытым)
    void insertSpecialChar(const QString &symbol);

    // Вставка горизонтальной линии в "пустой" абзац, где расположен курсор (пустой абзац заменяется на горизонтальную линию)
    void onInsertHorizontalLineClicked();

    void onLowerCase(); // "Строчные"
    void onUpperCase(); // "ПРОПИСНЫЕ"

private:

    // Общий движок заголовков H1/H2: выставить уровень абзацам
    // в выделении (повторный клик снимает в обычный текст)
    void applyHeadingLevel(int level);

    // Форматирование выделения как блока кода с подсветкой выбранного языка
    // (CodePlain - старое поведение без подсветки)
    void formatCodeWithLanguage(int language);

    // Одноразовая подсветка синтаксиса в текущем выделении через
    // хайлайтеры QCodeEditor на временном документе. Цвета запекаются
    // в HTML заметки, поэтому переживают сохранение и не требуют
    // живого QSyntaxHighlighter на документе редактора
    void applyCodeHighlight(int language);

    void doChangeFontcolor(const QColor &selectedColor);
    void doChangeBackgroundColor(const QColor &selectedColor);
    void doChangeFontFamily(QString fontFamily);
    void doChangeFontPointSize(int n);

    void smartFormat(int formatType);
    QString replaceSpacesOnlyTags(QString htmlCode);
    QString clearTypeFace(QString htmlCode);
    QString replaceSpaces(QString htmlCode);
    void recurseReplaceSpaces(const QDomNode &node);
    void replaceReplacementCharacterToSpaceInSelectedText(int startCursorPos, int endCursorPos);
    int removeSpaces(int startCursorPos, int calculateEndCursorPos);
    int replaceSpaceAndParagraphSeparatorToParagraphSeparator(int startCursorPos, int calculateEndCursorPos);
    QString replaceTabs(QString htmlCode);
    void applyStandartFontForSelection();
    void applyPureBlockFormatForSelection();
    void clearColorForSelection();
    void clearBackgroundColorForSelection();
    void removeAnchorDataForSelection();
    QString htmlSimplyfier(QString htmlCode);
    void clearSimple(void);
    void replaceSymbolCase(const QChar::Category &category);

    QList<QTextCursor> getComplexSelectionCursors();

    void clearBackgroundInSelection();
    void clearBackgroundInSimpleSelection(const QTextCursor &cursor);
    static void clearTableCellsBackground(QTextDocument *doc, int selStart, int selEnd);
    static void findAndClearTables(QTextFrame *frame, int selStart, int selEnd);
    static void clearSingleTable(QTextTable *table, int selStart, int selEnd);

    // Обработка мягкого переноса
    void workingSoftCarryInSelection();
};

#endif // TYPEFACEFORMATTER_H

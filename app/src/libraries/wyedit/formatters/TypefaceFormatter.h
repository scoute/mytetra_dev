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
class QSyntaxStyle;


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
                       CodeAngular, CodeBash, CodeC, CodeCMake, CodeCss,
                       CodeCxx, CodeCSharp, CodeGlsl, CodeGo, CodeHtml,
                       CodeIni, CodeJava, CodeJs, CodeJson, CodeLua,
                       CodeMake, CodePhp, CodePython, CodeQml, CodeReact,
                       CodeRust, CodeSql, CodeTypescript, CodeXml, CodeYaml,
                       CodeCount};

    // Адрес ссылки-кнопки [copy] в строке маркера блока кода.
    // Публичный: нужен EditorTextArea для опознания клика по кнопке
    static QString copyCodeHref(void);

    // Курсор (или начало выделения) внутри блока кода с маркером
    // и под маркером есть хоть одна непустая строка кода.
    // Публичный: используется контекстным меню для показа пункта копирования
    bool isCursorInCodeBlock(void);

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

    // Клик по ссылке-кнопке [copy] в строке маркера: код блока ниже
    // маркера уходит в буфер обмена. Позиция - место клика в документе
    void onCopyCodeClicked(int docPosition);

    // Тот же копипаст, но точка берется из текущего курсора
    // (для контекстного меню: начало выделения или позиция)
    void onCopyCodeFromCursor(void);
    // Выбор пункта меню кнопки заголовков: 0-H1, 1-H2, 2-H3,
    // 3-обычный текст. H1-H3 тумблеры (повтор снимает уровень)
    void onHeadingClicked(int index);

    // Enter в заголовке: в конце непустого - новый абзац сразу обычный,
    // на пустом - уровень снимается на месте без новой строки.
    // Выделение строго внутри одного заголовка вырезается как штатно,
    // затем работают те же правила. В середине абзаца, через абзацы
    // и вне заголовков - false, штатный путь
    bool handleHeadingEnter(void);

    // Префикс ссылок-переходов оглавления. Публичный: нужен
    // EditorTextArea для опознания клика по пункту оглавления
    static QString tocGoPrefix(void);

    // Вставка снимка оглавления (H1-H3) в курсор или обновление
    // оглавления под курсором повторным вызовом
    void onTocClicked(void);

    // Клик по пункту оглавления: прыжок в раздел
    void onTocGoClicked(const QString &href);
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

    // Общий движок заголовков H1-H3: выставить уровень абзацам
    // в выделении (повторный клик снимает в обычный текст)
    void applyHeadingLevel(int level);

    // Базовый размер обычного текста: от него масштабируются уровни.
    // Берется размер шрифта документа (в редакторе выставлен), запасной -
    // шрифт приложения. Ноль исключен: дальше на него умножается
    double normalPointSize(void) const;

    // Строка снимка оглавления: уровень, текст, позиция абзаца
    struct TocEntry
    {
        int level;
        QString text;
        int position;
    };

    // Все заголовки H1-H3 документа по порядку. Пустые пропускаются,
    // длинные обрезаются: это снимок для оглавления, а не весь текст
    QList<TocEntry> collectTocEntries(void) const;

    // Абзац - строка оглавления: хоть один фрагмент несет нашу ссылку
    bool blockHasTocLink(const QTextBlock &block) const;

    // Форматирование выделения как блока кода с подсветкой выбранного языка
    // (CodePlain - старое поведение без подсветки)
    void formatCodeWithLanguage(int language);

    // Каноническое имя языка для маркера блока кода. Без перевода:
    // маркер в тексте заметки всегда на английском. Пусто для Plain и JSON
    // (у JSON нет комментариев, маркер вставлять не во что)
    static QString codeMarkerName(int language);

    // Строка-маркер языка для вставки в текст. Пусто - у языка нет маркера
    static QString codeMarkerLine(int language);

    // Распознать наш маркер в строке, вернуть язык или -1.
    // Форматы: // {lang = Имя}, # {lang = Имя}, -- {lang = Имя},
    // ; {lang = Имя}, /* {lang = Имя} */, <!-- {lang = Имя} -->.
    // В хвосте может быть ссылка-кнопка [copy]. Совпадение строгое:
    // вся строка - только маркер с необязательным суффиксом
    static int codeMarkerLanguage(const QString &line);

    // Текст кнопки копирования, добавляемый к строке маркера через пробел.
    // Не переводится: это часть маркера, который всегда на английском,
    // иначе опознание маркера ломается на других языках интерфейса
    static QString copyCodeSuffix(void);

    // Одноразовая подсветка синтаксиса в текущем выделении через
    // хайлайтеры QCodeEditor на временном документе. Цвета запекаются
    // в HTML заметки, поэтому переживают сохранение и не требуют
    // живого QSyntaxHighlighter на документе редактора
    void applyCodeHighlight(int language);

    // Стиль подсветки для языка: отдельная тема в :/styles/<имя>.xml
    // (палитры в духе родных nano-схем, приглушенные под светлый фон),
    // поэтому языки легко отличить с первого взгляда. Кешируется,
    // fallback - стиль по умолчанию
    static QSyntaxStyle *codeStyleForLanguage(int language);

    // Обеспечить маркер языка в начале выделения: заменить наш маркер
    // (в выделении или строкой выше) на новый, вставить новый сверху
    // или убрать маркер для языка без маркера (Plain, JSON).
    // Вызывается внутри edit-блока formatCodeWithLanguage.
    // Возвращает false, если после обработки выделять нечего
    bool normalizeCodeMarker(int language);

    // Залить фоном абзацы текущего выделения (единая плашка блока кода).
    // Вызывается внутри edit-блока formatCodeWithLanguage
    void setCodeBlockBackground(const QBrush &brush);

    // Цвет плашки блока кода. Единый для всех мест, где плашка
    // ставится и где опознается (копирование блока)
    static QColor codeCardColor(void);

    // Собрать текст кода под строкой маркера, начиная с блока,
    // содержащего позицию. Пусто - маркера нет или кода под ним нет
    QString collectCodeBelowMarker(int docPosition) const;

    // Проверка, что абзац выглядит как строка блока кода: либо
    // залит плашкой карточки, либо набран шрифтом кода
    bool isCodeBlock(const QTextBlock &block) const;

    // Оформить суффикс-кнопку [copy] в строке маркера как ссылку.
    // lineStart - начало строки, markerLen - длина текста маркера
    // без суффикса. Вызывается внутри edit-блока
    void applyCopyLinkFormat(int lineStart, int markerLen);

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

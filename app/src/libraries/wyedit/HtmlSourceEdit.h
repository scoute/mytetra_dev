#ifndef HTMLSOURCEEDIT_H
#define HTMLSOURCEEDIT_H

#include <QPlainTextEdit>
#include <QTextDocument>

class QStyleSyntaxHighlighter;

// Правка HTML-исходника заметки и конфигов: моноширинный текст,
// подсветка тегов (QXMLHighlighter, стиль :/styles/html.xml -
// только foreground, темные темы не ломаются), гуттер номеров
// строк, движок поиска/замены с зацикливанием.
// Панель поиска живет в диалоге-владельце, здесь только движок.
// Виджет самодостаточен: AppContext не трогает, поэтому покрывается
// headless-пробой без инициализации контекста
class HtmlSourceEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit HtmlSourceEdit(QWidget *parent=nullptr);
    virtual ~HtmlSourceEdit();

    // Поиск дальше/назад с зацикливанием на границах документа
    bool findNext(const QString &text, QTextDocument::FindFlags flags=QTextDocument::FindFlags());
    bool findPrevious(const QString &text, QTextDocument::FindFlags flags=QTextDocument::FindFlags());

    // Подсветить все вхождения (сверху ограничено: HTML заметок большие).
    // Возвращает число подсвеченных
    int highlightMatches(const QString &text, QTextDocument::FindFlags flags=QTextDocument::FindFlags());
    void clearMatchHighlight(void);

    // Замена текущего вхождения (если курсор не на нем - сначала ищет).
    // Возвращает, была ли замена
    bool replaceCurrent(const QString &findText, const QString &replaceText,
                        QTextDocument::FindFlags flags=QTextDocument::FindFlags());

    // Все вхождения одной правкой отмены. Возвращает число замен.
    // Пустой запрос - всегда 0 (защита от бесконечного цикла)
    int replaceAll(const QString &findText, const QString &replaceText,
                  QTextDocument::FindFlags flags=QTextDocument::FindFlags());

    // Строка 1-based с клампом, курсор в ее начало
    void goToLine(int line);

    // Имя загруженного стиля подсветки (для проб)
    QString syntaxStyleName(void) const;

    // Ширина гуттера в пикселях (для проб)
    int lineNumberAreaWidth(void) const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void updatePairHighlight(void);

private:
    // Полоса номеров строк, красит ее сам редактор
    class LineNumberArea;

    // Тег для подсветки пары: [начало, конец), имя в нижнем регистре
    struct TagSpan
    {
        int start=-1;
        int end=-1;
        QString name;
        bool isClose=false;
        bool selfClose=false;
        bool special=false;
    };

    LineNumberArea *lineNumberArea;
    QStyleSyntaxHighlighter *xmlHighlighter;
    QList<QTextEdit::ExtraSelection> matchSelections;
    QList<QTextEdit::ExtraSelection> pairSelections;

    bool findInternal(const QString &text, QTextDocument::FindFlags flags, bool backward);
    void paintLineNumberArea(QPaintEvent *event);
    void applySelections(void);

    // Тег, содержащий позицию pos (курсор внутри или на его границах).
    // Многострочные теги поддерживаются, незакрытый (при наборе) - нет
    bool tagAt(int pos, TagSpan &tag) const;

    // Тег, заканчивающийся ровно в pos (перед курсором только что
    // закрыли '>' при наборе). Для Enter и автозакрытия
    bool tagEndingAt(int pos, TagSpan &tag) const;

    // Парный тег с учетом вложенности одноименных тегов. Комментарии вида
    // <!-- <div> --> внутри эвристикой отбрасываются (окно 1000 символов).
    // Поиск ограничен сверху, чтобы не висеть на огромных HTML
    bool findMate(const TagSpan &tag, TagSpan &mate) const;

    static bool isVoidElement(const QString &name);
    static QString tagInnerText(QTextDocument *doc, int from, int to);

    void handleSmartEnter(QKeyEvent *event);
    void autoCloseTag(void);

    // Лимит подсветки вхождений: полный HTML заметки с картинками
    // в base64 дает тысячи совпадений на коротком запросе
    static const int maxHighlightMatches=1000;

    // Лимит поиска парного тега (совпадений) и окна проверки комментария.
    // Хватает с запасом для заметок, на гигантских HTML просто
    // не найдет дальнюю пару вместо висения
    static const int maxPairScan=2000;
    static const int commentWindow=1000;
};

#endif // HTMLSOURCEEDIT_H

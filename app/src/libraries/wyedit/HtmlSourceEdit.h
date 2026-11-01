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
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    // Полоса номеров строк, красит ее сам редактор
    class LineNumberArea;

    LineNumberArea *lineNumberArea;
    QStyleSyntaxHighlighter *xmlHighlighter;

    bool findInternal(const QString &text, QTextDocument::FindFlags flags, bool backward);
    void paintLineNumberArea(QPaintEvent *event);

    // Лимит подсветки вхождений: полный HTML заметки с картинками
    // в base64 дает тысячи совпадений на коротком запросе
    static const int maxHighlightMatches=1000;
};

#endif // HTMLSOURCEEDIT_H

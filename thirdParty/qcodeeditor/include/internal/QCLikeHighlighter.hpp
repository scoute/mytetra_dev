#pragma once

// QCodeEditor
#include <QStyleSyntaxHighlighter> // Required for inheritance
#include <QHighlightRule>

// Qt
#include <QRegularExpression>
#include <QVector>
#include <QString>

class QSyntaxStyle;

/**
 * @brief Class, that describes generic C-like language highlighter.
 *
 * MyTetra-side adapter with the exact highlight logic of QCXXHighlighter,
 * but the keyword file is a constructor parameter instead of the hardcoded
 * :/languages/cpp.xml. Used for C, Java, C#, Go, Rust and JavaScript,
 * whose keyword lists live in :/languages/<lang>.xml and share the C
 * comment/string/number syntax.
 */
class QCLikeHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
public:

    /**
     * @brief Constructor.
     * @param languageFile Resource path of the language keyword file,
     *        for example ":/languages/go.xml".
     * @param document Pointer to document.
     */
    explicit QCLikeHighlighter(const QString& languageFile, QTextDocument* document=nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:

    QVector<QHighlightRule> m_highlightRules;

    QRegularExpression m_includePattern;
    QRegularExpression m_functionPattern;
    QRegularExpression m_defTypePattern;

    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;
};
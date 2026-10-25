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
 * @brief Class, that describes PHP highlighter.
 *
 * MyTetra-side adapter: QCLikeHighlighter logic (// and multi-line comments,
 * function calls) plus $variable highlighting and # comments.
 * Keyword list is loaded from :/languages/php.xml through QLanguage.
 */
class QPhpHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
public:

    /**
     * @brief Constructor.
     * @param document Pointer to document.
     */
    explicit QPhpHighlighter(QTextDocument* document=nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:

    QVector<QHighlightRule> m_highlightRules;

    QRegularExpression m_functionPattern;

    QRegularExpression m_variablePattern;
    QRegularExpression m_bracedVariablePattern;

    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;
};
#pragma once

// QCodeEditor
#include <QStyleSyntaxHighlighter> // Required for inheritance
#include <QHighlightRule>

// Qt
#include <QRegularExpression>
#include <QVector>

class QSyntaxStyle;

/**
 * @brief Class, that describes CSS highlighter.
 *
 * MyTetra-side adapter. Property list is loaded from :/languages/css.xml
 * through QLanguage; a property is highlighted only when followed by
 * a colon, so values are never mistaken for properties.
 */
class QCssHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
public:

    /**
     * @brief Constructor.
     * @param document Pointer to document.
     */
    explicit QCssHighlighter(QTextDocument* document=nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:

    QVector<QHighlightRule> m_highlightRules;

    QRegularExpression m_functionPattern;

    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;
};
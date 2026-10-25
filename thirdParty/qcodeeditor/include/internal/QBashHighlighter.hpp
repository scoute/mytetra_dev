#pragma once

// QCodeEditor
#include <QStyleSyntaxHighlighter> // Required for inheritance
#include <QHighlightRule>

// Qt
#include <QRegularExpression>
#include <QVector>

class QSyntaxStyle;

/**
 * @brief Class, that describes Bash script highlighter.
 *
 * MyTetra-side adapter, modeled on QLuaHighlighter/QPythonHighlighter.
 * Keyword list is loaded from :/languages/bash.xml through QLanguage,
 * comment and string rules are hardcoded like in upstream highlighters.
 */
class QBashHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
public:

    /**
     * @brief Constructor.
     * @param document Pointer to document.
     */
    explicit QBashHighlighter(QTextDocument* document=nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:

    QVector<QHighlightRule> m_highlightRules;

    QRegularExpression m_functionPattern;

    QRegularExpression m_variablePattern;
    QRegularExpression m_bracedVariablePattern;
    QRegularExpression m_specialVariablePattern;
};
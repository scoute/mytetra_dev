// MyTetra-side adapter, modeled on QLuaHighlighter/QPythonHighlighter.
#include <QBashHighlighter>
#include <QSyntaxStyle>
#include <QLanguage>

// Qt
#include <QFile>


QBashHighlighter::QBashHighlighter(QTextDocument* document) :
    QStyleSyntaxHighlighter(document),
    m_highlightRules     (),
    m_variablePattern    (QRegularExpression(R"(\$[a-zA-Z_][a-zA-Z0-9_]*)")),
    m_bracedVariablePattern(QRegularExpression(R"(\$\{[^}\n]*\})"))
{
    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile fl(":/languages/bash.xml");

    if (!fl.open(QIODevice::ReadOnly))
    {
        return;
    }

    QLanguage language(&fl);

    if (!language.isLoaded())
    {
        return;
    }

    auto keys = language.keys();
    for (auto&& key : keys)
    {
        auto names = language.names(key);
        for (auto&& name : names)
        {
            m_highlightRules.append({
                QRegularExpression(QString(R"(\b%1\b)").arg(name)),
                key
            });
        }
    }

    // Numbers
    m_highlightRules.append({
        QRegularExpression(R"(\b(0b|0x){0,1}[\d.']+\b)"),
        "Number"
    });

    // Double quoted strings
    m_highlightRules.append({
        QRegularExpression(R"("[^\n"]*")"),
        "String"
    });

    // Single quoted strings
    m_highlightRules.append({
        QRegularExpression(R"('[^\n']*')"),
        "String"
    });

    // Variables win over strings: "$HOME" colors the variable
    m_highlightRules.append({
        m_variablePattern,
        "Global"
    });
    m_highlightRules.append({
        m_bracedVariablePattern,
        "Global"
    });

    // Single line comments have the highest priority
    m_highlightRules.append({
        QRegularExpression(R"(#[^\n]*)"),
        "Comment"
    });
}

void QBashHighlighter::highlightBlock(const QString& text)
{
    for (auto& rule : m_highlightRules)
    {
        auto matchIterator = rule.pattern.globalMatch(text);

        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();

            setFormat(
                match.capturedStart(),
                match.capturedLength(),
                syntaxStyle()->getFormat(rule.formatName)
            );
        }
    }

    setCurrentBlockState(0);
}
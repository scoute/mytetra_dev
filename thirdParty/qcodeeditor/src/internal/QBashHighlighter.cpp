// MyTetra-side adapter, modeled on QLuaHighlighter/QPythonHighlighter.
#include <QBashHighlighter>
#include <QSyntaxStyle>
#include <QLanguage>

// Qt
#include <QFile>


QBashHighlighter::QBashHighlighter(QTextDocument* document) :
    QStyleSyntaxHighlighter(document),
    m_highlightRules     (),
    m_functionPattern    (QRegularExpression(R"(\b([A-Za-z0-9_]+)(?=\())")),
    m_variablePattern    (QRegularExpression(R"(\$[a-zA-Z_][a-zA-Z0-9_]*)")),
    m_bracedVariablePattern(QRegularExpression(R"(\$\{[^}\n]*\})")),
    m_specialVariablePattern(QRegularExpression(R"(\$[-@*#?$!0-9])"))
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

    // Brackets, test operators and command separators.
    // Order inside the alternation is longest-first
    m_highlightRules.append({
        QRegularExpression(R"(\[\[|\]\]|\(\(|\)\)|;;&|;;|;&|\|\||&&|[|&;()<>[\]{}])"),
        "Preprocessor"
    });

    // Command substitution opening and backticks (single chars,
    // like nano punctuation: multiline pairs are out of scope)
    m_highlightRules.append({
        QRegularExpression(R"(\$\()"),
        "Preprocessor"
    });
    m_highlightRules.append({
        QRegularExpression(R"(`)"),
        "Preprocessor"
    });

    // Long and short options: --linux, -euo.
    // \b does not work before a dash (both sides are non-word),
    // so the separator is spelled out explicitly
    m_highlightRules.append({
        QRegularExpression(R"(--[A-Za-z0-9_-]+)"),
        "Global"
    });
    m_highlightRules.append({
        QRegularExpression(R"((?:^|[ \t])-[A-Za-z][A-Za-z0-9]*)"),
        "Global"
    });

    // Variable assignments: VAR=value
    m_highlightRules.append({
        QRegularExpression(R"(\b[a-zA-Z_][a-zA-Z0-9_]*=)"),
        "Field"
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

    // Variables win over strings: "$HOME" colors the variable.
    // Special parameters ($?, $#, $0, $@ and so on) too
    m_highlightRules.append({
        m_variablePattern,
        "Global"
    });
    m_highlightRules.append({
        m_bracedVariablePattern,
        "Global"
    });
    m_highlightRules.append({
        m_specialVariablePattern,
        "Global"
    });

    // Single line comments have the highest priority.
    // Like nano: # starts a comment only at line start or after blank,
    // so $# and ${x#y} are not treated as comments
    m_highlightRules.append({
        QRegularExpression(R"((?:^|[ \t])#[^\n]*)"),
        "Comment"
    });
}

void QBashHighlighter::highlightBlock(const QString& text)
{
    // Checking for function calls and definitions: name(args
    {
        auto matchIterator = m_functionPattern.globalMatch(text);

        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();

            setFormat(
                match.capturedStart(1),
                match.capturedLength(1),
                syntaxStyle()->getFormat("Function")
            );
        }
    }

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
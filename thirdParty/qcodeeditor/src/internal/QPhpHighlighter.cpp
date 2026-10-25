// MyTetra-side adapter: QCLikeHighlighter logic plus $variables and # comments.
#include <QPhpHighlighter>
#include <QSyntaxStyle>
#include <QLanguage>

// Qt
#include <QFile>


QPhpHighlighter::QPhpHighlighter(QTextDocument* document) :
    QStyleSyntaxHighlighter(document),
    m_highlightRules     (),
    m_functionPattern    (QRegularExpression(R"(\b([_a-zA-Z][_a-zA-Z0-9]*)(?=\())")),
    m_variablePattern    (QRegularExpression(R"(\$[a-zA-Z_][a-zA-Z0-9_]*)")),
    m_bracedVariablePattern(QRegularExpression(R"(\$\{[^}\n]*\})")),
    m_commentStartPattern(QRegularExpression(R"(/\*)")),
    m_commentEndPattern  (QRegularExpression(R"(\*/)"))
{
    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile fl(":/languages/php.xml");

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

    // Superglobals and variables win over strings: "$_GET" colors the variable
    m_highlightRules.append({
        m_variablePattern,
        "Global"
    });
    m_highlightRules.append({
        m_bracedVariablePattern,
        "Global"
    });

    // // comments, but not inside strings like "http://...".
    // (The string rule above already painted the string; this keeps
    // the URL inside it intact)
    m_highlightRules.append({
        QRegularExpression(R"((?<![:\"'])//[^\n]*)"),
        "Comment"
    });

    // # comments only at line start or after blank ($# is not a thing in PHP,
    // but ${x#y} brace truncation and a#b inside strings must survive)
    m_highlightRules.append({
        QRegularExpression(R"((?:^|[ \t])#[^\n]*)"),
        "Comment"
    });
}

void QPhpHighlighter::highlightBlock(const QString& text)
{
    // Checking for function calls
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

    int startIndex = 0;
    if (previousBlockState() != 1)
    {
        startIndex = text.indexOf(m_commentStartPattern);
    }

    while (startIndex >= 0)
    {
        auto match = m_commentEndPattern.match(text, startIndex);

        int endIndex = match.capturedStart();
        int commentLength = 0;

        if (endIndex == -1)
        {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else
        {
            commentLength = endIndex - startIndex + match.capturedLength();
        }

        setFormat(
            startIndex,
            commentLength,
            syntaxStyle()->getFormat("Comment")
        );
        startIndex = text.indexOf(m_commentStartPattern, startIndex + commentLength);
    }
}
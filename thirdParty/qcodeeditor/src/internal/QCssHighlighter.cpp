// MyTetra-side adapter for CSS.
#include <QCssHighlighter>
#include <QSyntaxStyle>
#include <QLanguage>

// Qt
#include <QFile>


QCssHighlighter::QCssHighlighter(QTextDocument* document) :
    QStyleSyntaxHighlighter(document),
    m_highlightRules     (),
    m_functionPattern    (QRegularExpression(R"(\b([_a-zA-Z][_a-zA-Z0-9-]*)(?=\())")),
    m_commentStartPattern(QRegularExpression(R"(/\*)")),
    m_commentEndPattern  (QRegularExpression(R"(\*/)"))
{
    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile fl(":/languages/css.xml");

    if (!fl.open(QIODevice::ReadOnly))
    {
        return;
    }

    QLanguage language(&fl);

    if (!language.isLoaded())
    {
        return;
    }

    // Properties are highlighted only when followed by a colon
    // (an optional vendor prefix is tolerated), so values like "red"
    // are never mistaken for properties
    auto propertyNames = language.names("Property");
    for (auto&& name : propertyNames)
    {
        m_highlightRules.append({
            QRegularExpression(QString(R"((?:^|[;{} \t])(?:-[a-z]+-)?\b%1\b\s*:)").arg(
                QRegularExpression::escape(name))),
            "Property"
        });
    }

    auto keys = language.keys();
    for (auto&& key : keys)
    {
        if (key == "Property")
            continue;

        auto names = language.names(key);
        for (auto&& name : names)
        {
            m_highlightRules.append({
                QRegularExpression(QString(R"(\b%1\b)").arg(name)),
                key
            });
        }
    }

    // At-rules: @media, @import and the like
    m_highlightRules.append({
        QRegularExpression(R"(@[a-zA-Z-]+\b)"),
        "Preprocessor"
    });

    // Hex colors: #fff, #ffffff, #ffffffff
    m_highlightRules.append({
        QRegularExpression(R"(#[0-9a-fA-F]{3,8}\b)"),
        "Number"
    });

    // Class and id selectors: .menu, #header
    m_highlightRules.append({
        QRegularExpression(R"([.#][a-zA-Z_][a-zA-Z0-9_-]*)"),
        "Global"
    });

    // Numbers with units are covered by the number rule;
    // unit suffixes (px, em) come from the language file
    m_highlightRules.append({
        QRegularExpression(R"(\b\d[\d.]*)"),
        "Number"
    });

    // Brackets, separators and combinators
    m_highlightRules.append({
        QRegularExpression(R"([{};:,>+~])"),
        "Preprocessor"
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
}

void QCssHighlighter::highlightBlock(const QString& text)
{
    // Checking for function calls: url(, rgb(, var(
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
// MyTetra-side adapter, modeled on QLuaHighlighter/QPythonHighlighter.
#include <QSqlHighlighter>
#include <QSyntaxStyle>
#include <QLanguage>

// Qt
#include <QFile>


QSqlHighlighter::QSqlHighlighter(QTextDocument* document) :
    QStyleSyntaxHighlighter(document),
    m_highlightRules     (),
    m_highlightBlockRules()
{
    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile fl(":/languages/sql.xml");

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

    // Single quoted strings
    m_highlightRules.append({
        QRegularExpression(R"('[^\n']*')"),
        "String"
    });

    // Double quoted identifiers
    m_highlightRules.append({
        QRegularExpression(R"("[^\n"]*")"),
        "String"
    });

    // Single line comments
    m_highlightRules.append({
        QRegularExpression(R"(--[^\n]*)"),
        "Comment"
    });

    // Multi-line comments
    m_highlightBlockRules.append({
         QRegularExpression(R"(/\*)"),
         QRegularExpression(R"(\*/)"),
         "Comment"
     });
}

void QSqlHighlighter::highlightBlock(const QString& text)
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
    int startIndex = 0;
    int highlightRuleId = previousBlockState();
    if (highlightRuleId < 1 || highlightRuleId > m_highlightBlockRules.size()) {
        for(int i = 0; i < m_highlightBlockRules.size(); ++i) {
            startIndex = text.indexOf(m_highlightBlockRules.at(i).startPattern);

            if (startIndex >= 0) {
                highlightRuleId = i + 1;
                break;
            }
        }
    }

    while (startIndex >= 0)
    {
        const auto &blockRules = m_highlightBlockRules.at(highlightRuleId - 1);
        auto match = blockRules.endPattern.match(text, startIndex+1);

        int endIndex = match.capturedStart();
        int matchLength = 0;

        if (endIndex == -1)
        {
            setCurrentBlockState(highlightRuleId);
            matchLength = text.length() - startIndex;
        }
        else
        {
            matchLength = endIndex - startIndex + match.capturedLength();
        }

        setFormat(
            startIndex,
            matchLength,
            syntaxStyle()->getFormat(blockRules.formatName)
        );
        startIndex = text.indexOf(blockRules.startPattern, startIndex + matchLength);
    }
}
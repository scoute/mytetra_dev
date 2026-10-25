// MyTetra-side adapter for INI files, modeled on QPythonHighlighter.
#include <QIniHighlighter>
#include <QSyntaxStyle>
#include <QLanguage>

// Qt
#include <QFile>


QIniHighlighter::QIniHighlighter(QTextDocument* document) :
    QStyleSyntaxHighlighter(document),
    m_highlightRules     (),
    m_sectionPattern     (QRegularExpression(R"(^\[[^\]\n]+\])")),
    m_keyPattern         (QRegularExpression(R"(^[ \t]*[a-zA-Z0-9_.-]+(?=\s*=))"))
{
    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile fl(":/languages/ini.xml");

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
        QRegularExpression(R"(\b\d[\d.]*\b)"),
        "Number"
    });

    // Double quoted strings
    m_highlightRules.append({
        QRegularExpression(R"("[^\n"]*")"),
        "String"
    });

    // Section headers: [section]
    m_highlightRules.append({
        m_sectionPattern,
        "Keyword"
    });

    // Keys: name before =
    m_highlightRules.append({
        m_keyPattern,
        "Field"
    });

    // ; and # comments at line start or after blank
    m_highlightRules.append({
        QRegularExpression(R"((?:^|[ \t])[;#][^\n]*)"),
        "Comment"
    });
}

void QIniHighlighter::highlightBlock(const QString& text)
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